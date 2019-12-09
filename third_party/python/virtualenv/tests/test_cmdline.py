from __future__ import absolute_import, unicode_literals

import os
import subprocess
import sys

import pytest

import virtualenv


def test_commandline_basic(tmpdir):
    """Simple command line usage should work and files should be generated"""
    home_dir, lib_dir, inc_dir, bin_dir = virtualenv.path_locations(str(tmpdir.join("venv")))
    subprocess.check_call([sys.executable, "-m", "virtualenv", "-vvv", home_dir, "--no-download"])

    assert os.path.exists(home_dir)
    assert os.path.exists(bin_dir)

    assert os.path.exists(os.path.join(bin_dir, "activate"))
    assert os.path.exists(os.path.join(bin_dir, "activate_this.py"))
    assert os.path.exists(os.path.join(bin_dir, "activate.ps1"))

    exe = os.path.join(bin_dir, os.path.basename(sys.executable))
    assert os.path.exists(exe)

    def _check_no_warnings(module):
        subprocess.check_call((exe, "-Werror", "-c", "import {}".format(module)))

    _check_no_warnings("distutils")


def test_commandline_os_path_sep(tmp_path):
    path = tmp_path / "bad{}0".format(os.pathsep)
    assert not path.exists()
    process = subprocess.Popen(
        [sys.executable, "-m", "virtualenv", str(path)],
        cwd=str(tmp_path),
        universal_newlines=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        stdin=subprocess.PIPE,
    )
    out, err = process.communicate()
    assert process.returncode == 3
    msg = (
        "ERROR: target path contains the operating system path separator '{}'\n"
        "This is not allowed as would make the activation scripts unusable.\n".format(os.pathsep)
    )
    assert not err
    assert out == msg
    assert not path.exists()


def test_commandline_explicit_interp(tmpdir):
    """Specifying the Python interpreter should work"""
    subprocess.check_call([sys.executable, "-m", "virtualenv", "-p", sys.executable, str(tmpdir.join("venv"))])


# The registry lookups to support the abbreviated "-p 3.5" form of specifying
# a Python interpreter on Windows don't seem to work with Python 3.5. The
# registry layout is not well documented, and it's not clear that the feature
# is sufficiently widely used to be worth fixing.
# See https://github.com/pypa/virtualenv/issues/864
@pytest.mark.skipif("sys.platform == 'win32' and sys.version_info[:1] >= (3,)")
def test_commandline_abbrev_interp(tmpdir):
    """Specifying abbreviated forms of the Python interpreter should work"""
    abbrev = "{}{}.{}".format("" if sys.platform == "win32" else "python", *sys.version_info[0:2])
    subprocess.check_call([sys.executable, "-m", "virtualenv", "-p", abbrev, str(tmpdir.join("venv"))])
