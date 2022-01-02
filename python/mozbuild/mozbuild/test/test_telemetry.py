# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import, print_function

import os

import buildconfig
import mozunit
from mozbuild.telemetry import filter_args


TELEMETRY_LOAD_ERROR = """
Error loading telemetry. mach output:
=========================================================
%s
=========================================================
"""


def test_path_filtering():
    srcdir_path = os.path.join(buildconfig.topsrcdir, "a")
    srcdir_path_2 = os.path.join(buildconfig.topsrcdir, "a/b/c")
    objdir_path = os.path.join(buildconfig.topobjdir, "x")
    objdir_path_2 = os.path.join(buildconfig.topobjdir, "x/y/z")
    home_path = os.path.join(os.path.expanduser("~"), "something_in_home")
    other_path = "/other/path"
    args = filter_args(
        "pass",
        [
            "python",
            "-c",
            "pass",
            srcdir_path,
            srcdir_path_2,
            objdir_path,
            objdir_path_2,
            home_path,
            other_path,
        ],
        buildconfig.topsrcdir,
        buildconfig.topobjdir,
        cwd=buildconfig.topsrcdir,
    )

    expected = [
        "a",
        "a/b/c",
        "$topobjdir/x",
        "$topobjdir/x/y/z",
        "$HOME/something_in_home",
        "<path omitted>",
    ]
    assert args == expected


def test_path_filtering_in_objdir():
    srcdir_path = os.path.join(buildconfig.topsrcdir, "a")
    srcdir_path_2 = os.path.join(buildconfig.topsrcdir, "a/b/c")
    objdir_path = os.path.join(buildconfig.topobjdir, "x")
    objdir_path_2 = os.path.join(buildconfig.topobjdir, "x/y/z")
    other_path = "/other/path"
    args = filter_args(
        "pass",
        [
            "python",
            "-c",
            "pass",
            srcdir_path,
            srcdir_path_2,
            objdir_path,
            objdir_path_2,
            other_path,
        ],
        buildconfig.topsrcdir,
        buildconfig.topobjdir,
        cwd=buildconfig.topobjdir,
    )
    expected = ["$topsrcdir/a", "$topsrcdir/a/b/c", "x", "x/y/z", "<path omitted>"]
    assert args == expected


def test_path_filtering_other_cwd(tmpdir):
    srcdir_path = os.path.join(buildconfig.topsrcdir, "a")
    srcdir_path_2 = os.path.join(buildconfig.topsrcdir, "a/b/c")
    other_path = str(tmpdir.join("other"))
    args = filter_args(
        "pass",
        ["python", "-c", "pass", srcdir_path, srcdir_path_2, other_path],
        buildconfig.topsrcdir,
        buildconfig.topobjdir,
        cwd=str(tmpdir),
    )
    expected = [
        "$topsrcdir/a",
        "$topsrcdir/a/b/c",
        # cwd-relative paths should be relativized
        "other",
    ]
    assert args == expected


if __name__ == "__main__":
    mozunit.main()
