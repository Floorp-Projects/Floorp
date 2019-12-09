from __future__ import absolute_import, unicode_literals

import os
import pipes
import subprocess
import sys
import textwrap

import pytest

import virtualenv

try:
    from pathlib import Path
except ImportError:
    from pathlib2 import Path

ROOT_DIR = Path(__file__).parents[1]


@pytest.fixture(scope="session")
def clean_python(tmp_path_factory):
    path = tmp_path_factory.mktemp("activation-test-env")
    prev_cwd = os.getcwd()
    try:
        os.chdir(str(path))
        home_dir, _, __, bin_dir = virtualenv.path_locations(str(path / "env"))
        virtualenv.create_environment(home_dir, no_pip=True, no_setuptools=True, no_wheel=True)

        site_packages = subprocess.check_output(
            [
                os.path.join(bin_dir, virtualenv.EXPECTED_EXE),
                "-c",
                "from distutils.sysconfig import get_python_lib; print(get_python_lib())",
            ],
            universal_newlines=True,
        ).strip()

        pydoc_test = path.__class__(site_packages) / "pydoc_test.py"
        pydoc_test.write_text('"""This is pydoc_test.py"""')
    finally:
        os.chdir(str(prev_cwd))

    yield home_dir, bin_dir, pydoc_test


@pytest.fixture()
def sdist(tmp_path):
    """make assertions on what we package"""
    import tarfile

    path = os.environ.get("TOX_PACKAGE")
    if path is not None:
        dest_path = tmp_path / "sdist"
        dest_path.mkdir()
        prev = os.getcwd()
        try:
            os.chdir(str(dest_path))
            tar = tarfile.open(path, "r:gz")
            tar.extractall()
            return next(dest_path.iterdir())
        finally:
            os.chdir(prev)
    return None


@pytest.fixture(scope="session")
def wheel(tmp_path_factory):
    """test that we can create a virtual environment by feeding to a clean python the wheels content"""
    dest_path = tmp_path_factory.mktemp("wheel")
    env = os.environ.copy()
    try:
        subprocess.check_output(
            [sys.executable, "-m", "pip", "wheel", "-w", str(dest_path), "--no-deps", str(ROOT_DIR)],
            universal_newlines=True,
            stderr=subprocess.STDOUT,
            env=env,
        )
    except subprocess.CalledProcessError as exception:
        assert not exception.returncode, exception.output

    wheels = list(dest_path.glob("*.whl"))
    assert len(wheels) == 1
    wheel = wheels[0]
    return wheel


@pytest.fixture()
def extracted_wheel(tmp_path, wheel):
    dest_path = tmp_path / "wheel-extracted"

    import zipfile

    with zipfile.ZipFile(str(wheel), "r") as zip_ref:
        zip_ref.extractall(str(dest_path))
    return dest_path


def _call(cmd, env=None, stdin=None, allow_fail=False, shell=False, **kwargs):
    env = os.environ if env is None else env
    process = subprocess.Popen(
        cmd,
        universal_newlines=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        stdin=subprocess.PIPE,
        env=env,
        shell=shell,
        **kwargs
    )
    out, err = process.communicate(input=stdin)
    if allow_fail is False:
        msg = textwrap.dedent(
            """
        cmd:
        {}
        out:
        {}
        err:
        {}
        env:
        {}
        """
        ).format(
            cmd if shell else " ".join(pipes.quote(str(i)) for i in cmd),
            out,
            err,
            os.linesep.join("{}={!r}".format(k, v) for k, v in env.items()),
        )
        msg = msg.lstrip()
        assert process.returncode == 0, msg
        return out, err
    else:
        return process.returncode, out, err


@pytest.fixture(scope="session")
def call_subprocess():
    return _call
