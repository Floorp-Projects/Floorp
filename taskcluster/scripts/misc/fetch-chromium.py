#!/usr/bin/python3 -u
# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
This script downloads the latest chromium build (or a manually
defined version) for a given platform. It then uploads the build,
with the revision of the build stored in a REVISION file.
'''

from __future__ import absolute_import, print_function

import argparse
import errno
import json
import os
import shutil
import subprocess
import requests
import tempfile

NEWEST_BASE_URL = 'https://download-chromium.appspot.com/'
NEWREV_DOWNLOAD_URL = NEWEST_BASE_URL + 'dl/%s?type=snapshots'
NEWREV_REVISION_URL = NEWEST_BASE_URL + 'rev/%s?type=snapshots'

OLDREV_BASE_URL = 'http://commondatastorage.googleapis.com/chromium-browser-snapshots/'
OLDREV_DOWNLOAD_URL = OLDREV_BASE_URL + '%s/%s/%s'  # (platform/revision/archive)

CHROMIUM_INFO = {
    'linux': {
        'platform': 'Linux_x64',
        'archive': 'chrome-linux.zip',
        'result': 'chromium-linux.tar.bz2'
    },
    'win32': {
        'platform': 'Win_x64',
        'archive': 'chrome-win.zip',
        'result': 'chromium-win32.tar.bz2'
    },
    'win64': {
        'platform': 'Win_x64',
        'archive': 'chrome-win.zip',
        'result': 'chromium-win64.tar.bz2'
    },
    'mac': {
        'platform': 'Mac',
        'archive': 'chrome-mac.zip',
        'result': 'chromium-mac.tar.bz2'
    }
}


def log(msg):
    print('build-chromium: %s' % msg)


def fetch_file(url, filepath):
    '''Download a file from the given url to a given file.'''
    size = 4096
    r = requests.get(url, stream=True)
    r.raise_for_status()

    with open(filepath, 'wb') as fd:
        for chunk in r.iter_content(size):
            fd.write(chunk)


def fetch_chromium_revision(platform):
    '''Get the revision of the latest chromium build. '''
    chromium_platform = CHROMIUM_INFO[platform]['platform']
    revision_url = NEWREV_REVISION_URL % chromium_platform

    log(
        'Getting revision number for latest %s chromium build...' %
        chromium_platform
    )

    # Expects a JSON of the form:
    # {
    #   'content': '<REVNUM>',
    #   'last-modified': '<DATE>'
    # }
    r = requests.get(revision_url, timeout=30)
    r.raise_for_status()

    chromium_revision = json.loads(
        r.content.decode('utf-8')
    )['content']

    return chromium_revision


def fetch_chromium_build(platform, revision, zippath):
    '''Download a chromium build for a given revision, or the latest. '''
    use_oldrev = True
    if not revision:
        use_oldrev = False
        revision = fetch_chromium_revision(platform)

    download_platform = CHROMIUM_INFO[platform]['platform']
    if use_oldrev:
        chromium_archive = CHROMIUM_INFO[platform]['archive']
        download_url = OLDREV_DOWNLOAD_URL % (
            download_platform, revision, chromium_archive
        )
    else:
        download_url = NEWREV_DOWNLOAD_URL % download_platform

    log(
        'Downloading %s chromium build revision %s...' %
        (download_platform, revision)
    )

    fetch_file(download_url, zippath)
    return revision


def build_chromium_archive(platform, revision=None):
    '''
    Download and store a chromium build for a given platform.

    Retrieves either the latest version, or uses a pre-defined version if
    the `--revision` option is given a revision.
    '''
    upload_dir = os.environ.get('UPLOAD_DIR')
    if upload_dir:
        # Create the upload directory if it doesn't exist.
        try:
            log('Creating upload directory in %s...' % os.path.abspath(upload_dir))
            os.makedirs(upload_dir)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise

    # Make a temporary location for the file
    tmppath = tempfile.mkdtemp()
    tmpzip = os.path.join(tmppath, 'tmp-chromium.zip')

    revision = fetch_chromium_build(platform, revision, tmpzip)

    # Unpack archive in `tmpzip` to store the revision number
    log('Unpacking archive at: %s to: %s' % (tmpzip, tmppath))
    unzip_command = ['unzip', '-q', '-o', tmpzip, '-d', tmppath]
    subprocess.check_call(unzip_command)

    dirs = [
        d for d in os.listdir(tmppath)
        if os.path.isdir(os.path.join(tmppath, d)) and d.startswith('chrome-')
    ]

    if len(dirs) > 1:
        raise Exception(
            "Too many directories starting with 'chrome-' after extracting."
        )
    elif len(dirs) == 0:
        raise Exception(
            "Could not find any directories after extraction of chromium zip."
        )

    chromium_dir = os.path.join(tmppath, dirs[0])
    revision_file = os.path.join(chromium_dir, '.REVISION')
    with open(revision_file, 'w+') as f:
        f.write(str(revision))

    tar_file = CHROMIUM_INFO[platform]['result']
    tar_command = ['tar', 'cjf', tar_file, '-C', tmppath, dirs[0]]
    log(
        "Added revision to %s file." % revision_file
    )

    log('Tarring with the command: %s' % str(tar_command))
    subprocess.check_call(tar_command)

    upload_dir = os.environ.get('UPLOAD_DIR')
    if upload_dir:
        # Move the tarball to the output directory for upload.
        log('Moving %s to the upload directory...' % tar_file)
        shutil.copy(tar_file, os.path.join(upload_dir, tar_file))

    shutil.rmtree(tmppath)


def parse_args():
    '''Read command line arguments and return options.'''
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--platform',
        help='Platform version of chromium to build.',
        required=True
    )
    parser.add_argument(
        '--revision',
        help='Revision of chromium to build to get. '
             '(Defaults to the newest chromium build).',
        default=None
    )

    return parser.parse_args()


if __name__ == '__main__':
    args = vars(parse_args())
    build_chromium_archive(**args)
