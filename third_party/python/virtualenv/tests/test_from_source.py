"""test using the project from source/package rather than install"""
from __future__ import absolute_import, unicode_literals

import os

import pytest

import virtualenv

try:
    from pathlib import Path
except ImportError:
    from pathlib2 import Path

ROOT_DIR = Path(__file__).parents[1]


def test_use_from_source_tree(tmp_path, clean_python, monkeypatch, call_subprocess):
    """test that we can create a virtual environment by feeding to a clean python the wheels content"""
    monkeypatch.chdir(tmp_path)
    call_subprocess(
        [
            str(Path(clean_python[1]) / virtualenv.EXPECTED_EXE),
            str(Path(ROOT_DIR) / "virtualenv.py"),
            "--no-download",
            "env",
        ]
    )


@pytest.mark.skipif(os.environ.get("TOX_PACKAGE") is None, reason="needs tox provisioned sdist")
def test_use_from_source_sdist(sdist, tmp_path, clean_python, monkeypatch, call_subprocess):
    """test that we can create a virtual environment by feeding to a clean python the sdist content"""
    virtualenv_file = sdist / "virtualenv.py"
    assert virtualenv_file.exists()

    monkeypatch.chdir(tmp_path)
    call_subprocess(
        [str(Path(clean_python[1]) / virtualenv.EXPECTED_EXE), str(virtualenv_file), "--no-download", "env"]
    )


def test_use_from_wheel(tmp_path, extracted_wheel, clean_python, monkeypatch, call_subprocess):
    """test that we can create a virtual environment by feeding to a clean python the wheels content"""
    virtualenv_file = extracted_wheel / "virtualenv.py"
    monkeypatch.chdir(tmp_path)
    call_subprocess(
        [str(Path(clean_python[1]) / virtualenv.EXPECTED_EXE), str(virtualenv_file), "--no-download", "env"]
    )
