#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import datetime
import os
import re
import shutil
import tarfile
import urllib
from subprocess import Popen, PIPE, STDOUT


def prepare_upstream(base, commit):
    upstream_url = 'https://chromium.googlesource.com/libyuv/libyuv'
    tarball_file = os.path.join(base, 'libyuv.tar.gz')
    lib_path = os.path.join(base, 'libyuv')

    print(upstream_url + '/+archive/' + commit + '.tar.gz')
    urllib.urlretrieve(upstream_url + '/+archive/' + commit + '.tar.gz',
                       tarball_file)
    shutil.rmtree(lib_path)
    tarfile.open(tarball_file).extractall(path=lib_path)
    os.remove(tarball_file)

    shutil.copy2(os.path.join(lib_path, "LICENSE"), os.path.join(base, "LICENSE"))


def get_commit_date(commit):
    upstream_url = 'https://chromium.googlesource.com/libyuv/libyuv/+/' + commit
    text = urllib.urlopen(upstream_url).read()
    regex = r'<tr><th class="Metadata-title">committer</th>' \
            r'<td>.+</td><td>[^\s]+ ([0-9a-zA-Z: ]+)\s*\+*[0-9]*</td></tr>'
    date = re.search(regex, text).group(1)
    return datetime.datetime.strptime(date, "%b %d %H:%M:%S %Y")


def cleanup_upstream(base):
    os.remove(os.path.join(base, 'libyuv/.gitignore'))


def apply_patches(base):
    patches = [
        # update gyp build files
        "update_gyp.patch",
        # fix build errors
        'fix_build_errors.patch',
        # make mjpeg printfs optional at build time
        'make_mjpeg_printfs_optional.patch',
        # allow disabling of inline ASM and AVX2 code
        'allow_disabling_asm_avx2.patch',
    ]

    for patch in patches:
        print('\nApplying patch %s' % patch)
        with open(os.path.join(base, patch)) as f:
            Popen(["patch", "-p3"], stdin=f, cwd=base).wait()


def update_moz_yaml(base, commit, commitdate):
    moz_yaml_file = os.path.join(base, 'moz.yaml')
    with open(moz_yaml_file) as f:
        moz_yaml = f.read()

    new_moz_yaml = re.sub(r'\n\s+release:.+\n',
                          '\n  release: "%s (%s)"\n' % (commit, commitdate),
                          moz_yaml)

    if moz_yaml != new_moz_yaml:
        with open(moz_yaml_file, 'w') as f:
            f.write(new_moz_yaml)


def main():
    parser = argparse.ArgumentParser(description='Update libyuv')
    parser.add_argument('--no-patches', dest='no_patches', action="store_true")
    parser.add_argument('--commit', dest='commit', default='master')
    args = parser.parse_args()

    commit = args.commit
    no_patches = args.no_patches
    base = os.path.realpath(os.path.dirname(__file__))

    prepare_upstream(base, commit)
    commitdate = get_commit_date(commit)

    if not no_patches:
        apply_patches(base)

    update_moz_yaml(base, commit, commitdate)

    print('\nPatches applied; '
          'run "hg addremove --similarity 70 libyuv" before committing changes')

    cleanup_upstream(base)


if __name__ == '__main__':
    main()
