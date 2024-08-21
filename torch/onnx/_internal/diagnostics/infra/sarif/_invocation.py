# DO NOT EDIT! This file was generated by jschema_to_python version 0.0.1.dev29,
# with extension for dataclasses and type annotation.

from __future__ import annotations

import dataclasses
from typing import Any, List, Optional

from torch.onnx._internal.diagnostics.infra.sarif import (
    _artifact_location,
    _configuration_override,
    _notification,
    _property_bag,
)


@dataclasses.dataclass
class Invocation(object):
    """The runtime environment of the analysis tool run."""

    execution_successful: bool = dataclasses.field(
        metadata={"schema_property_name": "executionSuccessful"}
    )
    account: Optional[str] = dataclasses.field(
        default=None, metadata={"schema_property_name": "account"}
    )
    arguments: Optional[List[str]] = dataclasses.field(
        default=None, metadata={"schema_property_name": "arguments"}
    )
    command_line: Optional[str] = dataclasses.field(
        default=None, metadata={"schema_property_name": "commandLine"}
    )
    end_time_utc: Optional[str] = dataclasses.field(
        default=None, metadata={"schema_property_name": "endTimeUtc"}
    )
    environment_variables: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "environmentVariables"}
    )
    executable_location: Optional[_artifact_location.ArtifactLocation] = (
        dataclasses.field(
            default=None, metadata={"schema_property_name": "executableLocation"}
        )
    )
    exit_code: Optional[int] = dataclasses.field(
        default=None, metadata={"schema_property_name": "exitCode"}
    )
    exit_code_description: Optional[str] = dataclasses.field(
        default=None, metadata={"schema_property_name": "exitCodeDescription"}
    )
    exit_signal_name: Optional[str] = dataclasses.field(
        default=None, metadata={"schema_property_name": "exitSignalName"}
    )
    exit_signal_number: Optional[int] = dataclasses.field(
        default=None, metadata={"schema_property_name": "exitSignalNumber"}
    )
    machine: Optional[str] = dataclasses.field(
        default=None, metadata={"schema_property_name": "machine"}
    )
    notification_configuration_overrides: Optional[
        List[_configuration_override.ConfigurationOverride]
    ] = dataclasses.field(
        default=None,
        metadata={"schema_property_name": "notificationConfigurationOverrides"},
    )
    process_id: Optional[int] = dataclasses.field(
        default=None, metadata={"schema_property_name": "processId"}
    )
    process_start_failure_message: Optional[str] = dataclasses.field(
        default=None, metadata={"schema_property_name": "processStartFailureMessage"}
    )
    properties: Optional[_property_bag.PropertyBag] = dataclasses.field(
        default=None, metadata={"schema_property_name": "properties"}
    )
    response_files: Optional[List[_artifact_location.ArtifactLocation]] = (
        dataclasses.field(
            default=None, metadata={"schema_property_name": "responseFiles"}
        )
    )
    rule_configuration_overrides: Optional[
        List[_configuration_override.ConfigurationOverride]
    ] = dataclasses.field(
        default=None, metadata={"schema_property_name": "ruleConfigurationOverrides"}
    )
    start_time_utc: Optional[str] = dataclasses.field(
        default=None, metadata={"schema_property_name": "startTimeUtc"}
    )
    stderr: Optional[_artifact_location.ArtifactLocation] = dataclasses.field(
        default=None, metadata={"schema_property_name": "stderr"}
    )
    stdin: Optional[_artifact_location.ArtifactLocation] = dataclasses.field(
        default=None, metadata={"schema_property_name": "stdin"}
    )
    stdout: Optional[_artifact_location.ArtifactLocation] = dataclasses.field(
        default=None, metadata={"schema_property_name": "stdout"}
    )
    stdout_stderr: Optional[_artifact_location.ArtifactLocation] = dataclasses.field(
        default=None, metadata={"schema_property_name": "stdoutStderr"}
    )
    tool_configuration_notifications: Optional[List[_notification.Notification]] = (
        dataclasses.field(
            default=None,
            metadata={"schema_property_name": "toolConfigurationNotifications"},
        )
    )
    tool_execution_notifications: Optional[List[_notification.Notification]] = (
        dataclasses.field(
            default=None,
            metadata={"schema_property_name": "toolExecutionNotifications"},
        )
    )
    working_directory: Optional[_artifact_location.ArtifactLocation] = (
        dataclasses.field(
            default=None, metadata={"schema_property_name": "workingDirectory"}
        )
    )


# flake8: noqa
