#!/bin/env python
'''
This script patches tooltool manifests in the firefox source
tree to update them to a new set of rust packages.
'''

import json
import os.path
import sys

from collections import OrderedDict


def load_manifest(path):
    with open(path) as f:
        return json.load(f, object_pairs_hook=OrderedDict)
    return None


def save_manifest(manifest, path):
    with open(path, 'w') as f:
        json.dump(manifest, f, indent=2, separators=(',', ': '))
        f.write('\n')


def replace(manifest, stanza):
    key = 'rustc'
    version = stanza.get('version')
    for s in manifest:
        if key in s.get('filename'):
            if version:
                print('Replacing %s\n     with %s' % (s['version'], version))
                s['version'] = version
            print('  old %s' % s['digest'][:12])
            s['digest'] = stanza['digest']
            s['size'] = stanza['size']
            print('  new %s' % s['digest'][:12])
            return True
    print('Warning: Could not find matching %s filename' % key)
    return False


def update_manifest(source_manifest, target, target_filename):
    for stanza in source_manifest:
        filename = stanza.get('filename')
        if target in filename:
            size = int(stanza.get('size'))
            print('Found %s %d bytes' % (filename, size))
            version = stanza.get('version')
            if version:
                print('  %s' % version)
            print('Updating %s' % target_filename)
            old = load_manifest(target_filename)
            replace(old, stanza)
            save_manifest(old, target_filename)
            break


'''Mapping from targets to target filenames.'''
TARGETS = {
        'x86_64-unknown-linux-gnu-repack': [
            'browser/config/tooltool-manifests/linux32/releng.manifest',
            'browser/config/tooltool-manifests/linux64/asan.manifest',
            'browser/config/tooltool-manifests/linux64/clang.manifest',
            'browser/config/tooltool-manifests/linux64/clang.manifest.centos6',
            'browser/config/tooltool-manifests/linux64/fuzzing.manifest',
            'browser/config/tooltool-manifests/linux64/hazard.manifest',
            'browser/config/tooltool-manifests/linux64/msan.manifest',
            'browser/config/tooltool-manifests/linux64/releng.manifest',
            ],
        'x86_64-unknown-linux-gnu-android-cross-repack': [
            'mobile/android/config/tooltool-manifests/android/releng.manifest',
            'mobile/android/config/tooltool-manifests/android-x86/releng.manifest',
            'mobile/android/config/tooltool-manifests/android-gradle-dependencies/releng.manifest',
            ],
        'x86_64-unknown-linux-gnu-mingw32-cross-repack': [
            'browser/config/tooltool-manifests/mingw32/releng.manifest',
            ],
        'x86_64-unknown-linux-gnu-mac-cross-repack': [
            'browser/config/tooltool-manifests/macosx64/cross-releng.manifest',
            ],
        'x86_64-apple-darwin-repack': [
            'browser/config/tooltool-manifests/macosx64/clang.manifest',
            'browser/config/tooltool-manifests/macosx64/releng.manifest',
            ],
        'x86_64-pc-windows-msvc-repack': [
            'browser/config/tooltool-manifests/win64/clang.manifest',
            'browser/config/tooltool-manifests/win64/releng.manifest',
            ],
        'i686-pc-windows-msvc-repack': [
            'browser/config/tooltool-manifests/win32/clang.manifest',
            'browser/config/tooltool-manifests/win32/releng.manifest',
            ],
}

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('%s PATH' % sys.argv[0])
        sys.exit(1)

    base_path = sys.argv[1]

    updates = load_manifest('manifest.tt')
    for target, filenames in TARGETS.items():
        for target_filename in filenames:
            update_manifest(updates, target,
                            os.path.join(base_path, target_filename))
