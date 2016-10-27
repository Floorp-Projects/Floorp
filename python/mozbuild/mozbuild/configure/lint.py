# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from StringIO import StringIO
from . import ConfigureSandbox


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
