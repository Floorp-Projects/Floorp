# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import hashlib
import os
import shutil
import stat
import sys
import tempfile
from urllib.request import urlopen

from mozpack.macpkg import Pbzx, uncpio, unxar


def unpack_sdk(url, sha256, extract_prefix, out_dir="."):
    with tempfile.TemporaryFile() as pkg:
        hash = hashlib.sha256()
        with urlopen(url) as fh:
            # Equivalent to shutil.copyfileobj, but computes sha256 at the same time.
            while True:
                buf = fh.read(1024 * 1024)
                if not buf:
                    break
                hash.update(buf)
                pkg.write(buf)
        digest = hash.hexdigest()
        if digest != sha256:
            raise Exception(f"(actual) {digest} != (expected) {sha256}")

        pkg.seek(0, os.SEEK_SET)

        for name, content in unxar(pkg):
            if name == "Payload":
                extract_payload(content, extract_prefix, out_dir)


def extract_payload(fileobj, extract_prefix, out_dir="."):
    for path, mode, content in uncpio(Pbzx(fileobj)):
        if not path:
            continue
        path = path.decode()
        if not path.startswith(extract_prefix):
            continue
        path = os.path.join(out_dir, path[len(extract_prefix) :].lstrip("/"))
        if stat.S_ISDIR(mode):
            os.makedirs(path, exist_ok=True)
        else:
            parent = os.path.dirname(path)
            if parent:
                os.makedirs(parent, exist_ok=True)

            if stat.S_ISLNK(mode):
                os.symlink(content.read(), path)
            elif stat.S_ISREG(mode):
                with open(path, "wb") as out:
                    shutil.copyfileobj(content, out)
            else:
                raise Exception(f"File mode {mode:o} is not supported")


if __name__ == "__main__":
    unpack_sdk(*sys.argv[1:])
