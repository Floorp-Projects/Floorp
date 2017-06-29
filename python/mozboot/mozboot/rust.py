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
# Use the no-CNAME host for compatibilty with Python 2.7
# which doesn't support SNI.
RUSTUP_URL_BASE = 'https://static-rust-lang-org.s3.amazonaws.com/rustup'

# Pull this to get the lastest stable version number.
RUSTUP_MANIFEST = os.path.join(RUSTUP_URL_BASE, 'release-stable.toml')

# We bake in a known version number so we can verify a checksum.
RUSTUP_VERSION = '1.5.0'

# SHA-256 checksums of the installers, per platform.
RUSTUP_HASHES = {
    'x86_64-unknown-freebsd':
        '0fd08eaf4cb2935a195e4bab2352a4506e986dc65e3ba6c696d374081f9b56cb',
    'x86_64-apple-darwin':
        '29884ee3b2b896fd5ac0ecee624d726667974a4a6df01a66a7ab345325d623a5',
    'x86_64-unknown-linux-gnu':
        '7b5ce33a881992b285e2aa6cbc785da4138c5bab7c8c9b55c06918bfb1ba0efa',
    'x86_64-pc-windows-msvc':
        '7500b705855c4f8e70e222ad1192bb46de15629f73a6459a98c9fd4bd06136bc',
}

NO_PLATFORM = '''
Sorry, we have no installer configured for your platform.

Please try installing rust for your system from https://rustup.rs/
or from https://rust-lang.org/ or from your package manager.
'''

def rustup_url(host, version=RUSTUP_VERSION):
    '''Download url for a particular version of the installer.'''
    return '%(base)s/archive/%(version)s/%(host)s/rustup-init%(ext)s' % {
                'base': RUSTUP_URL_BASE,
                'version': version,
                'host': host,
                'ext': exe_suffix(host)}

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

def exe_suffix(host=None):
    if not host:
        host = platform()
    if 'windows' in host:
        return '.exe'
    return ''

USAGE = '''
python rust.py [--update]

Pass the --update option print info for the latest release of rustup-init.

When invoked without the --update option, it queries the latest version
and verifies the current stored checksums against the distribution server,
but doesn't update the version installed by `mach bootstrap`.
'''

def unquote(s):
    '''Strip outer quotation marks from a string.'''
    return s.strip("'").strip('"')

def rustup_latest_version():
    '''Query the latest version of the rustup installer.'''
    import urllib2
    f = urllib2.urlopen(RUSTUP_MANIFEST)
    # The manifest is toml, but we might not have the toml4 python module
    # available, so use ad-hoc parsing to obtain the current release version.
    #
    # The manifest looks like:
    #
    # schema-version = '1'
    # version = '0.6.5'
    #
    for line in f:
        key, value = map(str.strip, line.split('=', 2))
        if key == 'schema-version':
            schema = int(unquote(value))
            if schema != 1:
                print('ERROR: Unknown manifest schema %s' % value)
                sys.exit(1)
        elif key == 'version':
            return unquote(value)
    return None

def http_download_and_hash(url):
    import hashlib
    import requests
    h = hashlib.sha256()
    r = requests.get(url, stream=True)
    for data in r.iter_content(4096):
        h.update(data)
    return h.hexdigest()

def make_checksums(version, validate=False):
    hashes = []
    for platform in RUSTUP_HASHES.keys():
        if validate:
            print('Checking %s... ' % platform, end='')
        else:
            print('Fetching %s... ' % platform, end='')
        checksum = http_download_and_hash(rustup_url(platform, version))
        if validate and checksum != rustup_hash(platform):
            print('mismatch:\n  script: %s\n  server: %s' % (
                RUSTUP_HASHES[platform], checksum))
        else:
            print('OK')
        hashes.append((platform, checksum))
    return hashes

if __name__ == '__main__':
    '''Allow invoking the module as a utility to update checksums.'''

    # Unbuffer stdout so our two-part 'Checking...' messages print correctly
    # even if there's network delay.
    sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)

    # Hook the requests module from the greater source tree. We can't import
    # this at the module level since we might be imported into the bootstrap
    # script in standalone mode.
    #
    # This module is necessary for correct https certificate verification.
    mod_path = os.path.dirname(__file__)
    sys.path.insert(0, os.path.join(mod_path, '..', '..', 'requests'))

    update = False
    if len(sys.argv) > 1:
        if sys.argv[1] == '--update':
            update = True
        else:
            print(USAGE)
            sys.exit(1)

    print('Checking latest installer version... ', end='')
    version = rustup_latest_version()
    if not version:
        print('ERROR: Could not query current rustup installer version.')
        sys.exit(1)
    print(version)

    if version == RUSTUP_VERSION:
        print("We're up to date. Validating checksums.")
        make_checksums(version, validate=True)
        exit()

    if not update:
        print('Out of date. We use %s. Validating checksums.' % RUSTUP_VERSION)
        make_checksums(RUSTUP_VERSION, validate=True)
        exit()

    print('Out of date. We use %s. Calculating checksums.' % RUSTUP_VERSION)
    hashes = make_checksums(version)
    print('')
    print("RUSTUP_VERSION = '%s'" % version)
    print("RUSTUP_HASHES = {")
    for item in hashes:
        print("    '%s':\n        '%s'," % item)
    print("}")
