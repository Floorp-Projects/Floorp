import unittest

import mozunit

from mozbuild.vendor.vendor_rust import VendorRust


class TestLicenses(unittest.TestCase):
    """
    Unit tests for the Rust Vendoring stuff
    """

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def testLicense(self):
        self.assertEqual(VendorRust.runtime_license("", "Apache-2.0"), True)
        self.assertEqual(VendorRust.runtime_license("", "MIT"), True)
        self.assertEqual(VendorRust.runtime_license("", "GPL"), False)
        self.assertEqual(VendorRust.runtime_license("", "MIT /GPL"), True)
        self.assertEqual(VendorRust.runtime_license("", "GPL/ Proprietary"), False)
        self.assertEqual(VendorRust.runtime_license("", "GPL AND MIT"), False)
        self.assertEqual(VendorRust.runtime_license("", "ISC\tAND\tMIT"), False)
        self.assertEqual(VendorRust.runtime_license("", "GPL OR MIT"), True)
        self.assertEqual(VendorRust.runtime_license("", "ALLIGATOR MIT"), False)
        pass


if __name__ == "__main__":
    mozunit.main()
