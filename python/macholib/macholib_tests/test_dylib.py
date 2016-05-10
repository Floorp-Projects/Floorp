from macholib import dylib

import sys
if sys.version_info[:2] <= (2,6):
    import unittest2 as unittest
else:
    import unittest

def d(location=None, name=None, shortname=None, version=None, suffix=None):
    return dict(
        location=location,
        name=name,
        shortname=shortname,
        version=version,
        suffix=suffix
    )

class TestDylib (unittest.TestCase):
    def testInvalid(self):
        self.assertTrue(dylib.dylib_info('completely/invalid') is None)
        self.assertTrue(dylib.dylib_info('completely/invalid_debug') is None)

    def testUnversioned(self):
        self.assertEqual(dylib.dylib_info('P/Foo.dylib'),
                d('P', 'Foo.dylib', 'Foo'))
        self.assertEqual(dylib.dylib_info('P/Foo_debug.dylib'),
                d('P', 'Foo_debug.dylib', 'Foo', suffix='debug'))

    def testVersioned(self):
        self.assertEqual(dylib.dylib_info('P/Foo.A.dylib'),
            d('P', 'Foo.A.dylib', 'Foo', 'A'))
        self.assertEqual(dylib.dylib_info('P/Foo_debug.A.dylib'),
            d('P', 'Foo_debug.A.dylib', 'Foo_debug', 'A'))
        self.assertEqual(dylib.dylib_info('P/Foo.A_debug.dylib'),
            d('P', 'Foo.A_debug.dylib', 'Foo', 'A', 'debug'))

if __name__ == "__main__":
    unittest.main()
