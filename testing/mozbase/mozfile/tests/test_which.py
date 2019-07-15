# Any copyright is dedicated to the Public Domain.
# https://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import

import os
import sys

import mozunit

from mozfile import which

here = os.path.abspath(os.path.dirname(__file__))


def test_which(monkeypatch):
    cwd = os.path.join(here, 'files', 'which')
    monkeypatch.chdir(cwd)

    if sys.platform == "win32":
        bindir = os.path.join(cwd, "win")
        monkeypatch.setenv("PATH", bindir)

        assert which("foo.exe").lower() == os.path.join(bindir, "foo.exe").lower()
        assert which("foo").lower() == os.path.join(bindir, "foo.exe").lower()
        assert which("foo", exts=[".FOO", ".BAR"]).lower() == os.path.join(bindir, "foo").lower()
        assert os.environ.get("PATHEXT") != [".FOO", ".BAR"]
        assert which("foo.txt") is None

        assert which("bar").lower() == os.path.join(bindir, "bar").lower()
        assert which("baz").lower() == os.path.join(cwd, "baz.exe").lower()

    else:
        bindir = os.path.join(cwd, "unix")
        monkeypatch.setenv("PATH", bindir)
        assert which("foo") == os.path.join(bindir, "foo")
        assert which("baz") is None
        assert which("baz", exts=[".EXE"]) is None
        assert "PATHEXT" not in os.environ
        assert which("file") is None


if __name__ == '__main__':
    mozunit.main()
