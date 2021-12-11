# Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
# For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

"""Implementations of unittest features from the future."""

import unittest


def unittest_has(method):
    """Does `unittest.TestCase` have `method` defined?"""
    return hasattr(unittest.TestCase, method)


class TestCase(unittest.TestCase):
    """Just like unittest.TestCase, but with assert methods added.

    Designed to be compatible with 3.1 unittest.  Methods are only defined if
    `unittest` doesn't have them.

    """
    # pylint: disable=arguments-differ, deprecated-method

    if not unittest_has('assertCountEqual'):
        def assertCountEqual(self, *args, **kwargs):
            return self.assertItemsEqual(*args, **kwargs)

    if not unittest_has('assertRaisesRegex'):
        def assertRaisesRegex(self, *args, **kwargs):
            return self.assertRaisesRegexp(*args, **kwargs)

    if not unittest_has('assertRegex'):
        def assertRegex(self, *args, **kwargs):
            return self.assertRegexpMatches(*args, **kwargs)
