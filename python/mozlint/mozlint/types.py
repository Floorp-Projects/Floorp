# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
from abc import ABCMeta, abstractmethod

from mozpack import path as mozpath
from mozpack.files import FileFinder

from . import result


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
        exclude = lintargs.get('exclude', [])
        exclude.extend(linter.get('exclude', []))

        paths = self._filter(paths, linter.get('include'), exclude)
        if not paths:
            return

        if self.batch:
            return self._lint(paths, linter, **lintargs)

        errors = []
        for p in paths:
            result = self._lint(p, linter, **lintargs)
            if result:
                errors.extend(result)
        return errors

    def _filter(self, paths, include=None, exclude=None):
        if not include and not exclude:
            return paths

        if include:
            include = map(os.path.normpath, include)

        if exclude:
            exclude = map(os.path.normpath, exclude)

        def match(path, patterns):
            return any(mozpath.match(path, pattern) for pattern in patterns)

        filtered = []
        for path in paths:
            if os.path.isfile(path):
                if include and not match(path, include):
                    continue
                elif exclude and match(path, exclude):
                    continue
                filtered.append(path)
            elif os.path.isdir(path):
                finder = FileFinder(path, find_executables=False, ignore=exclude)
                if self.batch:
                    # Batch means the underlying linter will be responsible for finding
                    # matching files in the directory. Return the path as is if there
                    # exists at least one matching file.
                    if any(finder.contains(pattern) for pattern in include):
                        filtered.append(path)
                else:
                    # Convert the directory to a list of matching files.
                    for pattern in include:
                        filtered.extend([os.path.join(path, p)
                                         for p, f in finder.find(pattern)])

        return filtered

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


supported_types = {
    'string': StringType(),
    'regex': RegexType(),
    'external': ExternalType(),
}
"""Mapping of type string to an associated instance."""
