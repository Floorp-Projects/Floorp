# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

import yaml

from .types import supported_types
from .errors import LinterNotFound, LinterParseError


class Parser(object):
    """Reads and validates lint configuration files."""
    required_attributes = (
        'name',
        'description',
        'type',
        'payload',
    )

    def __call__(self, path):
        return self.parse(path)

    def _validate(self, linter):
        missing_attrs = []
        for attr in self.required_attributes:
            if attr not in linter:
                missing_attrs.append(attr)

        if missing_attrs:
            raise LinterParseError(linter['path'], "Missing required attribute(s): "
                                                   "{}".format(','.join(missing_attrs)))

        if linter['type'] not in supported_types:
            raise LinterParseError(linter['path'], "Invalid type '{}'".format(linter['type']))

        for attr in ('include', 'exclude'):
            if attr in linter and (not isinstance(linter[attr], list) or
                                   not all(isinstance(a, basestring) for a in linter[attr])):
                raise LinterParseError(linter['path'], "The {} directive must be a "
                                                       "list of strings!".format(attr))

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
            config = yaml.load(fh)

        if not config:
            raise LinterParseError(path, "No lint definitions found!")

        linters = []
        for name, linter in config.iteritems():
            linter['name'] = name
            linter['path'] = path
            self._validate(linter)
            linters.append(linter)
        return linters
