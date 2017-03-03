#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import os
import re
import shutil
import sys
import datetime
import subprocess
import tarfile
import urllib
from pprint import pprint
from StringIO import StringIO

def prepare_upstream(prefix, commit=None):
    upstream_url = 'https://chromium.googlesource.com/libyuv/libyuv'
    shutil.rmtree(os.path.join(base, 'libyuv/'))
    print(upstream_url + '/+archive/' + commit + '.tar.gz')
    urllib.urlretrieve(upstream_url + '/+archive/' + commit + '.tar.gz', 'libyuv.tar.gz')
    tarfile.open('libyuv.tar.gz').extractall(path='libyuv')
    os.remove(os.path.join(base, 'libyuv.tar.gz'))
    os.chdir(base)
    return commit

def get_commit_date(prefix, commit=None):
    upstream_url = 'https://chromium.googlesource.com/libyuv/libyuv/+/' + commit
    text = urllib.urlopen(upstream_url).readlines()
    text = "".join(text)
    regex = '<tr><th class="Metadata-title">committer</th><td>.+</td><td>[^\s]+ ([0-9a-zA-Z: ]+)\s*\+*[0-9]*</td></tr>'
    date = re.search(regex, text).groups(0)[0]
    return datetime.datetime.strptime(date, "%b %d %H:%M:%S %Y")

def cleanup_upstream():
    os.remove(os.path.join(base, 'libyuv/.gitignore'))

def apply_patches():
    # Patch to update gyp build files
    os.system("patch -p3 < update_gyp.patch")
    # Patch to fix build errors
    os.system("patch -p3 < fix_build_errors.patch")
    # Patch to make mjpeg printfs optional at build time
    os.system("patch -p3 < make_mjpeg_printfs_optional.patch")
    # Patch to allow disabling of inline ASM and AVX2 code
    os.system("patch -p3 < allow_disabling_asm_avx2.patch")
    # Patch to add H444ToARGB() variant
    os.system("patch -p3 < add_H444ToARGB.patch")
    # Patch for bug 1342730
    os.system("patch -p3 < cpu_id.patch")
    # Patch for bug 1342730
    os.system("patch -p3 < cpu_id.patch")
    # Patch for bug 1342732
    os.system("patch -p3 < row_any.patch")

def update_readme(commit, commitdate):
    with open('README_MOZILLA') as f:
        readme = f.read()

    if 'The git commit ID last used to import was ' in readme:
        new_readme = re.sub('The git commit ID last used to import was [v\.a-f0-9]+ \(.+\)',
            'The git commit ID last used to import was %s (%s)' % (commit, commitdate), readme)
    else:
        new_readme = "%s\n\nThe git commit ID last used to import was %s\n" % (readme, commit)

    if readme != new_readme:
        with open('README_MOZILLA', 'w') as f:
            f.write(new_readme)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='''Update libyuv''')
    parser.add_argument('--debug', dest='debug', action="store_true")
    parser.add_argument('--commit', dest='commit', type=str, default='master')

    args = parser.parse_args()

    commit = args.commit
    DEBUG = args.debug

    base = os.path.abspath(os.curdir)
    prefix = os.path.join(base, 'libyuv/')

    commit = prepare_upstream(prefix, commit)
    commitdate = get_commit_date(prefix, commit)

    apply_patches()
    update_readme(commit, commitdate)

    print('Patches applied; run "hg addremove --similarity 70 libyuv" before committing changes')

    cleanup_upstream()
