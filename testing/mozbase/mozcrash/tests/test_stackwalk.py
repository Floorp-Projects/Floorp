#!/usr/bin/env python
# coding=UTF-8

import os

import mozunit
from conftest import fspath


def test_stackwalk_not_found(check_for_crashes, minidump_files, tmpdir, capsys):
    """Test that check_for_crashes can handle unicode in dump_directory."""
    stackwalk = tmpdir.join("stackwalk")

    assert 1 == check_for_crashes(stackwalk_binary=fspath(stackwalk), quiet=False)

    out, _ = capsys.readouterr()
    assert "MINIDUMP_STACKWALK binary not found" in out


def test_stackwalk_envvar(check_for_crashes, minidump_files, stackwalk):
    """Test that check_for_crashes uses the MINIDUMP_STACKWALK environment var."""
    os.environ["MINIDUMP_STACKWALK"] = fspath(stackwalk)
    try:
        assert 1 == check_for_crashes(stackwalk_binary=None)
    finally:
        del os.environ["MINIDUMP_STACKWALK"]


def test_stackwalk_unicode(check_for_crashes, minidump_files, tmpdir, capsys):
    """Test that check_for_crashes can handle unicode in dump_directory."""
    stackwalk = tmpdir.mkdir("🍪").join("stackwalk")
    stackwalk.write("fake binary")
    stackwalk.chmod(0o744)

    assert 1 == check_for_crashes(stackwalk_binary=fspath(stackwalk), quiet=False)

    out, err = capsys.readouterr()
    assert fspath(stackwalk) in out


if __name__ == "__main__":
    mozunit.main()
