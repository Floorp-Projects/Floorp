# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals


class Checker(object):
    '''Abstract class to implement checks per file type.
    '''
    pattern = None
    # if a check uses all reference entities, set this to True
    needs_reference = False

    @classmethod
    def use(cls, file):
        return cls.pattern.match(file.file)

    def __init__(self, extra_tests, locale=None):
        self.extra_tests = extra_tests
        self.locale = locale
        self.reference = None

    def check(self, refEnt, l10nEnt):
        '''Given the reference and localized Entities, performs checks.

        This is a generator yielding tuples of
        - "warning" or "error", depending on what should be reported,
        - tuple of line, column info for the error within the string
        - description string to be shown in the report
        '''
        if True:
            raise NotImplementedError("Need to subclass")
        yield ("error", (0, 0), "This is an example error", "example")

    def set_reference(self, reference):
        '''Set the reference entities.
        Only do this if self.needs_reference is True.
        '''
        self.reference = reference
