# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals
from mozbuild.util import memoize
from mozpack.files import FileFinder
import mozpack.path as mozpath
import hashlib


@memoize
def _hash_path(path):
    with open(path) as fh:
        return hashlib.sha256(fh.read()).hexdigest()


def hash_paths(base_path, patterns):
    """
    Give a list of path patterns, return a digest of the contents of all
    the corresponding files, similarly to git tree objects or mercurial
    manifests.

    Each file is hashed. The list of all hashes and file paths is then
    itself hashed to produce the result.
    """
    finder = FileFinder(base_path)
    h = hashlib.sha256()
    files = {}
    for pattern in patterns:
        found = list(finder.find(pattern))
        if found:
            files.update(found)
        else:
            raise Exception('%s did not match anything' % pattern)
    for path in sorted(files.keys()):
        h.update('{} {}\n'.format(
            _hash_path(mozpath.abspath(mozpath.join(base_path, path))),
            mozpath.normsep(path)
        ))
    return h.hexdigest()
