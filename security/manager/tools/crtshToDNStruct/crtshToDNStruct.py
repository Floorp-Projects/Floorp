#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This utility takes a series of https://crt.sh/ identifiers and writes to
stdout all of those certs' distinguished name fields in hex, with an array
of all those named "RootDNs". You'll need to post-process this list to rename
"RootDNs" and handle any duplicates.

Requires Python 3.
"""
import re
import requests
import sys
import io

from pyasn1.codec.der import decoder
from pyasn1.codec.der import encoder
from pyasn1_modules import pem
from pyasn1_modules import rfc5280

from cryptography import x509
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import hashes
from cryptography.x509.oid import NameOID

assert sys.version_info >= (3, 2), "Requires Python 3.2 or later"

def hex_string_for_struct(bytes):
    return ["0x{:02X}".format(x) for x in bytes]

def hex_string_human_readable(bytes):
    return ["{:02X}".format(x) for x in bytes]

def nameOIDtoString(oid):
    if oid == NameOID.COUNTRY_NAME:
        return "C"
    if oid == NameOID.COMMON_NAME:
        return "CN"
    if oid == NameOID.LOCALITY_NAME:
        return "L"
    if oid == NameOID.ORGANIZATION_NAME:
        return "O"
    if oid == NameOID.ORGANIZATIONAL_UNIT_NAME:
        return "OU"
    raise Exception("Unknown OID: {}".format(oid))

def print_block(pemData):
    substrate = pem.readPemFromFile(io.StringIO(pemData.decode("utf-8")))
    cert, rest = decoder.decode(substrate, asn1Spec=rfc5280.Certificate())
    der_subject = encoder.encode(cert['tbsCertificate']['subject'])
    octets = hex_string_for_struct(der_subject)

    cert = x509.load_pem_x509_certificate(pemData, default_backend())
    common_name = cert.subject.get_attributes_for_oid(NameOID.COMMON_NAME)[0]
    block_name = "CA{}DN".format(re.sub(r'[-:=_. ]', '', common_name.value))

    fingerprint = hex_string_human_readable(cert.fingerprint(hashes.SHA256()))

    dn_parts = ["/{id}={value}".format(id=nameOIDtoString(part.oid),
                                       value=part.value) for part in cert.subject]
    distinguished_name = "".join(dn_parts)

    print("// {dn}".format(dn=distinguished_name))
    print("// SHA256 Fingerprint: " + ":".join(fingerprint[:16]))
    print("//                     " + ":".join(fingerprint[16:]))
    print("// https://crt.sh/?id={crtsh} (crt.sh ID={crtsh})"
          .format(crtsh=crtshId))
    print("static const uint8_t {}[{}] = ".format(block_name, len(octets)) + "{")

    while len(octets) > 0:
        print("  " + ", ".join(octets[:13]) + ",")
        octets = octets[13:]

    print("};")
    print()

    return block_name


if __name__ == "__main__":
    blocks = []

    certshIds = sys.argv[1:]
    print("// Script from security/manager/tools/crtshToDNStruct/crtshToDNStruct.py")
    print("// Invocation: {} {}".format(sys.argv[0], " ".join(certshIds)))
    print()
    for crtshId in certshIds:
        r = requests.get('https://crt.sh/?d={}'.format(crtshId))
        r.raise_for_status()

        pemData = r.content
        blocks.append(print_block(pemData))

    print("static const DataAndLength RootDNs[]= {")
    for structName in blocks:
        print("  { " + "{},".format(structName))
        print("    sizeof({})".format(structName) + " },")
    print("};")
