# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import stat
import sys

from mozpack.macpkg import Pbzx, uncpio, unxar


def unpkg(pkg_path, out_dir="."):
    with open(pkg_path, "rb") as pkg:
        for name, content in unxar(pkg):
            if name == "Payload":
                extract_payload(content, out_dir)


def extract_payload(fileobj, out_dir="."):
    for path, mode, content in uncpio(Pbzx(fileobj)):
        if not path:
            continue
        path = os.path.join(out_dir, path.decode())
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
    unpkg(*sys.argv[1:])
