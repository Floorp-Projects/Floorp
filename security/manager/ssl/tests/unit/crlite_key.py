#!/usr/bin/python

# Given PEM encoded X.509 certificates Issuer and Subscriber,
# outputs the urlsafe base64 encoding of the SHA256 hash of
# the Issuer's SubjectPublicKeyInfo, and the ascii hex encoding
# of the Subscriber's serial number.

import sys
import base64

from cryptography import x509
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives import hashes


def uint_to_serial_bytes(a):
    # Encode the non-negative integer |a| as a DER integer without the leading
    # tag and length prefix. The DER encoding of |a| is the shortest octet
    # string that encodes |a| in big endian two's complement form.
    assert a >= 0

    # Since |a| is non-negative, the shortest bit string that encodes it in
    # big-endian two's complement form has a leading 0 bit. Positive python
    # integers have a `bit_length` method that gives the index of the leading 1
    # bit. The minimal two's complement bit length is one more than this.
    #
    # NB: Python defines |int(0).bit_length() == 0|. The other cases are more
    # intuitive; for integers x and k with x >= 0 and k > 0 with 2**k > x we
    # have |int(2**k + x).bit_length() == k+1|.
    bit_len = 1 + a.bit_length()
    byte_len = (bit_len + 7) // 8
    return a.to_bytes(byte_len, byteorder="big", signed=False)


if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <path to issuer cert> <path to subscriber cert>")
    sys.exit(1)

with open(sys.argv[1], "r") as f:
    issuer = x509.load_pem_x509_certificate(f.read().encode("utf-8"), backend=None)

with open(sys.argv[2], "r") as f:
    subscriber = x509.load_pem_x509_certificate(f.read().encode("utf-8"), backend=None)

assert issuer.subject.public_bytes() == subscriber.issuer.public_bytes()

issuer_spki = issuer.public_key().public_bytes(
    format=serialization.PublicFormat.SubjectPublicKeyInfo,
    encoding=serialization.Encoding.DER,
)
hasher = hashes.Hash(hashes.SHA256(), backend=None)
hasher.update(issuer_spki)
issuer_spki_hash = hasher.finalize()

subscriber_serial = uint_to_serial_bytes(int(subscriber.serial_number))

print(base64.urlsafe_b64encode(issuer_spki_hash).decode("utf-8"))
print(subscriber_serial.hex())
