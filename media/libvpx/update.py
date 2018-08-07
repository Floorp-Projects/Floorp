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
import tarfile
import urllib
from pprint import pprint
from StringIO import StringIO

def prepare_upstream(prefix, commit=None):
    upstream_url = 'https://chromium.googlesource.com/webm/libvpx'
    shutil.rmtree(os.path.join(base, 'libvpx/'))
    print(upstream_url + '/+archive/' + commit + '.tar.gz')
    urllib.urlretrieve(upstream_url + '/+archive/' + commit + '.tar.gz', 'libvpx.tar.gz')
    tarfile.open('libvpx.tar.gz').extractall(path='libvpx')
    os.remove(os.path.join(base, 'libvpx.tar.gz'))
    os.chdir(base)
    return commit

def cleanup_upstream():
    os.remove(os.path.join(base, 'libvpx/.gitattributes'))
    os.remove(os.path.join(base, 'libvpx/.gitignore'))
    os.remove(os.path.join(base, 'libvpx/build/.gitattributes'))
    os.remove(os.path.join(base, 'libvpx/build/.gitignore'))

def apply_patches():
    # Patch to permit vpx users to specify their own <stdint.h> types.
    os.system("patch -p3 < stdint.patch")
    # Patch to fix a crash caused by MSVC 2013
    os.system("patch -p3 < bug1137614.patch")
    # Bug 1263384 - Check input frame resolution
    os.system("patch -p3 < input_frame_validation.patch")
    # Bug 1315288 - Check input frame resolution for vp9
    os.system("patch -p3 < input_frame_validation_vp9.patch")
    # Avoid c/asm name collision for loopfilter_sse2
    os.system("patch -p1 < rename_duplicate_files.patch")
    os.system("mv libvpx/vpx_dsp/x86/loopfilter_sse2.c libvpx/vpx_dsp/x86/loopfilter_intrin_sse2.c")


def update_readme(commit):
    with open('README_MOZILLA') as f:
        readme = f.read()

    if 'The git commit ID used was' in readme:
        new_readme = re.sub('The git commit ID used was [v\.a-f0-9]+',
            'The git commit ID used was %s' % commit, readme)
    else:
        new_readme = "%s\n\nThe git commit ID used was %s\n" % (readme, commit)

    if readme != new_readme:
        with open('README_MOZILLA', 'w') as f:
            f.write(new_readme)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='''Update libvpx''')
    parser.add_argument('--debug', dest='debug', action="store_true")
    parser.add_argument('--commit', dest='commit', type=str, default='master')

    args = parser.parse_args()

    commit = args.commit
    DEBUG = args.debug

    base = os.path.abspath(os.curdir)
    prefix = os.path.join(base, 'libvpx/')

    commit = prepare_upstream(prefix, commit)

    apply_patches()
    update_readme(commit)

    cleanup_upstream()
