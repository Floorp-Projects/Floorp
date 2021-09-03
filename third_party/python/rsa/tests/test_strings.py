'''Tests string operations.'''

from __future__ import absolute_import

import unittest2

import rsa

from constants import unicode_string

class StringTest(unittest2.TestCase):

    def setUp(self):
        (self.pub, self.priv) = rsa.newkeys(384)

    def test_enc_dec(self):

        message = unicode_string.encode('utf-8')
        print("\tMessage:   %s" % message)

        encrypted = rsa.encrypt(message, self.pub)
        print("\tEncrypted: %s" % encrypted)

        decrypted = rsa.decrypt(encrypted, self.priv)
        print("\tDecrypted: %s" % decrypted)

        self.assertEqual(message, decrypted)

