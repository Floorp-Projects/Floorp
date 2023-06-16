# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import hashlib

import mozpack.path as mozpath
from mozbuild.util import memoize
from mozversioncontrol import get_repository_object


@memoize
def hash_path(path):
    """Hash a single file.

    Returns the SHA-256 hash in hex form.
    """
    with open(path, mode="rb") as fh:
        return hashlib.sha256(fh.read()).hexdigest()


@memoize
def get_file_finder(base_path):
    from pathlib import Path

    repo = get_repository_object(base_path)
    if repo:
        files = repo.get_tracked_files_finder(base_path)
        if files:
            return files
        else:
            return None
    else:
        return get_repository_object(Path(base_path)).get_tracked_files_finder(
            base_path
        )


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
    if finder:
        for pattern in patterns:
            found = list(finder.find(pattern))
            if found:
                files.update(found)
            else:
                raise Exception("%s did not match anything" % pattern)
        for path in sorted(files.keys()):
            if path.endswith((".pyc", ".pyd", ".pyo")):
                continue
            h.update(
                "{} {}\n".format(
                    hash_path(mozpath.abspath(mozpath.join(base_path, path))),
                    mozpath.normsep(path),
                ).encode("utf-8")
            )

    return h.hexdigest()
