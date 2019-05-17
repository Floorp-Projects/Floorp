import unittest

from StringIO import StringIO
import os
import sys
import os.path
import mozunit

from mozbuild.preprocessor import Preprocessor

from mozbuild.vendor_rust import VendorRust

class TestLicenses(unittest.TestCase):
  """
  Unit tests for the Rust Vendoring stuff
  """

  def setUp(self):
    pass

  def tearDown(self):
    pass

  def testLicense(self):
    self.assertEquals(VendorRust.runtime_license('', 'Apache-2.0'), True)
    self.assertEquals(VendorRust.runtime_license('', 'MIT'), True)
    self.assertEquals(VendorRust.runtime_license('', 'GPL'), False)
    self.assertEquals(VendorRust.runtime_license('', 'MIT /GPL'), True)
    self.assertEquals(VendorRust.runtime_license('', 'GPL/ Proprietary'), False)
    self.assertEquals(VendorRust.runtime_license('', 'GPL AND MIT'), False)
    self.assertEquals(VendorRust.runtime_license('', 'ISC\tAND\tMIT'), False)
    self.assertEquals(VendorRust.runtime_license('', 'GPL OR MIT'), True)
    self.assertEquals(VendorRust.runtime_license('', 'ALLIGATOR MIT'), False)
    pass

if __name__ == '__main__':
  mozunit.main()
