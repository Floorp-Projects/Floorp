# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozlint import result
from mozlint.errors import LintException


def badreturncode(files, config, **lintargs):
    return 1


def external(files, config, **lintargs):
    results = []
    for path in files:
        with open(path, 'r') as fh:
            for i, line in enumerate(fh.readlines()):
                if 'foobar' in line:
                    results.append(result.from_config(
                        config, path=path, lineno=i+1, column=1, rule="no-foobar"))
    return results


def raises(files, config, **lintargs):
    raise LintException("Oh no something bad happened!")


def structured(files, config, logger, **kwargs):
    for path in files:
        with open(path, 'r') as fh:
            for i, line in enumerate(fh.readlines()):
                if 'foobar' in line:
                    logger.lint_error(path=path,
                                      lineno=i+1,
                                      column=1,
                                      rule="no-foobar")
