#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import os
import re
import shutil
import sys
import subprocess
from pprint import pprint
from StringIO import StringIO

libvpx_files = [
    'build/make/ads2gas.pl',
    'build/make/thumb.pm',
    'LICENSE',
    'PATENTS',
]

def prepare_upstream(prefix, commit=None):
    upstream_url = 'https://chromium.googlesource.com/webm/libvpx'
    shutil.rmtree(os.path.join(base, 'libvpx/'))
    subprocess.call(['git', 'clone', upstream_url, prefix])
    os.chdir(prefix)
    if commit:
        subprocess.call(['git', 'checkout', commit])
    else:
        p = subprocess.Popen(['git', 'rev-parse', 'HEAD'], stdout=subprocess.PIPE)
        stdout, stderr = p.communicate()
        commit = stdout.strip()

    os.chdir(base)
    return commit

def cleanup_upstream():
    shutil.rmtree(os.path.join(base, 'libvpx/.git'))
    os.remove(os.path.join(base, 'libvpx/.gitattributes'))
    os.remove(os.path.join(base, 'libvpx/.gitignore'))
    os.remove(os.path.join(base, 'libvpx/build/.gitattributes'))
    os.remove(os.path.join(base, 'libvpx/build/.gitignore'))

def apply_patches():
    # Patch to permit vpx users to specify their own <stdint.h> types.
    os.system("patch -p0 < stdint.patch")
    # Patch to fix a crash caused by MSVC 2013
    os.system("patch -p3 < bug1137614.patch")
    # Cherry pick https://chromium-review.googlesource.com/#/c/276889/
    # to fix crash on 32bit
    os.system("patch -p1 < vp9_filter_restore_aligment.patch")
    # Patch win32 vpx_once.
    os.system("patch -p3 < vpx_once.patch")
    # Bug 1224363 - Clamp seg_lvl also in abs-value mode.
    os.system("patch -p3 < clamp_abs_lvl_seg.patch")
    # Bug 1224361 - Clamp QIndex also in abs-value mode.
    os.system("patch -p3 < clamp-abs-QIndex.patch")
    # Bug 1233983 - Make libvpx build with clang-cl
    os.system("patch -p3 < clang-cl.patch")
    # Bug 1224371 - Cast uint8_t to uint32_t before shift
    os.system("patch -p3 < cast-char-to-uint-before-shift.patch")
    # Bug 1237848 - Check lookahead ctx
    os.system("patch -p3 < 1237848-check-lookahead-ctx.patch")
    # Bug 1263384 - Check input frame resolution
    os.system("patch -p3 < input_frame_validation.patch")
    # Bug 1315288 - Check input frame resolution for vp9
    os.system("patch -p3 < input_frame_validation_vp9.patch")
    # Fix the make files so they don't refer to the duplicate-named files.
    os.system("patch -p1 < rename_duplicate_files.patch")
    # Correctly identify vp9_block_error_fp as using x86inc
    os.system("patch -p1 < block_error_fp.patch")

def update_readme(commit):
    with open('README_MOZILLA') as f:
        readme = f.read()

    if 'The git commit ID used was' in readme:
        new_readme = re.sub('The git commit ID used was [a-f0-9]+',
            'The git commit ID used was %s' % commit, readme)
    else:
        new_readme = "%s\n\nThe git commit ID used was %s\n" % (readme, commit)

    if readme != new_readme:
        with open('README_MOZILLA', 'w') as f:
            f.write(new_readme)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='''Update libvpx''')
    parser.add_argument('--debug', dest='debug', action="store_true")
    parser.add_argument('--commit', dest='commit', type=str, default=None)

    args = parser.parse_args()

    commit = args.commit
    DEBUG = args.debug

    base = os.path.abspath(os.curdir)
    prefix = os.path.join(base, 'libvpx/')

    commit = prepare_upstream(prefix, commit)

    apply_patches()
    update_readme(commit)

    cleanup_upstream()
