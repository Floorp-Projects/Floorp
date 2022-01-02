#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This utility is used by the build system to create test inputs for the
signed tree head decoding and verification implementation. The format is
generally lines of <key>:<value> pairs except for the to-be-signed
section, which consists of one or more lines of hex bytes. Comments may
appear at the end of lines and begin with '//'.
The following keys are valid:
signingKey: A pykey key identifier to use to sign the to-be-signed data.
            Required.
spki: A pykey key identifier to create an encoded SubjectPublicKeyInfo
      to be included with the test data. The tests will use this spki to
      validate the signature. Required.
prefix: Hex bytes to include at the beginning of the signed tree head
        data. This data is not covered by the signature (typically this
        is used for the log_id field). Optional. Defaults to the empty
        string.
hash: The name of a hash algorithm to use when signing. Optional.
      Defaults to 'sha256'.
"""

from pyasn1.codec.der import encoder
import binascii

import os
import sys

sys.path.append(
    os.path.join(os.path.dirname(__file__), "..", "..", "..", "manager", "tools")
)
import pykey


def sign(signingKey, hashAlgorithm, hexToSign):
    """Given a pykey, the name of a hash function, and hex bytes to
    sign, signs the data (as binary) and returns a hex string consisting
    of the signature."""
    # key.sign returns a hex string in the format "'<hex bytes>'H",
    # so we have to strip off the "'"s and trailing 'H'
    return signingKey.sign(binascii.unhexlify(hexToSign), "hash:%s" % hashAlgorithm)[
        1:-2
    ]


class Error(Exception):
    """Base class for exceptions in this module."""

    pass


class UnknownParameterTypeError(Error):
    """Base class for handling unexpected input in this module."""

    def __init__(self, value):
        super(Error, self).__init__()
        self.value = value
        self.category = "key"

    def __str__(self):
        return 'Unknown %s type "%s"' % (self.category, repr(self.value))


class InputTooLongError(Error):
    """Helper exception type for inputs that are too long."""

    def __init__(self, length):
        super(InputTooLongError, self).__init__()
        self.length = length

    def __str__(self):
        return "Input too long: %s > 65535" % self.length


def getTwoByteLenAsHex(callLenOnMe):
    """Given something that len can be called on, returns a hex string
    representing the two-byte length of the something, in network byte
    order (the length must be less than or equal to 65535)."""
    length = len(callLenOnMe)
    if length > 65535:
        raise InputTooLongError(length)
    return bytes([length // 256, length % 256]).hex()


def createSTH(configStream):
    """Given a stream that will provide the specification for a signed
    tree head (see the comment at the top of this file), creates the
    corresponding signed tree head. Returns a string that can be
    compiled as C/C++ that declares two const char*s kSTHHex and
    kSPKIHex corresponding to the hex encoding of the signed tree head
    and the hex encoding of the subject public key info from the
    specification, respectively."""
    toSign = ""
    prefix = ""
    hashAlgorithm = "sha256"
    for line in configStream.readlines():
        if ":" in line:
            param = line.split(":")[0]
            arg = line.split(":")[1].split("//")[0].strip()
            if param == "signingKey":
                signingKey = pykey.keyFromSpecification(arg)
            elif param == "spki":
                spki = pykey.keyFromSpecification(arg)
            elif param == "prefix":
                prefix = arg
            elif param == "hash":
                hashAlgorithm = arg
            else:
                raise UnknownParameterTypeError(param)
        else:
            toSign = toSign + line.split("//")[0].strip()
    signature = sign(signingKey, hashAlgorithm, toSign)
    lengthBytesHex = getTwoByteLenAsHex(binascii.unhexlify(signature))
    sth = prefix + toSign + lengthBytesHex + signature
    spkiHex = encoder.encode(spki.asSubjectPublicKeyInfo()).hex()
    return 'const char* kSTHHex = "%s";\nconst char* kSPKIHex = "%s";\n' % (
        sth,
        spkiHex,
    )


def main(output, inputPath):
    """Given a file-like output and the path to a signed tree head
    specification (see the comment at the top of this file), reads the
    specification, creates the signed tree head, and outputs test data
    that can be included by a gtest corresponding to the
    specification."""
    with open(inputPath) as configStream:
        output.write(createSTH(configStream))
