import functools
import logging
import os
import re
import subprocess
import time
from threading import Lock
from typing import Any, List, Optional, Sequence

# mypy: disallow-untyped-defs

__all__ = [
    "strobelight",
    "StrobelightCLIProfilerError",
    "StrobelightCLIFunctionProfiler",
]

logger = logging.getLogger("strobelight_function_profiler")

console_handler = logging.StreamHandler()
formatter = logging.Formatter(
    "%(name)s, line %(lineno)d, %(asctime)s, %(levelname)s: %(message)s"
)
console_handler.setFormatter(formatter)

logger.addHandler(console_handler)
logger.setLevel(logging.INFO)
logger.propagate = False


class StrobelightCLIProfilerError(Exception):
    """
    Raised when an error happens during strobelight profiling
    """


def _pid_namespace_link(pid: Optional[int] = None) -> str:
    PID_NAMESPACE_PATH = "/proc/{}/ns/pid"
    """Returns the link to the process's namespace, example: pid:[4026531836]"""
    pid = pid or os.getpid()
    return os.readlink(PID_NAMESPACE_PATH.format(pid))


def _pid_namespace(pid: Optional[int] = None) -> int:
    """Returns the process's namespace id"""
    pid = pid or os.getpid()
    link = _pid_namespace_link(pid)
    return int(link[link.find("[") + 1 : -1])


def _command_to_string(command: Sequence[str]) -> str:
    return " ".join(command)


class StrobelightCLIFunctionProfiler:
    """
    Note: this is a meta only tool.

    StrobelightCLIFunctionProfiler can be used to profile a python function and
    generate a strobelight link with the results. It works on meta servers but
    does not requries an fbcode target.
    When stop_at_error is false(default), error during profiling does not prevent
    the work function from running.

    Check function_profiler_example.py for an example.
    """

    # This lock is used to make sure only one thread is running the profiler at any point.
    _lock = Lock()

    def __init__(
        self,
        *,
        stop_at_error: bool = False,
        max_profile_duration_sec: int = 60 * 10,
        sample_each: float = 1e7,  # sample each sample_each cycles.
        run_user_name: str = "pytorch-strobelight-ondemand",
        timeout_wait_for_running_sec: int = 60,
        timeout_wait_for_finished_sec: int = 60,
        recorded_env_variables: Optional[List[str]] = None,
        sample_tags: Optional[List[str]] = None,
        stack_max_len: int = 127,
        async_stack_max_len: int = 127,
    ):
        self.stop_at_error = stop_at_error
        self.max_profile_duration_sec = max_profile_duration_sec
        self.sample_each = sample_each
        self.run_user_name = run_user_name
        self.timeout_wait_for_running_sec = timeout_wait_for_running_sec
        self.timeout_wait_for_finished_sec = timeout_wait_for_finished_sec
        # Results of the most recent run.
        # Tracks the strobelight run id of the most recent run
        self.current_run_id: Optional[int] = None
        self.sample_tags = sample_tags

    def _run_async(self) -> None:
        processId = os.getpid()
        namespace = _pid_namespace(processId)
        command = [
            "strobeclient",
            "run",
            "--profiler",
            "pyperf",
            "--event",
            "cycles",
            "--async",
            "--sample-interval",
            f"{int(self.sample_each)}",
            "--duration-ms",
            f"{int(self.max_profile_duration_sec * 1000)}",
            "--pid",
            f"{namespace}:{processId}",
        ]

        if self.sample_tags:
            command.append("--sample-tags")
            command.append(",".join(self.sample_tags))

        logger.debug("running command: %s", _command_to_string(command))
        result = subprocess.run(command, capture_output=True)
        output = result.stderr.decode("utf-8")
        logger.debug(f"output:\n{output}")

        if result.returncode != 0:
            raise StrobelightCLIProfilerError(
                f"failed to start strobelight profiling, error in run_async:{output}"
            )

        match = re.search("INFO Run Id:.*", output)
        if match:
            inner_match = re.search(r"-?\d+$", match.group(0))
            if inner_match:
                self.current_run_id = int(inner_match.group(0))
                return

        raise StrobelightCLIProfilerError(
            "failed to start strobelight profiling, unexpected result format"
        )

    def _wait_for_running(self, counter: int = 0) -> None:
        if counter > 20:
            raise StrobelightCLIProfilerError(
                "wait_for_running called more than 20 times"
            )

        command = ["strobeclient", "getRunStatus", "--run-id", f"{self.current_run_id}"]
        logger.debug("running command: %s", _command_to_string(command))
        result = subprocess.run(command, capture_output=True)
        output = result.stderr.decode("utf-8")
        logger.debug(f"output:\n{output}")

        if result.returncode != 0:
            raise StrobelightCLIProfilerError(
                f"failed to start strobelight profiling, error in wait_for_running:{output}"
            )

        match = re.search("Profile run status: (.*)", output)
        if match:
            current_status = match.group(1)
            if current_status == "RUNNING":
                return
            elif current_status == "PREPARING":
                time.sleep(10)
                self._wait_for_running(counter + 1)
                return
            else:
                raise StrobelightCLIProfilerError(f"unexpected {current_status} phase")

        raise StrobelightCLIProfilerError("unreachable")

    def _stop_run(self) -> None:
        command = ["strobeclient", "stopRun", "--run-id", str(self.current_run_id)]
        logger.debug("running command: %s", _command_to_string(command))
        result = subprocess.run(command, capture_output=True)
        output = result.stderr.decode("utf-8")
        logger.debug(f"output:\n{output}")

        if result.returncode != 0:
            raise StrobelightCLIProfilerError(
                f"failed to stop strobelight profiling, return code is not 0 :{output}"
            )

        match = re.search("INFO ::1:(.*)", output)
        if match:
            current_status = match.group(1)
            if current_status.__contains__("Success!"):
                return
            else:
                raise StrobelightCLIProfilerError(
                    f"failed to stop strobelight profiling, got {current_status} result"
                )

        raise StrobelightCLIProfilerError("unreachable")

    def _get_results(self) -> None:
        command = ["strobeclient", "getRunStatus", "--run-id", str(self.current_run_id)]
        logger.debug("running command: %s", _command_to_string(command))
        result = subprocess.run(command, capture_output=True)
        output = result.stderr.decode("utf-8")
        logger.debug(f"output:\n{output}")

        if result.returncode != 0:
            raise StrobelightCLIProfilerError(
                f"failed to extract profiling results, return code is not 0 : {output}"
            )

        match = re.search("INFO ::1:(.*)", output)
        if match:
            current_status = match.group(1)
            if not current_status.__contains__("Profile run finished with SUCCESS"):
                raise StrobelightCLIProfilerError(
                    f"failed to extract profiling results, unexpected response {output}"
                )

        for item in re.findall(
            r"(Total samples(.*)|GraphProfiler(.*)|Icicle view \(python stack\)(.*))",
            output,
        ):
            logger.info(item[0])

    def _stop_strobelight_no_throw(
        self,
        collect_results: bool,
    ) -> None:
        try:
            # call stop run
            self._stop_run()
            logger.info("strobelight profiling stopped")

            logger.debug("collection stopped")

            if not collect_results:
                return

            self._get_results()
        except Exception as error:
            logger.warning("error during stop_strobelight", exc_info=True)

    # Return true if strobelight started and is running.
    def _start_strobelight(self) -> bool:
        strobelight_started = False
        try:
            self._run_async()
            strobelight_started = True
            logger.info("strobelight run id is: %s", self.current_run_id)
            self._wait_for_running()
            logger.info("strobelight profiling running")
            return True

        except Exception as error:
            logger.warning("error during start_strobelight:", exc_info=True)
            if strobelight_started:
                self._stop_strobelight_no_throw(collect_results=False)
            return False

    def profile(self, work_function: Any, *args: Any, **kwargs: Any) -> Any:
        self.current_run_id = None

        if StrobelightCLIFunctionProfiler._lock.locked():
            if self.stop_at_error:
                raise StrobelightCLIProfilerError("simultaneous runs not supported")

            return work_function(*args, **kwargs)

        with StrobelightCLIFunctionProfiler._lock:
            started = self._start_strobelight()
            if not started:
                if self.stop_at_error:
                    raise StrobelightCLIProfilerError(
                        "failed to start strobelight profiling"
                    )
                return work_function(*args, **kwargs)

            try:
                logger.debug("collection started")
                result = work_function(*args, **kwargs)
                self._stop_strobelight_no_throw(collect_results=True)
                return result
            except Exception as error:
                logger.warning("work function throw exception", exc_info=True)
                self._stop_strobelight_no_throw(collect_results=False)
                raise error


# A function decorator that wraps profile, if no profiler is provided one with
# default args is created. A function can be annotated as:
# @strobelight()
# @strobelight(profiler = StrobelightFunctionProfiler(stop_at_error=True,..))
# @strobelight(stop_at_error=True,...)
def strobelight(
    profiler: Optional[StrobelightCLIFunctionProfiler] = None, **kwargs: Any
) -> Any:
    if not profiler:
        profiler = StrobelightCLIFunctionProfiler(**kwargs)

    def strobelight_inner(work_function: Any) -> Any:
        @functools.wraps(work_function)
        def wrapper_function(*args: Any, **kwargs: Any) -> Any:
            return profiler.profile(work_function, *args, **kwargs)

        return wrapper_function

    return strobelight_inner
