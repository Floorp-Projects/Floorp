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

class ECDH():
    """Class that provides the generator function for a single ECDH test case."""

    def format_testcase(self, testcase, curve):

        result = '\n// Comment: {}'.format(testcase['comment'])
        result += '\n// tcID: {}'.format(testcase['tcId'])
        private_key = ec.derive_private_key(
                int(testcase["private"], 16),
                curve,
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

class DSA():
    pub_keys = {}
    def format_testcase(self, testcase, key, hash_oid, keySize, out_defs):
        key_name = "kPubKey"
        if key in self.pub_keys:
            key_name = self.pub_keys[key]
        else:
            key_name += str(len(self.pub_keys))
            self.pub_keys[key] = key_name
            out_defs.append('static const std::vector<uint8_t> ' + key_name + string_to_hex_array(key) + ';\n\n')
        result = '\n// Comment: {}'.format(testcase['comment'])
        result += '\n// tcID: {}\n'.format(testcase['tcId'])
        result += '{{{}, {},\n'.format(hash_oid, testcase['tcId'])
        result += '{},\n'.format(string_to_hex_array(testcase['sig']))
        result += '{},\n'.format(key_name)
        result += '{},\n'.format(string_to_hex_array(testcase['msg']))
        valid = testcase['result'] == 'valid' or (testcase['result'] == 'acceptable' and 'NoLeadingZero' in testcase['flags'])
        result += '{}}},\n'.format(str(valid).lower())

        return result

class ECDSA():
    """Class that provides the generator function for a single ECDSA test case."""

    def format_testcase(self, testcase, key, hash_oid, keySize, out_defs):
        result = '\n// Comment: {}'.format(testcase['comment'])
        result += '\n// tcID: {}\n'.format(testcase['tcId'])
        result += '{{{}, {},\n'.format(hash_oid, testcase['tcId'])
        result += '{},\n'.format(string_to_hex_array(testcase['sig']))
        result += '{},\n'.format(string_to_hex_array(key))
        result += '{},\n'.format(string_to_hex_array(testcase['msg']))
        valid = testcase['result'] == 'valid'
        if not valid and testcase['result'] == 'acceptable':
            valid = 'MissingZero' in testcase['flags']
        result += '{}}},\n'.format(str(valid).lower())

        return result

class HKDF():
    """Class that provides the generator function for a single HKDF test case."""

    def format_testcase(self, vector):
        """Format an HKDF testcase object. Return a string in C-header format."""
        result = '{{ {},\n'.format(vector['tcId'])
        for key in ['ikm', 'salt', 'info', "okm"]:
            result += ' \"{}\",\n'.format(vector[key])
        result += ' {},\n'.format(vector['size'])
        result += ' {}}},\n\n'.format(str(vector['result'] == 'valid').lower())

        return result

class RSA_PKCS1_SIGNATURE():
    pub_keys = {}

    def format_testcase(self, testcase, key, keysize, hash_oid, out_defs):
        # To avoid hundreds of copies of the same key, define it once and reuse.
        key_name = "pub_key_"
        if key in self.pub_keys:
            key_name = self.pub_keys[key]
        else:
            key_name += str(len(self.pub_keys))
            self.pub_keys[key] = key_name
            out_defs.append('static const std::vector<uint8_t> ' + key_name + string_to_hex_array(key) + ';\n\n')

        result = '\n// Comment: {}'.format(testcase['comment'])
        result += '\n// tcID: {}\n'.format(testcase['tcId'])
        result += '{{{}, {}, \n'.format(hash_oid, testcase['tcId'])
        result += '{},\n'.format(string_to_hex_array(testcase['sig']))
        result += '{},\n'.format(key_name)
        result += '{},\n'.format(string_to_hex_array(testcase['msg']))

        valid = testcase['result'] == 'valid'
        if not valid and testcase['result'] == 'acceptable':
            valid = keysize >= 1024 and ('SmallModulus' in testcase['flags'] or
                                         'SmallPublicKey' in testcase['flags'])
        result += '{}}},\n'.format(str(valid).lower())

        return result


class RSA_PKCS1_DECRYPT():
    priv_keys = {}

    def format_testcase(self, testcase, priv_key, key_size, out_defs):
        key_name = "priv_key_"
        if priv_key in self.priv_keys:
            key_name = self.priv_keys[priv_key]
        else:
            key_name += str(len(self.priv_keys))
            self.priv_keys[priv_key] = key_name
            out_defs.append('static const std::vector<uint8_t> ' + key_name + string_to_hex_array(priv_key) + ';\n\n')

        result = '\n// Comment: {}'.format(testcase['comment'])
        result += '\n// tcID: {}'.format(testcase['tcId'])
        result += '\n{{{},\n'.format(testcase['tcId'])
        result += '{},\n'.format(string_to_hex_array(testcase['msg']))
        result += '{},\n'.format(string_to_hex_array(testcase['ct']))
        result += '{},\n'.format(key_name)
        valid = testcase['result'] == 'valid'
        result += '{}}},\n'.format(str(valid).lower())

        return result

class RSA_PSS():
    pub_keys = {}

    def format_testcase(self, testcase, key, hash_oid, mgf_hash, sLen, out_defs):
        key_name = "pub_key_"
        if key in self.pub_keys:
            key_name = self.pub_keys[key]
        else:
            key_name += str(len(self.pub_keys))
            self.pub_keys[key] = key_name
            out_defs.append('static const std::vector<uint8_t> ' + key_name + string_to_hex_array(key) + ';\n\n')

        result = '\n// Comment: {}'.format(testcase['comment'])
        result += '\n// tcID: {}\n'.format(testcase['tcId'])
        result += '{{{}, {}, {}, {},\n'.format(hash_oid, mgf_hash, testcase['tcId'], sLen)
        result += '{},\n'.format(string_to_hex_array(testcase['sig']))
        result += '{},\n'.format(key_name)
        result += '{},\n'.format(string_to_hex_array(testcase['msg']))

        valid = testcase['result'] == 'valid'
        if not valid and testcase['result'] == 'acceptable':
            valid = ('WeakHash' in testcase['flags'])
        result += '{}}},\n'.format(str(valid).lower())

        return result

class RSA_OAEP():
    priv_keys = {}

    def format_testcase(self, testcase, key, hash_oid, mgf_hash, out_defs):
        key_name = "priv_key_"
        if key in self.priv_keys:
            key_name = self.priv_keys[key]
        else:
            key_name += str(len(self.priv_keys))
            self.priv_keys[key] = key_name
            out_defs.append('static const std::vector<uint8_t> ' + key_name + string_to_hex_array(key) + ';\n\n')

        result = '\n// Comment: {}'.format(testcase['comment'])
        result += '\n// tcID: {}\n'.format(testcase['tcId'])
        result += '{{{}, {}, {},\n'.format(hash_oid, mgf_hash, testcase['tcId'])
        result += '{},\n'.format(string_to_hex_array(testcase['msg']))
        result += '{},\n'.format(string_to_hex_array(testcase['ct']))
        result += '{},\n'.format(string_to_hex_array(testcase['label']))
        result += '{},\n'.format(key_name)

        valid = testcase['result'] == 'valid'
        result += '{}}},\n'.format(str(valid).lower())

        return result

def getSha(sha):
    s = sha.split("-")
    return "SEC_OID_SHA" + s[1]

def getMgfSha(sha):
    s = sha.split("-")
    return "CKG_MGF1_SHA" + s[1]

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

    shared_defs = []
    vectors_file = base_vectors + params['array_init']

    for group in cases['testGroups']:
        for test in group['tests']:
            if 'key' in group:
                if 'curve' in group['key'] and group['key']['curve'] not in ['secp256r1', 'secp384r1', 'secp521r1']:
                    continue
                vectors_file += params['formatter'].format_testcase(test, group['keyDer'], getSha(group['sha']), group['key']['keySize'], shared_defs)
            elif 'type' in group and group['type'] == 'RsassaPssVerify':
                sLen = group['sLen'] if 'sLen' in group else 0
                vectors_file += params['formatter'].format_testcase(test, group['keyDer'], getSha(group['sha']), getMgfSha(group['mgfSha']), sLen, shared_defs)
            elif 'type' in group and group['type'] == 'RsaesOaepDecrypt':
                vectors_file += params['formatter'].format_testcase(test, group['privateKeyPkcs8'], getSha(group['sha']), getMgfSha(group['mgfSha']), shared_defs)
            elif 'keyDer' in group:
                vectors_file += params['formatter'].format_testcase(test, group['keyDer'], group['keysize'], getSha(group['sha']), shared_defs)
            elif 'privateKeyPkcs8' in group:
                vectors_file += params['formatter'].format_testcase(test, group['privateKeyPkcs8'], group['keysize'], shared_defs)
            elif 'curve' in group:
                if group['curve'] == 'secp256r1':
                    curve = ec.SECP256R1()
                elif group['curve'] == 'secp384r1':
                    curve = ec.SECP384R1()
                elif group['curve'] == 'secp521r1':
                    curve = ec.SECP521R1()
                elif group['curve'] == 'curve25519':
                    curve = "curve25519"
                else:
                    continue
                vectors_file += params['formatter'].format_testcase(test, curve)
            else:
                vectors_file += params['formatter'].format_testcase(test)

    vectors_file = vectors_file[:params['crop_size_end']] + '\n};\n\n'
    vectors_file += "#endif // " + params['section'] + '\n'

    with open(os.path.join(script_dir, params['target']), 'w') as target:
        target.write(header)
        for definition in shared_defs:
            target.write(definition)
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
    'array_init': 'const EcdhTestVector kCurve25519WycheproofVectors[] = {\n',
    'formatter' : Curve25519(),
    'crop_size_end': -2,
    'section': 'curve25519_vectors_h__',
    'comment' : '// The public key section of the pkcs8 wrapped private key is\n\
    // filled up with 0\'s, which is not correct, but acceptable for the\n\
    // tests at this moment because validity of the public key is not checked.\n'
}

dsa_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'dsa_test.json',
    'target': '../testvectors/dsa-vectors.h',
    'array_init': 'const DsaTestVector kDsaWycheproofVectors[] = {\n',
    'formatter' : DSA(),
    'crop_size_end': -2,
    'section': 'dsa_vectors_h__',
    'comment' : ''
}

hkdf_sha1_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'hkdf_sha1_test.json',
    'target': '../testvectors/hkdf-sha1-vectors.h',
    'array_init': 'const HkdfTestVector kHkdfSha1WycheproofVectors[] = {\n',
    'formatter' : HKDF(),
    'crop_size_end': -3,
    'section': 'hkdf_sha1_vectors_h__',
    'comment' : ''
}

hkdf_sha256_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'hkdf_sha256_test.json',
    'target': '../testvectors/hkdf-sha256-vectors.h',
    'array_init': 'const HkdfTestVector kHkdfSha256WycheproofVectors[] = {\n',
    'formatter' : HKDF(),
    'crop_size_end': -3,
    'section': 'hkdf_sha256_vectors_h__',
    'comment' : ''
}

hkdf_sha384_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'hkdf_sha384_test.json',
    'target': '../testvectors/hkdf-sha384-vectors.h',
    'array_init': 'const HkdfTestVector kHkdfSha384WycheproofVectors[] = {\n',
    'formatter' : HKDF(),
    'crop_size_end': -3,
    'section': 'hkdf_sha384_vectors_h__',
    'comment' : ''
}

hkdf_sha512_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'hkdf_sha512_test.json',
    'target': '../testvectors/hkdf-sha512-vectors.h',
    'array_init': 'const HkdfTestVector kHkdfSha512WycheproofVectors[] = {\n',
    'formatter' : HKDF(),
    'crop_size_end': -3,
    'section': 'hkdf_sha512_vectors_h__',
    'comment' : ''
}

p256ecdh_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'ecdh_secp256r1_test.json',
    'target': '../testvectors/p256ecdh-vectors.h',
    'array_init': 'const EcdhTestVector kP256EcdhWycheproofVectors[] = {\n',
    'formatter' : ECDH(),
    'crop_size_end': -2,
    'section': 'p256ecdh_vectors_h__',
    'comment' : ''
}

p384ecdh_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'ecdh_secp384r1_test.json',
    'target': '../testvectors/p384ecdh-vectors.h',
    'array_init': 'const EcdhTestVector kP384EcdhWycheproofVectors[] = {\n',
    'formatter' : ECDH(),
    'crop_size_end': -2,
    'section': 'p384ecdh_vectors_h__',
    'comment' : ''
}

p521ecdh_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'ecdh_secp521r1_test.json',
    'target': '../testvectors/p521ecdh-vectors.h',
    'array_init': 'const EcdhTestVector kP521EcdhWycheproofVectors[] = {\n',
    'formatter' : ECDH(),
    'crop_size_end': -2,
    'section': 'p521ecdh_vectors_h__',
    'comment' : ''
}

p256ecdsa_sha256_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'ecdsa_secp256r1_sha256_test.json',
    'target': '../testvectors/p256ecdsa-sha256-vectors.h',
    'array_init': 'const EcdsaTestVector kP256EcdsaSha256Vectors[] = {\n',
    'formatter' : ECDSA(),
    'crop_size_end': -2,
    'section': 'p256ecdsa_sha256_vectors_h__',
    'comment' : ''
}

p384ecdsa_sha384_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'ecdsa_secp384r1_sha384_test.json',
    'target': '../testvectors/p384ecdsa-sha384-vectors.h',
    'array_init': 'const EcdsaTestVector kP384EcdsaSha384Vectors[] = {\n',
    'formatter' : ECDSA(),
    'crop_size_end': -2,
    'section': 'p384ecdsa_sha384_vectors_h__',
    'comment' : ''
}

p521ecdsa_sha512_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'ecdsa_secp521r1_sha512_test.json',
    'target': '../testvectors/p521ecdsa-sha512-vectors.h',
    'array_init': 'const EcdsaTestVector kP521EcdsaSha512Vectors[] = {\n',
    'formatter' : ECDSA(),
    'crop_size_end': -2,
    'section': 'p521ecdsa_sha512_vectors_h__',
    'comment' : ''
}

rsa_signature_2048_sha224_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_signature_2048_sha224_test.json',
    'target': '../testvectors/rsa_signature_2048_sha224-vectors.h',
    'array_init': 'const RsaSignatureTestVector kRsaSignature2048Sha224WycheproofVectors[] = {\n',
    'formatter' : RSA_PKCS1_SIGNATURE(),
    'crop_size_end': -2,
    'section': 'rsa_signature_2048_sha224_vectors_h__',
    'comment' : ''
}

rsa_signature_2048_sha256_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_signature_2048_sha256_test.json',
    'target': '../testvectors/rsa_signature_2048_sha256-vectors.h',
    'array_init': 'const RsaSignatureTestVector kRsaSignature2048Sha256WycheproofVectors[] = {\n',
    'formatter' : RSA_PKCS1_SIGNATURE(),
    'crop_size_end': -2,
    'section': 'rsa_signature_2048_sha256_vectors_h__',
    'comment' : ''
}

rsa_signature_2048_sha512_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_signature_2048_sha512_test.json',
    'target': '../testvectors/rsa_signature_2048_sha512-vectors.h',
    'array_init': 'const RsaSignatureTestVector kRsaSignature2048Sha512WycheproofVectors[] = {\n',
    'formatter' : RSA_PKCS1_SIGNATURE(),
    'crop_size_end': -2,
    'section': 'rsa_signature_2048_sha512_vectors_h__',
    'comment' : ''
}

rsa_signature_3072_sha256_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_signature_3072_sha256_test.json',
    'target': '../testvectors/rsa_signature_3072_sha256-vectors.h',
    'array_init': 'const RsaSignatureTestVector kRsaSignature3072Sha256WycheproofVectors[] = {\n',
    'formatter' : RSA_PKCS1_SIGNATURE(),
    'crop_size_end': -2,
    'section': 'rsa_signature_3072_sha256_vectors_h__',
    'comment' : ''
}

rsa_signature_3072_sha256_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_signature_3072_sha256_test.json',
    'target': '../testvectors/rsa_signature_3072_sha256-vectors.h',
    'array_init': 'const RsaSignatureTestVector kRsaSignature3072Sha256WycheproofVectors[] = {\n',
    'formatter' : RSA_PKCS1_SIGNATURE(),
    'crop_size_end': -2,
    'section': 'rsa_signature_3072_sha256_vectors_h__',
    'comment' : ''
}

rsa_signature_3072_sha384_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_signature_3072_sha384_test.json',
    'target': '../testvectors/rsa_signature_3072_sha384-vectors.h',
    'array_init': 'const RsaSignatureTestVector kRsaSignature3072Sha384WycheproofVectors[] = {\n',
    'formatter' : RSA_PKCS1_SIGNATURE(),
    'crop_size_end': -2,
    'section': 'rsa_signature_3072_sha384_vectors_h__',
    'comment' : ''
}

rsa_signature_3072_sha512_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_signature_3072_sha512_test.json',
    'target': '../testvectors/rsa_signature_3072_sha512-vectors.h',
    'array_init': 'const RsaSignatureTestVector kRsaSignature3072Sha512WycheproofVectors[] = {\n',
    'formatter' : RSA_PKCS1_SIGNATURE(),
    'crop_size_end': -2,
    'section': 'rsa_signature_3072_sha512_vectors_h__',
    'comment' : ''
}

rsa_signature_4096_sha384_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_signature_4096_sha384_test.json',
    'target': '../testvectors/rsa_signature_4096_sha384-vectors.h',
    'array_init': 'const RsaSignatureTestVector kRsaSignature4096Sha384WycheproofVectors[] = {\n',
    'formatter' : RSA_PKCS1_SIGNATURE(),
    'crop_size_end': -2,
    'section': 'rsa_signature_4096_sha384_vectors_h__',
    'comment' : ''
}

rsa_signature_4096_sha512_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_signature_4096_sha512_test.json',
    'target': '../testvectors/rsa_signature_4096_sha512-vectors.h',
    'array_init': 'const RsaSignatureTestVector kRsaSignature4096Sha512WycheproofVectors[] = {\n',
    'formatter' : RSA_PKCS1_SIGNATURE(),
    'crop_size_end': -2,
    'section': 'rsa_signature_4096_sha512_vectors_h__',
    'comment' : ''
}

rsa_signature_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_signature_test.json',
    'base': '../testvectors_base/rsa_signature-vectors_base.txt',
    'target': '../testvectors/rsa_signature-vectors.h',
    'array_init': 'const RsaSignatureTestVector kRsaSignatureWycheproofVectors[] = {\n',
    'formatter' : RSA_PKCS1_SIGNATURE(),
    'crop_size_end': -2,
    'section': 'rsa_signature_vectors_h__',
    'comment' : ''
}

rsa_pkcs1_dec_2048_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_pkcs1_2048_test.json',
    'target': '../testvectors/rsa_pkcs1_2048_test-vectors.h',
    'array_init': 'const RsaDecryptTestVector kRsa2048DecryptWycheproofVectors[] = {\n',
    'formatter' : RSA_PKCS1_DECRYPT(),
    'crop_size_end': -2,
    'section': 'rsa_pkcs1_2048_vectors_h__',
    'comment' : ''
}

rsa_pkcs1_dec_3072_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_pkcs1_3072_test.json',
    'target': '../testvectors/rsa_pkcs1_3072_test-vectors.h',
    'array_init': 'const RsaDecryptTestVector kRsa3072DecryptWycheproofVectors[] = {\n',
    'formatter' : RSA_PKCS1_DECRYPT(),
    'crop_size_end': -2,
    'section': 'rsa_pkcs1_3072_vectors_h__',
    'comment' : ''
}

rsa_pkcs1_dec_4096_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_pkcs1_4096_test.json',
    'target': '../testvectors/rsa_pkcs1_4096_test-vectors.h',
    'array_init': 'const RsaDecryptTestVector kRsa4096DecryptWycheproofVectors[] = {\n',
    'formatter' : RSA_PKCS1_DECRYPT(),
    'crop_size_end': -2,
    'section': 'rsa_pkcs1_4096_vectors_h__',
    'comment' : ''
}

rsa_pss_2048_sha256_mgf1_32_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_pss_2048_sha256_mgf1_32_test.json',
    'target': '../testvectors/rsa_pss_2048_sha256_mgf1_32-vectors.h',
    # One key is used in both files. Just pull in the header that defines it.
    'array_init': '''#include "testvectors/rsa_pss_2048_sha256_mgf1_0-vectors.h"\n\n
                      const RsaPssTestVector kRsaPss2048Sha25632WycheproofVectors[] = {\n''',
    'formatter' : RSA_PSS(),
    'crop_size_end': -2,
    'section': 'rsa_pss_2048_sha256_32_vectors_h__',
    'comment' : ''
}

rsa_pss_2048_sha256_mgf1_0_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_pss_2048_sha256_mgf1_0_test.json',
    'target': '../testvectors/rsa_pss_2048_sha256_mgf1_0-vectors.h',
    'array_init': 'const RsaPssTestVector kRsaPss2048Sha2560WycheproofVectors[] = {\n',
    'formatter' : RSA_PSS(),
    'crop_size_end': -2,
    'section': 'rsa_pss_2048_sha256_0_vectors_h__',
    'comment' : ''
}

rsa_pss_2048_sha1_mgf1_20_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_pss_2048_sha1_mgf1_20_test.json',
    'target': '../testvectors/rsa_pss_2048_sha1_mgf1_20-vectors.h',
    'array_init': 'const RsaPssTestVector kRsaPss2048Sha120WycheproofVectors[] = {\n',
    'formatter' : RSA_PSS(),
    'crop_size_end': -2,
    'section': 'rsa_pss_2048_sha1_20_vectors_h__',
    'comment' : ''
}

rsa_pss_3072_sha256_mgf1_32_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_pss_3072_sha256_mgf1_32_test.json',
    'target': '../testvectors/rsa_pss_3072_sha256_mgf1_32-vectors.h',
    'array_init': 'const RsaPssTestVector kRsaPss3072Sha25632WycheproofVectors[] = {\n',
    'formatter' : RSA_PSS(),
    'crop_size_end': -2,
    'section': 'rsa_pss_3072_sha256_32_vectors_h__',
    'comment' : ''
}

rsa_pss_4096_sha256_mgf1_32_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_pss_4096_sha256_mgf1_32_test.json',
    'target': '../testvectors/rsa_pss_4096_sha256_mgf1_32-vectors.h',
    'array_init': 'const RsaPssTestVector kRsaPss4096Sha25632WycheproofVectors[] = {\n',
    'formatter' : RSA_PSS(),
    'crop_size_end': -2,
    'section': 'rsa_pss_4096_sha256_32_vectors_h__',
    'comment' : ''
}

rsa_pss_4096_sha512_mgf1_32_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_pss_4096_sha512_mgf1_32_test.json',
    'target': '../testvectors/rsa_pss_4096_sha512_mgf1_32-vectors.h',
    'array_init': 'const RsaPssTestVector kRsaPss4096Sha51232WycheproofVectors[] = {\n',
    'formatter' : RSA_PSS(),
    'crop_size_end': -2,
    'section': 'rsa_pss_4096_sha512_32_vectors_h__',
    'comment' : ''
}

rsa_pss_misc_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_pss_misc_test.json',
    'target': '../testvectors/rsa_pss_misc-vectors.h',
    'array_init': 'const RsaPssTestVector kRsaPssMiscWycheproofVectors[] = {\n',
    'formatter' : RSA_PSS(),
    'crop_size_end': -2,
    'section': 'rsa_pss_misc_vectors_h__',
    'comment' : ''
}

rsa_oaep_2048_sha1_mgf1sha1_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_oaep_2048_sha1_mgf1sha1_test.json',
    'target': '../testvectors/rsa_oaep_2048_sha1_mgf1sha1-vectors.h',
    'array_init': 'const RsaOaepTestVector kRsaOaep2048Sha1WycheproofVectors[] = {\n',
    'formatter' : RSA_OAEP(),
    'crop_size_end': -2,
    'section': 'rsa_oaep_2048_sha1_mgf1sha1_vectors_h__',
    'comment' : ''
}

rsa_oaep_2048_sha256_mgf1sha1_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_oaep_2048_sha256_mgf1sha1_test.json',
    'target': '../testvectors/rsa_oaep_2048_sha256_mgf1sha1-vectors.h',
    'array_init': 'const RsaOaepTestVector kRsaOaep2048Sha256Mgf1Sha1WycheproofVectors[] = {\n',
    'formatter' : RSA_OAEP(),
    'crop_size_end': -2,
    'section': 'rsa_oaep_2048_sha256_mgf1sha1_vectors_h__',
    'comment' : ''
}

rsa_oaep_2048_sha256_mgf1sha256_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_oaep_2048_sha256_mgf1sha256_test.json',
    'target': '../testvectors/rsa_oaep_2048_sha256_mgf1sha256-vectors.h',
    'array_init': 'const RsaOaepTestVector kRsaOaep2048Sha256Mgf1Sha256WycheproofVectors[] = {\n',
    'formatter' : RSA_OAEP(),
    'crop_size_end': -2,
    'section': 'rsa_oaep_2048_sha256_mgf1sha256_vectors_h__',
    'comment' : ''
}

rsa_oaep_2048_sha384_mgf1sha1_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_oaep_2048_sha384_mgf1sha1_test.json',
    'target': '../testvectors/rsa_oaep_2048_sha384_mgf1sha1-vectors.h',
    'array_init': 'const RsaOaepTestVector kRsaOaep2048Sha384Mgf1Sha1WycheproofVectors[] = {\n',
    'formatter' : RSA_OAEP(),
    'crop_size_end': -2,
    'section': 'rsa_oaep_2048_sha384_mgf1sha1_vectors_h__',
    'comment' : ''
}

rsa_oaep_2048_sha384_mgf1sha384_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_oaep_2048_sha384_mgf1sha384_test.json',
    'target': '../testvectors/rsa_oaep_2048_sha384_mgf1sha384-vectors.h',
    'array_init': 'const RsaOaepTestVector kRsaOaep2048Sha384Mgf1Sha384WycheproofVectors[] = {\n',
    'formatter' : RSA_OAEP(),
    'crop_size_end': -2,
    'section': 'rsa_oaep_2048_sha384_mgf1sha384_vectors_h__',
    'comment' : ''
}

rsa_oaep_2048_sha512_mgf1sha1_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_oaep_2048_sha512_mgf1sha1_test.json',
    'target': '../testvectors/rsa_oaep_2048_sha512_mgf1sha1-vectors.h',
    'array_init': 'const RsaOaepTestVector kRsaOaep2048Sha512Mgf1Sha1WycheproofVectors[] = {\n',
    'formatter' : RSA_OAEP(),
    'crop_size_end': -2,
    'section': 'rsa_oaep_2048_sha512_mgf1sha1_vectors_h__',
    'comment' : ''
}

rsa_oaep_2048_sha512_mgf1sha512_params = {
    'source_dir': 'source_vectors/',
    'source_file': 'rsa_oaep_2048_sha512_mgf1sha512_test.json',
    'target': '../testvectors/rsa_oaep_2048_sha512_mgf1sha512-vectors.h',
    'array_init': 'const RsaOaepTestVector kRsaOaep2048Sha512Mgf1Sha512WycheproofVectors[] = {\n',
    'formatter' : RSA_OAEP(),
    'crop_size_end': -2,
    'section': 'rsa_oaep_2048_sha512_mgf1sha512_vectors_h__',
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
                 dsa_params,
                 hkdf_sha1_params,
                 hkdf_sha256_params,
                 hkdf_sha384_params,
                 hkdf_sha512_params,
                 p256ecdsa_sha256_params,
                 p384ecdsa_sha384_params,
                 p521ecdsa_sha512_params,
                 p256ecdh_params,
                 p384ecdh_params,
                 p521ecdh_params,
                 rsa_oaep_2048_sha1_mgf1sha1_params,
                 rsa_oaep_2048_sha256_mgf1sha1_params,
                 rsa_oaep_2048_sha256_mgf1sha256_params,
                 rsa_oaep_2048_sha384_mgf1sha1_params,
                 rsa_oaep_2048_sha384_mgf1sha384_params,
                 rsa_oaep_2048_sha512_mgf1sha1_params,
                 rsa_oaep_2048_sha512_mgf1sha512_params,
                 rsa_pkcs1_dec_2048_params,
                 rsa_pkcs1_dec_3072_params,
                 rsa_pkcs1_dec_4096_params,
                 rsa_pss_2048_sha1_mgf1_20_params,
                 rsa_pss_2048_sha256_mgf1_0_params,
                 rsa_pss_2048_sha256_mgf1_32_params,
                 rsa_pss_3072_sha256_mgf1_32_params,
                 rsa_pss_4096_sha256_mgf1_32_params,
                 rsa_pss_4096_sha512_mgf1_32_params,
                 rsa_pss_misc_params,
                 rsa_signature_2048_sha224_params,
                 rsa_signature_2048_sha256_params,
                 rsa_signature_2048_sha512_params,
                 rsa_signature_3072_sha256_params,
                 rsa_signature_3072_sha384_params,
                 rsa_signature_3072_sha512_params,
                 rsa_signature_4096_sha384_params,
                 rsa_signature_4096_sha512_params,
                 rsa_signature_params]
    update_tests(all_tests)
    for test in all_tests:
        generate_vectors_file(test)

def main():
    generate_test_vectors()

if __name__ == '__main__':
    main()
