#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This script provides one-line bootstrap support to configure systems to build
# the tree.
#
# The role of this script is to load the Python modules containing actual
# bootstrap support. It does this through various means, including fetching
# content from the upstream source repository.

# If we add unicode_literals, optparse breaks on Python 2.6.1 (which is needed
# to support OS X 10.6).
from __future__ import print_function

import os
import shutil
import sys
import tempfile
try:
    from urllib2 import urlopen
except ImportError:
    from urllib.request import urlopen

from optparse import OptionParser

# The next two variables define where in the repository the Python files
# reside. This is used to remotely download file content when it isn't
# available locally.
REPOSITORY_PATH_PREFIX = 'python/mozboot'

REPOSITORY_PATHS = [
    'mozboot/__init__.py',
    'mozboot/base.py',
    'mozboot/bootstrap.py',
    'mozboot/centos.py',
    'mozboot/debian.py',
    'mozboot/fedora.py',
    'mozboot/freebsd.py',
    'mozboot/gentoo.py',
    'mozboot/openbsd.py',
    'mozboot/osx.py',
    'mozboot/ubuntu.py',
]

TEMPDIR = None

def fetch_files(repo_url, repo_type):
    repo_url = repo_url.rstrip('/')

    files = {}

    if repo_type == 'hgweb':
        for path in REPOSITORY_PATHS:
            url = repo_url + '/raw-file/default/python/mozboot/' + path

            req = urlopen(url=url, timeout=30)
            files[path] = req.read()
    else:
        raise NotImplementedError('Not sure how to handle repo type.', repo_type)

    return files

def ensure_environment(repo_url=None, repo_type=None):
    """Ensure we can load the Python modules necessary to perform bootstrap."""

    try:
        from mozboot.bootstrap import Bootstrapper
        return Bootstrapper
    except ImportError:
        # The first fallback is to assume we are running from a tree checkout
        # and have the files in a sibling directory.
        pardir = os.path.join(os.path.dirname(__file__), os.path.pardir)
        include = os.path.normpath(pardir)

        sys.path.append(include)
        try:
            from mozboot.bootstrap import Bootstrapper
            return Bootstrapper
        except ImportError:
            sys.path.pop()

            # The next fallback is to download the files from the source
            # repository.
            files = fetch_files(repo_url, repo_type)

            # Install them into a temporary location. They will be deleted
            # after this script has finished executing.
            global TEMPDIR
            TEMPDIR = tempfile.mkdtemp()

            for relpath in files.keys():
                destpath = os.path.join(TEMPDIR, relpath)
                destdir = os.path.dirname(destpath)

                if not os.path.exists(destdir):
                    os.makedirs(destdir)

                with open(destpath, 'wb') as fh:
                    fh.write(files[relpath])

            # This should always work.
            sys.path.append(TEMPDIR)
            from mozboot.bootstrap import Bootstrapper
            return Bootstrapper

def main(args):
    parser = OptionParser()
    parser.add_option('-r', '--repo-url', dest='repo_url',
        default='https://hg.mozilla.org/mozilla-central/',
        help='Base URL of source control repository where bootstrap files can '
             'be downloaded.')

    parser.add_option('--repo-type', dest='repo_type',
        default='hgweb',
        help='The type of the repository. This defines how we fetch file '
             'content. Like --repo, you should not need to set this.')

    options, leftover = parser.parse_args(args)

    try:
        try:
            cls = ensure_environment(options.repo_url, options.repo_type)
        except Exception as e:
            print('Could not load the bootstrap Python environment.\n')
            print('This should never happen. Consider filing a bug.\n')
            print('\n')
            print(e)
            return 1

        dasboot = cls()
        dasboot.bootstrap()

        return 0
    finally:
        if TEMPDIR is not None:
            shutil.rmtree(TEMPDIR)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
