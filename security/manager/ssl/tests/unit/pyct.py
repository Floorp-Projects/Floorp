#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Helper library for creating a Signed Certificate Timestamp given the
details of a signing key, when to sign, and the certificate data to
sign. Currently only supports precert_entry types. See RFC 6962.
"""

from pyasn1.codec.der import encoder
from struct import pack
import binascii
import calendar
import hashlib

import pykey


class InvalidKeyError(Exception):
    """Helper exception to handle unknown key types."""

    def __init__(self, key):
        self.key = key

    def __str__(self):
        return 'Invalid key: "%s"' % str(self.key)


class SCT(object):
    """SCT represents a Signed Certificate Timestamp."""

    def __init__(self, key, date, tbsCertificate, issuerKey):
        self.key = key
        self.timestamp = calendar.timegm(date.timetuple()) * 1000
        self.tbsCertificate = tbsCertificate
        self.issuerKey = issuerKey

    def signAndEncode(self):
        """Returns a signed and encoded representation of the SCT as a
        string."""
        # The signature is over the following data:
        # sct_version (one 0 byte)
        # signature_type (one 0 byte)
        # timestamp (8 bytes, milliseconds since the epoch)
        # entry_type (two bytes [0, 1] - currently only precert_entry is
        #             supported)
        # signed_entry (bytes of PreCert)
        # extensions (2-byte-length-prefixed, currently empty (so two 0
        #             bytes))
        # A PreCert is:
        # issuer_key_hash (32 bytes of SHA-256 hash of the issuing
        #                  public key, as DER-encoded SPKI)
        # tbs_certificate (3-byte-length-prefixed data)
        timestamp = pack("!Q", self.timestamp)
        hasher = hashlib.sha256()
        hasher.update(encoder.encode(self.issuerKey.asSubjectPublicKeyInfo()))
        issuer_key_hash = hasher.digest()
        len_prefix = pack("!L", len(self.tbsCertificate))[1:]
        data = (
            b"\0\0"
            + timestamp
            + b"\0\1"
            + issuer_key_hash
            + len_prefix
            + self.tbsCertificate
            + b"\0\0"
        )
        if isinstance(self.key, pykey.ECCKey):
            signatureByte = b"\3"
        elif isinstance(self.key, pykey.RSAKey):
            signatureByte = b"\1"
        else:
            raise InvalidKeyError(self.key)
        # sign returns a hex string like "'<hex bytes>'H", but we want
        # bytes here
        hexSignature = self.key.sign(data, pykey.HASH_SHA256)
        signature = binascii.unhexlify(hexSignature[1:-2])
        # The actual data returned is the following:
        # sct_version (one 0 byte)
        # id (32 bytes of SHA-256 hash of the signing key, as
        #     DER-encoded SPKI)
        # timestamp (8 bytes, milliseconds since the epoch)
        # extensions (2-byte-length-prefixed data, currently
        #             empty)
        # hash (one 4 byte representing sha256)
        # signature (one byte - 1 for RSA and 3 for ECDSA)
        # signature (2-byte-length-prefixed data)
        hasher = hashlib.sha256()
        hasher.update(encoder.encode(self.key.asSubjectPublicKeyInfo()))
        key_id = hasher.digest()
        signature_len_prefix = pack("!H", len(signature))
        return (
            b"\0"
            + key_id
            + timestamp
            + b"\0\0\4"
            + signatureByte
            + signature_len_prefix
            + signature
        )
