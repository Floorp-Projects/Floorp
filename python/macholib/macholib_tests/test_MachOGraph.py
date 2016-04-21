from macholib import MachOGraph

import sys
if sys.version_info[:2] <= (2,6):
    import unittest2 as unittest
else:
    import unittest

class TestMachOGraph (unittest.TestCase):
    @unittest.expectedFailure
    def test_missing(self):
        self.fail("tests are missing")

if __name__ == "__main__":
    unittest.main()
