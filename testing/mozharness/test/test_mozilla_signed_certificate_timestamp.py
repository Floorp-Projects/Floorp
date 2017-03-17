import unittest
import struct
from mozharness.mozilla.signed_certificate_timestamp import SignedCertificateTimestamp

log_id = 'pLkJkLQYWBSHuxOizGdwCjw1mAT5G9+443fNDsgN3BA='.decode('base64')
timestamp = 1483206164907
signature = 'BAMARzBFAiEAsyJov/LF1DIxurR+6xkxP/ZJzb3whHQ+1+PrJNuXfnoCIG28p1XRxkQqRprnCIDDBniKbJngig/NQnIEQ5VZOYG+'.decode('base64')

json_sct = {
    'sct_version': 0,
    'id': log_id.encode('base64'),
    'timestamp': timestamp,
    'signature': signature.encode('base64'),
}

hex_timestamp = struct.pack('!Q', timestamp).encode('hex')
hex_sct = '00' + log_id.encode('hex') + hex_timestamp + '0000' + signature.encode('hex')
binary_sct = hex_sct.decode('hex')

class TestSignedCertificateTimestamp(unittest.TestCase):
    def testEncode(self):
        sct = SignedCertificateTimestamp(json_sct)
        self.assertEquals(sct.to_rfc6962(), binary_sct)

    def testDecode(self):
        sct = SignedCertificateTimestamp.from_rfc6962(binary_sct)

        self.assertEquals(sct.version, 0)
        self.assertEquals(sct.id, log_id)
        self.assertEquals(sct.timestamp, timestamp)
        self.assertEquals(sct.signature, signature)
