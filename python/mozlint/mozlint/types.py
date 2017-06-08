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
from .pathutils import filterpaths, findobject


class BaseType(object):
    """Abstract base class for all types of linters."""
    __metaclass__ = ABCMeta
    batch = False

    def __call__(self, paths, config, **lintargs):
        """Run linter defined by `config` against `paths` with `lintargs`.

        :param paths: Paths to lint. Can be a file or directory.
        :param config: Linter config the paths are being linted against.
        :param lintargs: External arguments to the linter not defined in
                         the definition, but passed in by a consumer.
        :returns: A list of :class:`~result.ResultContainer` objects.
        """
        paths = filterpaths(paths, config, **lintargs)
        if not paths:
            return

        if self.batch:
            return self._lint(paths, config, **lintargs)

        errors = []
        try:
            for p in paths:
                result = self._lint(p, config, **lintargs)
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

    def _lint(self, path, config, **lintargs):
        payload = config['payload']

        with open(path, 'r') as fh:
            lines = fh.readlines()

        errors = []
        for i, line in enumerate(lines):
            if self.condition(payload, line):
                errors.append(result.from_config(config, path=path, lineno=i+1))

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

    def _lint(self, files, config, **lintargs):
        func = findobject(config['payload'])
        return func(files, config, **lintargs)


class LintHandler(LogHandler):
    def __init__(self, config):
        self.config = config
        self.results = []

    def lint(self, data):
        self.results.append(result.from_config(self.config, **data))


class StructuredLogType(BaseType):
    batch = True

    def _lint(self, files, config, **lintargs):
        handler = LintHandler(config)
        logger = config.get("logger")
        if logger is None:
            logger = get_default_logger()
        if logger is None:
            logger = structuredlog.StructuredLogger(config["name"])
            commandline.setup_logging(logger, {}, {"mach": sys.stdout})
        logger.add_handler(handler)

        func = findobject(config["payload"])
        try:
            func(files, config, logger, **lintargs)
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
