# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import hashlib
import io
import os
import six
import subprocess

from mozbuild.util import memoize
import mozpack.path as mozpath
from mozversioncontrol import get_repository_object, NonexistentFile


@memoize
def hash_path(path):
    """Hash a single file.

    Returns the SHA-256 hash in hex form.
    """
    with io.open(path, mode="rb") as fh:
        return hashlib.sha256(fh.read()).hexdigest()


@memoize
def hash_path_as_of_base_revision(base_path, path):
    repo = get_repository(base_path)
    base_revision = get_base_revision(base_path)
    return hashlib.sha256(
        repo.get_file_content(path, revision=base_revision)
    ).hexdigest()


@memoize
def get_repository(base_path):
    return get_repository_object(base_path)


@memoize
def get_file_finder(base_path):
    return get_repository(base_path).get_tracked_files_finder()


@memoize
def get_dirty(base_path):
    repo = get_repository(base_path)
    return set(repo.get_outgoing_files()) | set(repo.get_changed_files())


@memoize
def get_base_revision(base_path):
    return get_repository(base_path).base_ref


def hash_paths(base_path, patterns):
    """
    Give a list of path patterns, return a digest of the contents of all
    the corresponding files, similarly to git tree objects or mercurial
    manifests.

    Each file is hashed. The list of all hashes and file paths is then
    itself hashed to produce the result.
    """
    finder = get_file_finder(base_path)
    h = hashlib.sha256()
    files = {}
    for pattern in patterns:
        found = list(finder.find(pattern))
        if found:
            files.update(found)
        else:
            raise Exception("%s did not match anything" % pattern)

    dirty = set()
    include_local_changes = os.environ.get("MACH_ARTIFACT_INCLUDE_LOCAL_CHANGES", False)
    try:
        dirty = get_dirty(base_path)
    except subprocess.CalledProcessError:
        include_local_changes = True

    for path in sorted(files.keys()):
        path_hash = None
        # If the file is dirty, and we haven't been asked specifically to include
        # local changes, read the file contents from the VCS directly. Otherwise, read
        # the contents from the filesystem.
        if not include_local_changes and path in dirty:
            try:
                path_hash = hash_path_as_of_base_revision(base_path, path)
            except NonexistentFile:
                # If the file didn't exist at the base revision, that's fine,
                # just continue to the next file.
                continue
        if path_hash is None:
            path_hash = hash_path(mozpath.abspath(mozpath.join(base_path, path)))
        h.update(six.ensure_binary("{} {}\n".format(path_hash, mozpath.normsep(path))))

    return h.hexdigest()
