from sys import path, version_info
from os.path import sep
path.insert(1, path[0]+sep+'cer')
import test_encoder, test_decoder
from pyasn1.error import PyAsn1Error
if version_info[0:2] < (2, 7) or \
   version_info[0:2] in ( (3, 0), (3, 1) ):
    try:
        import unittest2 as unittest
    except ImportError:
        import unittest
else:
    import unittest

suite = unittest.TestSuite()
loader = unittest.TestLoader()
for m in (test_encoder, test_decoder):
    suite.addTest(loader.loadTestsFromModule(m))

def runTests(): unittest.TextTestRunner(verbosity=2).run(suite)

if __name__ == '__main__': runTests()
