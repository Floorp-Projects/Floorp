from macholib import mach_o

import sys
if sys.version_info[:2] <= (2,6):
    import unittest2 as unittest
else:
    import unittest


class TestMachO (unittest.TestCase):
    # This module is just a set of struct definitions,
    # not sure how to test those without replicating
    # the code.
    #
    # The definitions will get exercised by the
    # other tests, therefore testing is ignored
    # for now.
    pass

if __name__ == "__main__":
    unittest.main()
