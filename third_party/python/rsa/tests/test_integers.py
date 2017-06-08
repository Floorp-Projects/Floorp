'''Tests integer operations.'''

import unittest2

import rsa.core

class IntegerTest(unittest2.TestCase):

    def setUp(self):
        (self.pub, self.priv) = rsa.newkeys(64)

    def test_enc_dec(self):

        message = 42
        print("\tMessage:   %d" % message)

        encrypted = rsa.core.encrypt_int(message, self.pub.e, self.pub.n)
        print("\tEncrypted: %d" % encrypted)

        decrypted = rsa.core.decrypt_int(encrypted, self.priv.d, self.pub.n)
        print("\tDecrypted: %d" % decrypted)

        self.assertEqual(message, decrypted)

    def test_sign_verify(self):

        message = 42

        signed = rsa.core.encrypt_int(message,self.priv.d, self.pub.n)
        print("\tSigned:    %d" % signed)

        verified = rsa.core.decrypt_int(signed, self.pub.e,self.pub.n)
        print("\tVerified:  %d" % verified)

        self.assertEqual(message, verified)

