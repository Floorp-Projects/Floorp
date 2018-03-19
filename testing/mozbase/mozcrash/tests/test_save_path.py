#!/usr/bin/env python

from __future__ import absolute_import

import os

import mozunit
import pytest

from conftest import fspath


def test_save_path_not_present(check_for_crashes, minidump_files, tmpdir):
    """Test that dump_save_path works when the directory doesn't exist."""
    save_path = tmpdir.join("saved")

    assert 1 == check_for_crashes(dump_save_path=fspath(save_path))

    assert save_path.join(minidump_files[0]["dmp"].basename).check()
    assert save_path.join(minidump_files[0]["extra"].basename).check()


def test_save_path(check_for_crashes, minidump_files, tmpdir):
    """Test that dump_save_path works."""
    save_path = tmpdir.mkdir("saved")

    assert 1 == check_for_crashes(dump_save_path=fspath(save_path))

    assert save_path.join(minidump_files[0]["dmp"].basename).check()
    assert save_path.join(minidump_files[0]["extra"].basename).check()


def test_save_path_isfile(check_for_crashes, minidump_files, tmpdir):
    """Test that dump_save_path works when the path is a file and not a directory."""
    save_path = tmpdir.join("saved")
    save_path.write("junk")

    assert 1 == check_for_crashes(dump_save_path=fspath(save_path))

    assert save_path.join(minidump_files[0]["dmp"].basename).check()
    assert save_path.join(minidump_files[0]["extra"].basename).check()


def test_save_path_envvar(check_for_crashes, minidump_files, tmpdir):
    """Test that the MINDUMP_SAVE_PATH environment variable works."""
    save_path = tmpdir.mkdir("saved")

    os.environ['MINIDUMP_SAVE_PATH'] = fspath(save_path)
    try:
        assert 1 == check_for_crashes(dump_save_path=None)
    finally:
        del os.environ['MINIDUMP_SAVE_PATH']

    assert save_path.join(minidump_files[0]["dmp"].basename).check()
    assert save_path.join(minidump_files[0]["extra"].basename).check()


@pytest.mark.parametrize('minidump_files', [3], indirect=True)
def test_save_multiple(check_for_crashes, minidump_files, tmpdir):
    """Test that all minidumps are saved."""
    save_path = tmpdir.mkdir("saved")

    assert 3 == check_for_crashes(dump_save_path=fspath(save_path))

    for i in range(3):
        assert save_path.join(minidump_files[i]["dmp"].basename).check()
        assert save_path.join(minidump_files[i]["extra"].basename).check()


if __name__ == '__main__':
    mozunit.main()
