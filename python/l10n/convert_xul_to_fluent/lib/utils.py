# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import os


def read_file(path, base_path=None):
    if base_path is not None:
        path = os.path.join(base_path, path)
        path = path.replace("\\", "/")
    with open(path) as fptr:
        return fptr.read()


def write_file(path, text, base_path=None, append=False):
    if base_path is not None:
        path = os.path.join(base_path, path)
        path = path.replace("\\", "/")
    mode = "a" if append and os.path.exists(path) else "w"
    with open(path, mode) as text_file:
        text_file.write(text)
