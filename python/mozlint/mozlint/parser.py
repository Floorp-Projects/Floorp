# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os

import yaml

from .errors import LinterNotFound, LinterParseError
from .types import supported_types


class Parser(object):
    """Reads and validates lint configuration files."""
    required_attributes = (
        'name',
        'description',
        'type',
        'payload',
    )

    def __init__(self, root):
        self.root = root

    def __call__(self, path):
        return self.parse(path)

    def _validate(self, linter):
        relpath = os.path.relpath(linter['path'], self.root)

        missing_attrs = []
        for attr in self.required_attributes:
            if attr not in linter:
                missing_attrs.append(attr)

        if missing_attrs:
            raise LinterParseError(relpath, "Missing required attribute(s): "
                                            "{}".format(','.join(missing_attrs)))

        if linter['type'] not in supported_types:
            raise LinterParseError(relpath, "Invalid type '{}'".format(linter['type']))

        for attr in ('include', 'exclude', 'support-files'):
            if attr not in linter:
                continue

            if not isinstance(linter[attr], list) or \
                    not all(isinstance(a, basestring) for a in linter[attr]):
                raise LinterParseError(relpath, "The {} directive must be a "
                                                "list of strings!".format(attr))
            invalid_paths = set()
            for path in linter[attr]:
                if '*' in path:
                    continue

                abspath = path
                if not os.path.isabs(abspath):
                    abspath = os.path.join(self.root, path)

                if not os.path.exists(abspath):
                    invalid_paths.add('  ' + path)

            if invalid_paths:
                raise LinterParseError(relpath, "The {} directive contains the following "
                                                "paths that don't exist:\n{}".format(
                                                    attr, '\n'.join(sorted(invalid_paths))))

        if 'setup' in linter:
            if linter['setup'].count(':') != 1:
                raise LinterParseError(relpath, "The setup attribute '{!r}' must have the "
                                                "form 'module:object'".format(
                                                    linter['setup']))

        if 'extensions' in linter:
            linter['extensions'] = [e.strip('.') for e in linter['extensions']]

    def parse(self, path):
        """Read a linter and return its LINTER definition.

        :param path: Path to the linter.
        :returns: List of linter definitions ([dict])
        :raises: LinterNotFound, LinterParseError
        """
        if not os.path.isfile(path):
            raise LinterNotFound(path)

        if not path.endswith('.yml'):
            raise LinterParseError(path, "Invalid filename, linters must end with '.yml'!")

        with open(path) as fh:
            config = yaml.safe_load(fh)

        if not config:
            raise LinterParseError(path, "No lint definitions found!")

        linters = []
        for name, linter in config.iteritems():
            linter['name'] = name
            linter['path'] = path
            self._validate(linter)
            linter.setdefault('support-files', []).append(path)
            linters.append(linter)
        return linters
