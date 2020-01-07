# Any copyright is dedicated to the Public Domain.
# https://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import

import os
import six
import sys

import mozunit

from mozfile import which

here = os.path.abspath(os.path.dirname(__file__))


def test_which(monkeypatch):
    cwd = os.path.join(here, 'files', 'which')
    monkeypatch.chdir(cwd)

    if sys.platform == "win32":
        if six.PY3:
            import winreg
        else:
            import _winreg as winreg
        bindir = os.path.join(cwd, "win")
        monkeypatch.setenv("PATH", bindir)
        monkeypatch.setattr(winreg, 'QueryValue', (lambda k, sk: None))

        assert which("foo.exe").lower() == os.path.join(bindir, "foo.exe").lower()
        assert which("foo").lower() == os.path.join(bindir, "foo.exe").lower()
        assert which("foo", exts=[".FOO", ".BAR"]).lower() == os.path.join(bindir, "foo").lower()
        assert os.environ.get("PATHEXT") != [".FOO", ".BAR"]
        assert which("foo.txt") is None

        assert which("bar").lower() == os.path.join(bindir, "bar").lower()
        assert which("baz").lower() == os.path.join(cwd, "baz.exe").lower()

        registered_dir = os.path.join(cwd, "registered")
        quux = os.path.join(registered_dir, "quux.exe").lower()

        def mock_registry(key, subkey):
            assert subkey == (
                r"SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\quux.exe")
            return quux

        monkeypatch.setattr(winreg, 'QueryValue', mock_registry)
        assert which("quux").lower() == quux
        assert which("quux.exe").lower() == quux

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
