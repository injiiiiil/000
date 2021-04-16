'''
This coverage plug-in attempts to cover JIT'd functions and methods that were previously missed in code coverage. Any
function and method that was passed through/decorated with torch.jit.script or torch.jit.script_method should now be
marked covered when coverage is run with this plug-in.
'''

from coverage import CoveragePlugin, CoverageData
from inspect import ismodule, isclass, ismethod, isfunction, iscode, getsourcefile, getsourcelines

# All coverage stats resulting from this plug-in will be in a separate .coverage file that should be merged later with
# `coverage combine`. The convention seems to be .coverage.dotted.suffix based on the following link:
# https://coverage.readthedocs.io/en/coverage-5.5/cmd.html#combining-data-files-coverage-combine
cov_data = CoverageData(basename='.coverage.jit')


def is_not_builtin_class(obj):
    return isclass(obj) and not type(obj).__module__ == 'builtins'


class JitPlugin(CoveragePlugin):
    '''
    dynamic_context is an overridden function that gives us access to every frame run during the coverage process. We
    look for when the function being run is `should_drop`, as all functions that get passed into `should_drop` will be
    compiled and thus should be marked as covered.
    '''
    def dynamic_context(self, frame):
        if frame.f_code.co_name == 'should_drop':
            obj = frame.f_locals['fn']
            if is_not_builtin_class(obj) or ismodule(obj) or ismethod(obj) or isfunction(obj) or iscode(obj):
                filename = getsourcefile(obj)
                sourcelines, starting_lineno = getsourcelines(obj)
                line_data = {filename: range(starting_lineno, starting_lineno + len(sourcelines))}
                cov_data.add_lines(line_data)
        super().dynamic_context(frame)

def coverage_init(reg, options):
    reg.add_dynamic_context(JitPlugin())
