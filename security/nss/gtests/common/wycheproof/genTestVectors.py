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
                 dsa_params,
                 hkdf_sha1_params,
                 hkdf_sha256_params,
                 hkdf_sha384_params,
                 hkdf_sha512_params,
                 rsa_oaep_2048_sha1_mgf1sha1_params,
                 rsa_oaep_2048_sha256_mgf1sha1_params,
                 rsa_oaep_2048_sha256_mgf1sha256_params,
                 rsa_oaep_2048_sha384_mgf1sha1_params,
                 rsa_oaep_2048_sha384_mgf1sha384_params,
                 rsa_oaep_2048_sha512_mgf1sha1_params,
                 rsa_oaep_2048_sha512_mgf1sha512_params]
    update_tests(all_tests)
    for test in all_tests:
        generate_vectors_file(test)

def main():
    generate_test_vectors()

if __name__ == '__main__':
    main()
