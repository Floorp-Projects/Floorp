# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# If we add unicode_literals, Python 2.6.1 (required for OS X 10.6) breaks.
from __future__ import print_function

import errno
import os
import stat
import subprocess
import sys

# Base url for pulling the rustup installer.
RUSTUP_URL_BASE = 'https://static.rust-lang.org/rustup'

# Pull this to get the lastest stable version number.
RUSTUP_MANIFEST = os.path.join(RUSTUP_URL_BASE, 'release-stable.toml')

# We bake in a known version number so we can verify a checksum.
RUSTUP_VERSION = '0.6.5'

# SHA-256 checksums of the installers, per platform.
RUSTUP_HASHES = {
    'x86_64-apple-darwin':
        '6404ab0a92c1559bac279a20d31be9166c91434f8e7ff8d1a97bcbe4dbd3cadc',
    'x86_64-pc-windows-msvc':
        '772579edcbc9a480a61fb19ace49527839e7f919e1041bcc2dee2a4ff82d3ca2',
    'x86_64-unknown-linux-gnu':
        'e901e23ee48c3a24470d997c4376d8835cecca51bf2636dcd419821d4056d823',
    'x86_64-unknown-freebsd':
        '63b7c0f35a811993c94af85b96abdd3dcca847d260af284f888e91c2ffdb374e',
}

NO_PLATFORM = '''
Sorry, we have no installer configured for your platform.

Please try installing rust for your system from https://rustup.rs/
or from https://rust-lang.org/ or from your package manager.
'''

def rustup_url(host, version=RUSTUP_VERSION):
    '''Download url for a particular version of the installer.'''
    ext = ''
    if 'windows' in host:
        ext = '.exe'
    return os.path.join(RUSTUP_URL_BASE,
        'archive/%(version)s/%(host)s/rustup-init%(ext)s') % {
                'version': version,
                'host': host,
                'ext': ext}

def rustup_hash(host):
    '''Look up the checksum for the given installer.'''
    return RUSTUP_HASHES.get(host, None)

def platform():
    '''Determine the appropriate rust platform string for the current host'''
    if sys.platform.startswith('darwin'):
        return 'x86_64-apple-darwin'
    elif sys.platform.startswith(('win32', 'msys')):
        # Bravely assume we'll be building 64-bit Firefox.
        return 'x86_64-pc-windows-msvc'
    elif sys.platform.startswith('linux'):
        return 'x86_64-unknown-linux-gnu'
    elif sys.platform.startswith('freebsd'):
        return 'x86_64-unknown-freebsd'

    return None
