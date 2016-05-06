# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os


class LintException(Exception):
    pass


class LinterNotFound(LintException):
    def __init__(self, path):
        LintException.__init__(self, "Could not find lint file '{}'".format(path))


class LinterParseError(LintException):
    def __init__(self, path, message):
        LintException.__init__(self, "{}: {}".format(os.path.basename(path), message))


class LintersNotConfigured(LintException):
    def __init__(self):
        LintException.__init__(self, "No linters registered! Use `LintRoller.read` "
                                     "to register a linter.")
