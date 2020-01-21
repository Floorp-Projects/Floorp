#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import subprocess

from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization
import binascii

script_dir = os.path.dirname(os.path.abspath(__file__))

# Imports a JSON testvector file.
def import_testvector(file):
    """Import a JSON testvector file and return an array of the contained objects."""
    with open(file) as f:
        vectors = json.loads(f.read())
    return vectors

# Convert a test data string to a hex array.
def string_to_hex_array(string):
    """Convert a string of hex chars to a string representing a C-format array of hex bytes."""
    b = bytearray.fromhex(string)
    result = '{' + ', '.join("{:#04x}".format(x) for x in b) + '}'
    return result

# Writes one AES-GCM testvector into C-header format. (Not clang-format conform)
class AESGCM():
    """Class that provides the generator function for a single AES-GCM test case."""

    def format_testcase(self, vector):
        """Format an AES-GCM testcase object. Return a string in C-header format."""
        result = '{{ {},\n'.format(vector['tcId'])
        for key in ['key', 'msg', 'aad', 'iv']:
            result += ' \"{}\",\n'.format(vector[key])
        result += ' \"\",\n'
        result += ' \"{}\",\n'.format(vector['tag'])
        result += ' \"{}\",\n'.format(vector['ct'] + vector['tag'])
        result += ' {},\n'.format(str(vector['result'] == 'invalid').lower())
        result += ' {}}},\n\n'.format(str('ZeroLengthIv' in vector['flags']).lower())

        return result

# Writes one AES-CMAC testvector into C-header format. (Not clang-format conform)
class AESCMAC():
    """Class that provides the generator function for a single AES-CMAC test case."""

    def format_testcase(self, vector):
        """Format an AES-CMAC testcase object. Return a string in C-header format."""
        result = '{{ {},\n'.format(vector['tcId'])
        for key in ['comment', 'key', 'msg', 'tag']:
            result += ' \"{}\",\n'.format(vector[key])
        result += ' {}}},\n\n'.format(str(vector['result'] == 'invalid').lower())

        return result

# Writes one AES-CBC testvector into C-header format. (Not clang-format conform)
class AESCBC():
    """Class that provides the generator function for a single AES-CBC test case."""

    def format_testcase(self, vector):
        """Format an AES-CBC testcase object. Return a string in C-header format."""
        result = '{{ {},\n'.format(vector['tcId'])
        for key in ['key', 'msg', 'iv']:
            result += ' \"{}\",\n'.format(vector[key])
        result += ' \"{}\",\n'.format(vector['ct'])
        result += ' {}}},\n\n'.format(str(vector['result'] == 'valid' and len(vector['flags']) == 0).lower())

        return result

# Writes one ChaChaPoly testvector into C-header format. (Not clang-format conform)
class ChaChaPoly():
    """Class that provides the generator function for a single ChaCha test case."""

    def format_testcase(self, testcase):
        """Format an ChaCha testcase object. Return a string in C-header format."""
        result = '\n// Comment: {}'.format(testcase['comment'])
        result += '\n{{{},\n'.format(testcase['tcId']-1)
        for key in ['msg', 'aad', 'key', 'iv']:
            result += '{},\n'.format(string_to_hex_array(testcase[key]))
        ct = testcase['ct'] + testcase['tag']
        result += '{},\n'.format(string_to_hex_array(ct))
        result += '{},\n'.format(str(testcase['result'] == 'invalid').lower())
        result += '{}}},\n'.format(str(testcase['comment'] == 'invalid nonce size').lower())

        return result

# Writes one Curve25519 testvector into C-header format. (Not clang-format conform)
class Curve25519():
    """Class that provides the generator function for a single curve25519 test case."""

    # Static pkcs8 and skpi wrappers for the raw keys from Wycheproof.
    # The public key section of the pkcs8 wrapper is filled up with 0's, which is
    # not correct, but acceptable for the tests at this moment because
    # validity of the public key is not checked.
    # It's still necessary because of
    # https://searchfox.org/nss/rev/7bc70a3317b800aac07bad83e74b6c79a9ec5bff/lib/pk11wrap/pk11pk12.c#171
    pkcs8WrapperStart = "3067020100301406072a8648ce3d020106092b06010401da470f01044c304a0201010420"
    pkcs8WrapperEnd = "a1230321000000000000000000000000000000000000000000000000000000000000000000"
    spkiWrapper = "3039301406072a8648ce3d020106092b06010401da470f01032100"

    def format_testcase(self, testcase, curve):
        result = '\n// Comment: {}'.format(testcase['comment'])
        result += '\n{{{},\n'.format(testcase['tcId'])
        result += '{},\n'.format(string_to_hex_array(self.pkcs8WrapperStart + testcase['private'] + self.pkcs8WrapperEnd))
        result += '{},\n'.format(string_to_hex_array(self.spkiWrapper + testcase['public']))
        result += '{},\n'.format(string_to_hex_array(testcase['shared']))

        # Flag 'acceptable' cases with secret == 0 as invalid for NSS.
        # Flag 'acceptable' cases with forbidden public key values as invalid for NSS.
        # Flag 'acceptable' cases with small public key (0 or 1) as invalid for NSS.
        valid = testcase['result'] in ['valid', 'acceptable'] \
                and not testcase['shared'] == "0000000000000000000000000000000000000000000000000000000000000000" \
                and not testcase["public"] == "daffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff" \
                and not testcase["public"] == "dbffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff" \
                and not 'Small public key' in testcase['flags']
        # invalidASN is unused in Curve25519 tests, but this way we can use the ECDH struct
        result += '{},\n'.format(str(False).lower())
        result += '{}}},\n'.format(str(valid).lower())

        return result

class P256_ECDH():
    """Class that provides the generator function for a single P256_ECDH test case."""

    def format_testcase(self, testcase, curve):

        result = '\n// Comment: {}'.format(testcase['comment'])
        result += '\n// tcID: {}'.format(testcase['tcId'])
        private_key = ec.derive_private_key(
                int(testcase["private"], 16),
                ec.SECP256R1(),
                default_backend()
            ).private_bytes(
                encoding=serialization.Encoding.DER,
                format=serialization.PrivateFormat.PKCS8,
                encryption_algorithm=serialization.NoEncryption()
            )
        result += '\n{{{},\n'.format(testcase['tcId'])
        result += '{},\n'.format(string_to_hex_array(bytes.hex(private_key)))
        result += '{},\n'.format(string_to_hex_array(testcase['public']))
        result += '{},\n'.format(string_to_hex_array(testcase['shared']))
        invalid_asn = 'InvalidAsn' in testcase['flags']

        # Note: This classifies "Acceptable" tests cases as invalid.
        # As this represents a gray area, manual adjustments may be
        # necessary to match NSS' implementation.
        valid = testcase['result'] == 'valid'

        result += '{},\n'.format(str(invalid_asn).lower())
        result += '{}}},\n'.format(str(valid).lower())

        return result

def generate_vectors_file(params):
    """
    Generate and store a .h-file with test vectors for one test.

    params -- Dictionary with parameters for test vector generation for the desired test.
    """

    cases = import_testvector(os.path.join(script_dir, params['source_dir'] + params['source_file']))

    base_vectors = ""
    if 'base' in params:
        with open(os.path.join(script_dir, params['base'])) as base:
            base_vectors = base.read()
        base_vectors += "\n\n"

    header = standard_params['license']
    header += "\n"
    header += standard_params['top_comment']
    header += "\n"
    header += "#ifndef " + params['section'] + "\n"
    header += "#define " + params['section'] + "\n"
    header += "\n"

    for include in standard_params['includes']:
        header += "#include " + include + "\n"
    
    header += "\n"

    if 'includes' in params:
        for include in params['includes']:
            header += "#include " + include + "\n"

        header += "\n"

    vectors_file = header + base_vectors + params['array_init']

    for group in cases['testGroups']:
        for test in group['tests']:
            if 'key' in group:
                if 'curve' in group['key'] and group['key']['curve'] not in ['secp256r1']:
                    continue
                vectors_file += params['formatter'].format_testcase(test, group['keyDer'], getSha(group['sha']), group['key']['keySize'])
            elif 'curve' in group:
                if group['curve'] not in ['secp256r1', 'curve25519']:
                    continue
                if(group['curve'] == 'secp256r1'):
                    curve = ec.SECP256R1()
                else:
                    curve = "curve25519"
                vectors_file += params['formatter'].format_testcase(test, curve)
            else:
                vectors_file += params['formatter'].format_testcase(test)

    vectors_file = vectors_file[:params['crop_size_end']] + '\n};\n\n'
    vectors_file += "#endif // " + params['section'] + '\n'

    with open(os.path.join(script_dir, params['target']), 'w') as target:
        target.write(vectors_file)


standard_params = {
    'includes': ['"testvectors_base/test-structs.h"'],
    'license':
"""/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
 """,

    'top_comment':
"""/* This file is generated from sources in nss/gtests/common/wycheproof
 * automatically and should not be touched manually.
 * Generation is trigged by calling python3 genTestVectors.py */
 """
}

# Parameters that describe the generation of a testvector file for each supoorted test.
# source -- relative path to the wycheproof JSON source file with testvectors.
# base -- relative path to non-wycheproof vectors.
# target -- relative path to where the finished .h-file is written.
# array_init -- string to initialize the c-header style array of testvectors.
# formatter -- the test case formatter class to be used for this test.
# crop_size_end -- number of characters removed from the end of the last generated test vector to close the array definition.
# section -- name of the section
# comment -- additional comments to add to the file just before definition of the test vector array.

aes_gcm_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'aes_gcm_test.json',
    'base': '../testvectors_base/gcm-vectors_base.h',
    'target': '../testvectors/gcm-vectors.h',
    'array_init': 'const AesGcmKatValue kGcmWycheproofVectors[] = {\n',
    'formatter' : AESGCM(),
    'crop_size_end': -3,
    'section': 'gcm_vectors_h__',
    'comment' : ''
}

aes_cmac_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'aes_cmac_test.json',
    'target': '../testvectors/cmac-vectors.h',
    'array_init': 'const AesCmacTestVector kCmacWycheproofVectors[] = {\n',
    'formatter' : AESCMAC(),
    'crop_size_end': -3,
    'section': 'cmac_vectors_h__',
    'comment' : ''
}

aes_cbc_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'aes_cbc_pkcs5_test.json',
    'target': '../testvectors/cbc-vectors.h',
    'array_init': 'const AesCbcTestVector kCbcWycheproofVectors[] = {\n',
    'formatter' : AESCBC(),
    'crop_size_end': -3,
    'section': 'cbc_vectors_h__',
    'comment' : ''
}

chacha_poly_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'chacha20_poly1305_test.json',
    'base': '../testvectors_base/chachapoly-vectors_base.h',
    'target': '../testvectors/chachapoly-vectors.h',
    'array_init': 'const ChaChaTestVector kChaCha20WycheproofVectors[] = {\n',
    'formatter' : ChaChaPoly(),
    'crop_size_end': -2,
    'section': 'chachapoly_vectors_h__',
    'comment' : ''
}

curve25519_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'x25519_test.json',
    'base': '../testvectors_base/curve25519-vectors_base.h',
    'target': '../testvectors/curve25519-vectors.h',
    'array_init': 'const EcdhTestVectorStr kCurve25519WycheproofVectors[] = {\n',
    'formatter' : Curve25519(),
    'crop_size_end': -2,
    'section': 'curve25519_vectors_h__',
    'comment' : '// The public key section of the pkcs8 wrapped private key is\n\
    // filled up with 0\'s, which is not correct, but acceptable for the\n\
    // tests at this moment because validity of the public key is not checked.\n'
}

p256ecdh_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'ecdh_secp256r1_test.json',
    'target': '../testvectors/p256ecdh-vectors.h',
    'array_init': 'const EcdhTestVectorStr kP256EcdhWycheproofVectors[] = {\n',
    'formatter' : P256_ECDH(),
    'crop_size_end': -2,
    'section': 'p256ecdh_vectors_h__',
    'comment' : ''
}

def update_tests(tests):

    remote = "https://raw.githubusercontent.com/google/wycheproof/master/testvectors/"
    for test in tests:
        subprocess.check_call(['wget', remote+test['source_file'], '-O',
                               'gtests/common/wycheproof/source_vectors/' +test['source_file']])

def generate_test_vectors():
    """Generate C-header files for all supported tests."""
    all_tests = [aes_cbc_params,
                 aes_cmac_params,
                 aes_gcm_params,
                 chacha_poly_params,
                 curve25519_params,
                 p256ecdh_params]
    update_tests(all_tests)
    for test in all_tests:
        generate_vectors_file(test)

def main():
    generate_test_vectors()

if __name__ == '__main__':
    main()
