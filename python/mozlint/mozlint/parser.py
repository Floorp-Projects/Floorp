# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import imp
import os
import sys
import uuid

from .types import supported_types
from .errors import LinterNotFound, LinterParseError


class Parser(object):
    """Reads and validates `.lint.py` files."""
    required_attributes = (
        'name',
        'description',
        'type',
        'payload',
    )

    def __call__(self, path):
        return self.parse(path)

    def _load_linter(self, path):
        # Ensure parent module is present otherwise we'll (likely) get
        # an error due to unknown parent.
        parent_module = 'mozlint.linters'
        if parent_module not in sys.modules:
            mod = imp.new_module(parent_module)
            sys.modules[parent_module] = mod

        write_bytecode = sys.dont_write_bytecode
        sys.dont_write_bytecode = True

        module_name = '{}.{}'.format(parent_module, uuid.uuid1().get_hex())
        imp.load_source(module_name, path)

        sys.dont_write_bytecode = write_bytecode

        mod = sys.modules[module_name]

        if not hasattr(mod, 'LINTER'):
            raise LinterParseError(path, "No LINTER definition found!")

        definition = mod.LINTER
        definition['path'] = path
        return definition

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

    def parse(self, path):
        """Read a linter and return its LINTER definition.

        :param path: Path to the linter.
        :returns: Linter definition (dict)
        :raises: LinterNotFound, LinterParseError
        """
        if not os.path.isfile(path):
            raise LinterNotFound(path)

        if not path.endswith('.lint.py'):
            raise LinterParseError(path, "Invalid filename, linters must end with '.lint.py'!")

        linter = self._load_linter(path)
        self._validate(linter)
        return linter
