# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

# Tooltool packages generated from the script below.
WINDOWS = {
    'package_filename': 'clang.tar.bz2',
    'package_sha512sum': 'cd3ed31acefd185f441632158dde73538c62bab7ebf2a8ec630985ab345938ec522983721ddb1bead1de22d5ac1571d50a958ae002364d739f2a78c6e7244222',
}

OSX = {
    'package_filename': 'clang.tar.bz2',
    'package_sha512sum': '0e1a556b65d6398fa812b9ceb5ce5e2dec3eda77d4a032a818182b34fc8ce602412f42388bb1fda6bea265d35c1dde3847a730b264fec01cd7e3dcfd39941660',
}

LINUX = {
    'package_filename': 'clang.tar.xz',
    'package_sha512sum': '52f3fc23f0f5c98050f8b0ac7c92a6752d067582a16f712a5a58074be98975d594f9e36249fc2be7f1cc2ca6d509c663faaf2bea66f949243cc1f41651638ba6',
}

if __name__ == '__main__':
    '''Allow invoking the module as a utility to update tooltool downloads.'''
    import json
    import os
    import sys

    mod_path = os.path.dirname(__file__)
    browser_config_dir = os.path.join('browser', 'config', 'tooltool-manifests')

    os_map = {
        'WINDOWS': ('win64', 'clang.manifest'),
        'OSX': ('macosx64', 'releng.manifest'),
        'LINUX': ('linux64', 'releng.manifest'),
    }

    for os_name, (os_dir, f) in os_map.iteritems():
        manifest_file = os.path.join(browser_config_dir, os_dir, f)
        abspath = os.path.join(mod_path, '..', '..', '..', manifest_file)
        with open(abspath, 'r') as s:
            manifest = json.load(s)
            entries = filter(lambda x: x['filename'].startswith('clang'), manifest)
            if not entries:
                print('ERROR: could not find clang tooltool entry in %s' % manifest_file)
                sys.exit(1)
            if len(entries) > 1:
                print('ERROR: too many clang entries in %s' % manifest_file)
                sys.exit(1)

            clang = entries[0]
            if clang['algorithm'] != 'sha512':
                print("ERROR: don't know how to handle digest %s in %s" % (clang['algorithm'],
                                                                           manifest_file))
                sys.exit(1)

            FORMAT_STRING = """{os} = {{
    'package_filename': '{filename}',
    'package_sha512sum': '{digest}',
}}
"""
            digest = clang['digest']
            if os_name == 'WINDOWS':
                # The only clang version we can retrieve from the tooltool manifest
                # doesn't work with Stylo bindgen due to regressions in LLVM.  This
                # is an older version that works.
                digest = 'cd3ed31acefd185f441632158dde73538c62bab7ebf2a8ec630985ab345938ec522983721ddb1bead1de22d5ac1571d50a958ae002364d739f2a78c6e7244222'
            print(FORMAT_STRING.format(os=os_name,
                                       filename=clang['filename'],
                                       digest=digest))
