# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import hashlib
import os
import shutil
import stat
import sys
import tempfile
from io import BytesIO
from urllib.request import urlopen

from mozpack.macpkg import Pbzx, uncpio, unxar


def unpack_sdk(url, sha512, extract_prefix, out_dir="."):
    if "MOZ_AUTOMATION" in os.environ:
        url = f"http://taskcluster/tooltool.mozilla-releng.net/sha512/{sha512}"
    with tempfile.TemporaryFile() as pkg:
        hash = hashlib.sha512()
        for attempt in range(3):
            if attempt != 0:
                print(f"Failed to download from {url}. Retrying", file=sys.stderr)

            with urlopen(url) as fh:
                # Equivalent to shutil.copyfileobj, but computes sha512 at the same time.
                while True:
                    buf = fh.read(1024 * 1024)
                    if not buf:
                        break
                    hash.update(buf)
                    pkg.write(buf)
            digest = hash.hexdigest()
            if digest == sha512:
                break
        else:
            raise Exception(f"(actual) {digest} != (expected) {sha512}")

        pkg.seek(0, os.SEEK_SET)

        for name, content in unxar(pkg):
            if name in ("Payload", "Content"):
                extract_payload(content, extract_prefix, out_dir)


def extract_payload(fileobj, extract_prefix, out_dir="."):
    hardlinks = {}
    for path, st, content in uncpio(Pbzx(fileobj)):
        if not path:
            continue
        path = path.decode()
        matches = path.startswith(extract_prefix)
        if matches:
            path = os.path.join(out_dir, path[len(extract_prefix) :].lstrip("/"))

        # When there are hardlinks, normally a cpio stream is supposed to
        # contain the data for all of them, but, even with compression, that
        # can be a waste of space, so in some cpio streams (*cough* *cough*,
        # Apple's, e.g. in Xcode), the files after the first one have dummy
        # data.
        # As we may be filtering the first file out (if it doesn't match
        # extract_prefix), we need to keep its data around (we're not going
        # to be able to rewind).
        if stat.S_ISREG(st.mode) and st.nlink > 1:
            key = (st.dev, st.ino)
            hardlink = hardlinks.get(key)
            if hardlink:
                hardlink[0] -= 1
                if hardlink[0] == 0:
                    del hardlinks[key]
                content = hardlink[1]
                if isinstance(content, BytesIO):
                    content.seek(0)
                    if matches:
                        hardlink[1] = path
            elif matches:
                hardlink = hardlinks[key] = [st.nlink - 1, path]
            else:
                hardlink = hardlinks[key] = [st.nlink - 1, BytesIO(content.read())]
                content = hardlink[1]

        if not matches:
            continue
        if stat.S_ISDIR(st.mode):
            os.makedirs(path, exist_ok=True)
        else:
            parent = os.path.dirname(path)
            if parent:
                os.makedirs(parent, exist_ok=True)

            if stat.S_ISLNK(st.mode):
                os.symlink(content.read(), path)
            elif stat.S_ISREG(st.mode):
                if isinstance(content, str):
                    os.link(content, path)
                else:
                    with open(path, "wb") as out:
                        shutil.copyfileobj(content, out)
            else:
                raise Exception(f"File mode {st.mode:o} is not supported")


if __name__ == "__main__":
    unpack_sdk(*sys.argv[1:])
