# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os

from mozpack.archive import create_tar_gz_from_files
from mozpack.files import FileFinder


def distribution_files(root):
    """Find all files suitable for distributing.

    Given the path to generated Sphinx documentation, returns an iterable
    of (path, BaseFile) for files that should be archived, uploaded, etc.
    Paths are relative to given root directory.
    """
    finder = FileFinder(root, ignore=('_staging', '_venv'))
    return finder.find('**')


def create_tarball(filename, root):
    """Create a tar.gz archive of docs in a directory."""
    files = dict(distribution_files(root))

    with open(filename, 'wb') as fh:
        create_tar_gz_from_files(fh, files, filename=os.path.basename(filename),
                                 compresslevel=6)
