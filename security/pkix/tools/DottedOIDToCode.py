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
import argparse
import itertools
import sys

def base128(value):
    """
    Given an integral value, returns an array of the base-128 representation
    of that value, with all but the last byte having the high bit set as
    required by the DER rules for the nodes of an OID after the first two
    bytes.

    >>> base128(1)
    [1]
    >>> base128(10045)
    [206, 61]
    """

    if value < 0:
        raise ValueError("An OID must have only positive-value nodes.")

    # least significant byte has highest bit unset
    result = [value % 0x80]
    value /= 0x80

    while value != 0:
        result = [0x80 | (value % 0x80)] + result
        value /= 0x80

    return result

def dottedOIDToEncodedArray(dottedOID):
    """
    Takes a dotted OID string (e.g. '1.2.840.10045.4.3.4') as input, and
    returns an array that contains the DER encoding of its value, without
    the tag and length (e.g. [0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04]).
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
    restBase128 = [base128(x) for x in nodes[2:]]
    return [firstByte] + list(itertools.chain.from_iterable(restBase128))

def dottedOIDToCArray(dottedOID, mode):
    """
    Takes a dotted OID string (e.g. '1.2.840.10045.4.3.4') as input, and
    returns a string that contains the hex encoding of the OID in C++ literal
    notation, e.g. '0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04'.
    """
    bytes = dottedOIDToEncodedArray(dottedOID)

    if mode != "value":
        bytes = [0x06, len(bytes)] + bytes

    if mode == "alg":
        # Wrap the DER-encoded OID in a SEQUENCE to create an
        # AlgorithmIdentifier with no parameters.
        bytes = [0x30, len(bytes)] + bytes

    return ", ".join(["0x%.2x" % b for b in bytes])

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

def toCode(programName, specName, dottedOID, mode):
    """
    Given an ASN.1 name and a string containing the dotted representation of an
    OID, returns a string that contains a C++ declaration for a named constant
    that contains that OID value. If mode is "value" then only the value of
    the OID (without the tag or length) will be included in the output. If mode
    is "tlv" then the value will be prefixed with the tag and length. If mode
    is "alg" then the value will be a complete der-encoded AlgorithmIdentifier
    with no parameters.

    This:

        toCode("DottedOIDToCode.py", "ecdsa-with-SHA512", "1.2.840.10045.4.3.4",
               "value")

    would result in a string like:

        // python DottedOIDToCode.py ecdsa-with-SHA512 1.2.840.10045.4.3.4
        static const uint8_t ecdsa_with_SHA512[] = {
          0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04
        };

    This:

        toCode("DottedOIDToCode.py", "ecdsa-with-SHA512", "1.2.840.10045.4.3.4",
               "tlv")

    would result in a string like:

        // python DottedOIDToCode.py --tlv ecdsa-with-SHA512 1.2.840.10045.4.3.4
        static const uint8_t tlv_ecdsa_with_SHA512[] = {
          0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04
        };

    This:

        toCode("DottedOIDToCode.py", "ecdsa-with-SHA512", "1.2.840.10045.4.3.4",
               "alg")

    would result in a string like:

        // python DottedOIDToCode.py --alg ecdsa-with-SHA512 1.2.840.10045.4.3.4
        static const uint8_t alg_ecdsa_with_SHA512[] = {
          0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04
        };
    """
    programNameWithOptions = programName
    varName = specNameToCName(specName)
    if mode == "tlv":
        programNameWithOptions += " --tlv"
        varName = "tlv_" + varName
    elif mode == "alg":
        programNameWithOptions += " --alg"
        varName = "alg_" + varName

    return ("  // python %s %s %s\n" +
            "  static const uint8_t %s[] = {\n" +
            "    %s\n" +
            "  };\n") % (programNameWithOptions, specName, dottedOID, varName,
                         dottedOIDToCArray(dottedOID, mode))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
                 description="Generate code snippets to handle OIDs in C++",
                 epilog="example: python %s ecdsa-with-SHA1 1.2.840.10045.4.1"
                            % sys.argv[0])
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--tlv", action='store_true',
                       help="Wrap the encoded OID value with the tag and length")
    group.add_argument("--alg", action='store_true',
                       help="Wrap the encoded OID value in an encoded SignatureAlgorithm")
    parser.add_argument("name",
                        help="The name given to the OID in the specification")
    parser.add_argument("dottedOID", metavar="dotted-oid",
                        help="The OID value, in dotted notation")

    args = parser.parse_args()
    if args.alg:
        mode = 'alg'
    elif args.tlv:
        mode = 'tlv'
    else:
        mode = 'value'

    print(toCode(sys.argv[0], args.name, args.dottedOID, mode))
