# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def lint(files, logger, **kwargs):
    for path in files:
        with open(path, 'r') as fh:
            for i, line in enumerate(fh.readlines()):
                if 'foobar' in line:
                    logger.lint_error(path=path,
                                      lineno=i+1,
                                      column=1,
                                      rule="no-foobar")


LINTER = {
    'name': "StructuredLinter",
    'description': "It's bad to have the string foobar in js files.",
    'include': [
        'files',
    ],
    'type': 'structured_log',
    'extensions': ['.js', '.jsm'],
    'payload': lint,
}
