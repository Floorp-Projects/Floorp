# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""Tests for the FileAvoidWrite object."""

import locale
import pathlib

import pytest
from mozunit import main

from mozbuild.util import FileAvoidWrite


@pytest.fixture
def tmp_path(tmpdir):
    """Backport of the tmp_path fixture from pytest 3.9.1."""
    return pathlib.Path(str(tmpdir))


def test_overwrite_contents(tmp_path):
    file = tmp_path / "file.txt"
    file.write_text("abc")

    faw = FileAvoidWrite(str(file))
    faw.write("bazqux")

    assert faw.close() == (True, True)
    assert file.read_text() == "bazqux"


def test_store_new_contents(tmp_path):
    file = tmp_path / "file.txt"

    faw = FileAvoidWrite(str(file))
    faw.write("content")

    assert faw.close() == (False, True)
    assert file.read_text() == "content"


def test_change_binary_file_contents(tmp_path):
    file = tmp_path / "file.dat"
    file.write_bytes(b"\0")

    faw = FileAvoidWrite(str(file), readmode="rb")
    faw.write(b"\0\0\0")

    assert faw.close() == (True, True)
    assert file.read_bytes() == b"\0\0\0"


def test_obj_as_context_manager(tmp_path):
    file = tmp_path / "file.txt"

    with FileAvoidWrite(str(file)) as fh:
        fh.write("foobar")

    assert file.read_text() == "foobar"


def test_no_write_happens_if_file_contents_same(tmp_path):
    file = tmp_path / "file.txt"
    file.write_text("content")
    original_write_time = file.stat().st_mtime

    faw = FileAvoidWrite(str(file))
    faw.write("content")

    assert faw.close() == (True, False)
    assert file.stat().st_mtime == original_write_time


def test_diff_not_created_by_default(tmp_path):
    file = tmp_path / "file.txt"
    faw = FileAvoidWrite(str(file))
    faw.write("dummy")
    faw.close()
    assert faw.diff is None


def test_diff_update(tmp_path):
    file = tmp_path / "diffable.txt"
    file.write_text("old")

    faw = FileAvoidWrite(str(file), capture_diff=True)
    faw.write("new")
    faw.close()

    diff = "\n".join(faw.diff)
    assert "-old" in diff
    assert "+new" in diff


@pytest.mark.skipif(
    locale.getdefaultlocale()[1] == "cp1252",
    reason="Fails on win32 terminals with cp1252 encoding",
)
def test_write_unicode(tmp_path):
    # Unicode grinning face :D
    binary_emoji = b"\xf0\x9f\x98\x80"

    file = tmp_path / "file.dat"
    faw = FileAvoidWrite(str(file))
    faw.write(binary_emoji)
    faw.close()


if __name__ == "__main__":
    main()
