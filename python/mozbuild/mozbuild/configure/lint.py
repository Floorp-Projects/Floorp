# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from StringIO import StringIO
from . import (
    ConfigureError,
    ConfigureSandbox,
    DependsFunction,
)
from mozbuild.util import memoize


class LintSandbox(ConfigureSandbox):
    def __init__(self, environ=None, argv=None, stdout=None, stderr=None):
        out = StringIO()
        stdout = stdout or out
        stderr = stderr or out
        environ = environ or {}
        argv = argv or []
        super(LintSandbox, self).__init__({}, environ=environ, argv=argv,
                                          stdout=stdout, stderr=stderr)

    def run(self, path=None):
        if path:
            self.include_file(path)

    @memoize
    def _value_for_depends(self, obj, need_help_dependency=False):
        with_help = self._help_option in obj.dependencies
        if with_help:
            for arg in obj.dependencies:
                if isinstance(arg, DependsFunction):
                    if self._help_option not in arg.dependencies:
                        raise ConfigureError(
                            "`%s` depends on '--help' and `%s`. "
                            "`%s` must depend on '--help'"
                            % (obj.name, arg.name, arg.name))
        elif self._help or need_help_dependency:
            raise ConfigureError("Missing @depends for `%s`: '--help'" %
                                 obj.name)
        return super(LintSandbox, self)._value_for_depends(
            obj, need_help_dependency)
