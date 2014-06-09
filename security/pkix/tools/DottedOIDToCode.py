# This code is made available to you under your choice of the following sets
# of licensing terms:
###############################################################################
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
###############################################################################
# Copyright 2013 Mozilla Contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import print_function
import sys

def base128Stringified(value):
    """
    Encodes the given integral value into a string that is an encoded comma-
    separated series of bytes, base-128, with all but the last byte having
    the high bit set, in C++ hex notation, as required by the DER rules for the
    nodes of an OID after the first two.

    >>> base128Stringified(1)
    '0x01'
    >>> base128Stringified(10045)
    '0xce, 0x3d'
    """
    if value < 0:
        raise ValueError("An OID must have only positive-value nodes.")

    format = "0x%.2x"

    # least significant byte has highest bit unset
    result = format % (value % 0x80)
    value /= 0x80

    while value != 0:
        # other bytes have highest bit set
        result = (format % (0x80 | (value % 0x80))) + ", " + result
        value /= 0x80

    return result
    
def dottedOIDToCEncoding(dottedOID):
    """
    Takes a dotted OID string (e.g. '1.2.840.10045.4.3.4') as input, and
    returns a string that contains the hex encoding of the OID in C++ literal
    notation, e.g. '0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04'. Note that
    the ASN.1 tag and length are *not* included in the result.
    """
    nodes = [int(x) for x in dottedOID.strip().split(".")]
    if len(nodes) < 2:
        raise ValueError("An OID must have at least two nodes.")
    if not (0 <= nodes[0] <= 2):
        raise ValueError("The first node of an OID must be 0, 1, or 2.")
    if not (0 <= nodes[1] <= 39):
        # XXX: Does this restriction apply when the first part is 2?
        raise ValueError("The second node of an OID must be 0-39.")
    firstByte = (40 * nodes[0]) + nodes[1]
    allStringified = [base128Stringified(x) for x in [firstByte] + nodes[2:]]
    return ", ".join(allStringified)

def specNameToCName(specName):
    """
    Given an string containing an ASN.1 name, returns a string that is a valid
    C++ identifier that is as similar to that name as possible. Since most
    ASN.1 identifiers used in PKIX specifications are legal C++ names except
    for containing hyphens, this function just converts the hyphens to
    underscores. This may need to be improved in the future if we encounter
    names with other funny characters.
    """
    return specName.replace("-", "_")

def toCode(programName, specName, dottedOID):
    """
    Given an ASN.1 name and a string containing the dotted representation of an
    OID, returns a string that contains a C++ declaration for a named constant
    that contains that OID value. Note that the ASN.1 tag and length are *not*
    included in the result.

    This:
        toCode("DottedOIDToCode.py", "ecdsa-with-SHA512", "1.2.840.10045.4.3.4")
    would result in a string like:
      // python DottedOIDToCode.py ecdsa-with-SHA512 1.2.840.10045.4.3.4
      static const uint8_t ecdsa_with_SHA512[] = {
        0x2a, 0x86,0x48, 0xce,0x3d, 0x04, 0x03, 0x04
      };
    """
    return ("  // python %s %s %s\n" +
            "  static const uint8_t %s[] = {\n" +
            "    %s\n" +
            "  };\n") % (programName, specName, dottedOID,
                         specNameToCName(specName),
                         dottedOIDToCEncoding(dottedOID))

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("usage:   python %s <name> <dotted-oid>" % sys.argv[0],
              file=sys.stderr)
        print("example: python %s ecdsa-with-SHA1 1.2.840.10045.4.1" %
                  sys.argv[0], file=sys.stderr)
        sys.exit(1)

    print(toCode(sys.argv[0], sys.argv[1], sys.argv[2]))
