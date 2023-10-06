# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import hashlib
import os
from pathlib import Path
from urllib.request import urlopen

import certifi
from mach.site import PythonVirtualenv
from mach.util import get_state_dir

MINIMUM_RUST_VERSION = "1.70.0"


def get_tools_dir(srcdir=False):
    if os.environ.get("MOZ_AUTOMATION") and "MOZ_FETCHES_DIR" in os.environ:
        return os.environ["MOZ_FETCHES_DIR"]
    return get_state_dir(srcdir)


def get_mach_virtualenv_root():
    return Path(get_state_dir(specific_to_topsrcdir=True)) / "_virtualenvs" / "mach"


def get_mach_virtualenv_binary():
    root = get_mach_virtualenv_root()
    return Path(PythonVirtualenv(str(root)).python_path)


def http_download_and_save(url, dest: Path, hexhash, digest="sha256"):
    """Download the given url and save it to dest.  hexhash is a checksum
    that will be used to validate the downloaded file using the given
    digest algorithm.  The value of digest can be any value accepted by
    hashlib.new.  The default digest used is 'sha256'."""
    f = urlopen(url, cafile=certifi.where())
    h = hashlib.new(digest)
    with open(dest, "wb") as out:
        while True:
            data = f.read(4096)
            if data:
                out.write(data)
                h.update(data)
            else:
                break
    if h.hexdigest() != hexhash:
        dest.unlink()
        raise ValueError("Hash of downloaded file does not match expected hash")
