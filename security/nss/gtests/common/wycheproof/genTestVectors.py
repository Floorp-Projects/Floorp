#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os

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


def generate_header(params):
    """
    Generate and store a .h-file with test vectors for one test.

    params -- Dictionary with parameters for test vector generation for the desired test.
    """

    cases = import_testvector(os.path.join(script_dir, params['source']))

    with open(os.path.join(script_dir, params['base'])) as base:
        header = base.read()

    header = header[:params['crop_size_start']]
    header += '\n\n// Testvectors from project wycheproof\n'
    header += '// <https://github.com/google/wycheproof>\n'
    vectors_file = header + params['array_init']

    for group in cases['testGroups']:
        for test in group['tests']:
            vectors_file += params['formatter'].format_testcase(test)

    vectors_file = vectors_file[:params['crop_size_end']] + '};\n\n'
    vectors_file += params['finish']

    with open(os.path.join(script_dir, params['target']), 'w') as target:
        target.write(vectors_file)

# Parameters that describe the generation of a testvector file for each supoorted testself.
# source -- relaive path the wycheproof JSON source file with testvectorsself.
# base -- relative path to the pre-fabricated .h-file with general defintions and non-wycheproof vectors.
# target -- relative path to where the finished .h-file is written.
# crop_size_start -- number of characters removed from the end of the base file at start.
# array_init -- string to initialize the c-header style array of testvectors.
# formatter -- the test case formatter class to be used for this test.
# crop_size_end -- number of characters removed from the end of the last generated test vector to close the array definiton.
# finish -- string to re-insert at the end and finish the file. (identical to chars cropped at the start)
aes_gcm_params = {
    'source': 'testvectors/aes_gcm_test.json',
    'base': 'header_bases/gcm-vectors.h',
    'target': '../gcm-vectors.h',
    'crop_size_start': -27,
    'array_init': 'const gcm_kat_value kGcmWycheproofVectors[] = {\n',
    'formatter' : AESGCM(),
    'crop_size_end': -3,
    'finish': '#endif  // gcm_vectors_h__\n'
}

chacha_poly_params = {
    'source': 'testvectors/chacha20_poly1305_test.json',
    'base': 'header_bases/chachapoly-vectors.h',
    'target': '../chachapoly-vectors.h',
    'crop_size_start': -35,
    'array_init': 'const chacha_testvector kChaCha20WycheproofVectors[] = {\n',
    'formatter' : ChaChaPoly(),
    'crop_size_end': -2,
    'finish': '#endif  // chachapoly_vectors_h__\n'
}

def generate_test_vectors():
    """Generate C-header files for all supported tests."""
    all_params = [aes_gcm_params, chacha_poly_params]
    for param in all_params:
        generate_header(param)

def main():
    generate_test_vectors()

if __name__ == '__main__':
    main()
