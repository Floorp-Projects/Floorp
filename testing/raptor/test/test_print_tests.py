import os

import mozunit
import pytest

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))
from raptor import cmdline


def test_pageload_subtests(capsys, monkeypatch, tmpdir):
    # cmdline.py is hard-coded to use raptor.toml from the same directory so we need
    # to monkey patch os.dirname, which is not ideal. If we could make --print-tests
    # respect the --test path that would be much better.
    def mock(path):
        return str(tmpdir)

    monkeypatch.setattr(os.path, "dirname", mock)
    manifest = tmpdir.join("raptor.toml")
    manifest.write(
        """
[DEFAULT]
type = "pageload"
apps = "firefox"

["raptor-subtest-1"]
measure = ["foo", "bar"]

["raptor-subtest-2"]
"""
    )
    with pytest.raises(SystemExit):
        cmdline.parse_args(["--print-tests"])
    captured = capsys.readouterr()
    assert (
        captured.out
        == """
Raptor Tests Available for Firefox Desktop
==========================================

raptor
  type: pageload
  subtests:
    raptor-subtest-1 (foo, bar)
    raptor-subtest-2

Done.
"""
    )


if __name__ == "__main__":
    mozunit.main()
