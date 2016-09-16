# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import re
import sys
from abc import ABCMeta, abstractmethod

from mozlog import get_default_logger, commandline, structuredlog
from mozlog.reader import LogHandler

from . import result
from .pathutils import filterpaths


class BaseType(object):
    """Abstract base class for all types of linters."""
    __metaclass__ = ABCMeta
    batch = False

    def __call__(self, paths, linter, **lintargs):
        """Run `linter` against `paths` with `lintargs`.

        :param paths: Paths to lint. Can be a file or directory.
        :param linter: Linter definition paths are being linted against.
        :param lintargs: External arguments to the linter not defined in
                         the definition, but passed in by a consumer.
        :returns: A list of :class:`~result.ResultContainer` objects.
        """
        paths = filterpaths(paths, linter, **lintargs)
        if not paths:
            print("{}: no files to lint in specified paths".format(linter['name']))
            return

        if self.batch:
            return self._lint(paths, linter, **lintargs)

        errors = []
        try:
            for p in paths:
                result = self._lint(p, linter, **lintargs)
                if result:
                    errors.extend(result)
        except KeyboardInterrupt:
            pass
        return errors

    @abstractmethod
    def _lint(self, path):
        pass


class LineType(BaseType):
    """Abstract base class for linter types that check each line individually.

    Subclasses of this linter type will read each file and check the provided
    payload against each line one by one.
    """
    __metaclass__ = ABCMeta

    @abstractmethod
    def condition(payload, line):
        pass

    def _lint(self, path, linter, **lintargs):
        payload = linter['payload']

        with open(path, 'r') as fh:
            lines = fh.readlines()

        errors = []
        for i, line in enumerate(lines):
            if self.condition(payload, line):
                errors.append(result.from_linter(linter, path=path, lineno=i+1))

        return errors


class StringType(LineType):
    """Linter type that checks whether a substring is found."""

    def condition(self, payload, line):
        return payload in line


class RegexType(LineType):
    """Linter type that checks whether a regex match is found."""

    def condition(self, payload, line):
        return re.search(payload, line)


class ExternalType(BaseType):
    """Linter type that runs an external function.

    The function is responsible for properly formatting the results
    into a list of :class:`~result.ResultContainer` objects.
    """
    batch = True

    def _lint(self, files, linter, **lintargs):
        payload = linter['payload']
        return payload(files, **lintargs)


class LintHandler(LogHandler):
    def __init__(self, linter):
        self.linter = linter
        self.results = []

    def lint(self, data):
        self.results.append(result.from_linter(self.linter, **data))


class StructuredLogType(BaseType):
    batch = True

    def _lint(self, files, linter, **lintargs):
        payload = linter["payload"]
        handler = LintHandler(linter)
        logger = linter.get("logger")
        if logger is None:
            logger = get_default_logger()
        if logger is None:
            logger = structuredlog.StructuredLogger(linter["name"])
            commandline.setup_logging(logger, {}, {"mach": sys.stdout})
        logger.add_handler(handler)
        try:
            payload(files, logger, **lintargs)
        except KeyboardInterrupt:
            pass
        return handler.results

supported_types = {
    'string': StringType(),
    'regex': RegexType(),
    'external': ExternalType(),
    'structured_log': StructuredLogType()
}
"""Mapping of type string to an associated instance."""
