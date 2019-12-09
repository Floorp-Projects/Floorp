import os

import pytest

import virtualenv


@pytest.mark.skipif(os.environ.get("TOX_PACKAGE") is None, reason="needs tox provisioned sdist")
def test_sdist_contains(sdist):
    """make assertions on what we package"""
    content = set(sdist.iterdir())

    names = {i.name for i in content}

    must_have = {
        # sources
        "virtualenv.py",
        "virtualenv_embedded",
        "virtualenv_support",
        "setup.py",
        "setup.cfg",
        "MANIFEST.in",
        "pyproject.toml",
        # test files
        "tests",
        # documentation
        "docs",
        "README.rst",
        # helpers
        "tasks",
        "tox.ini",
        # meta-data
        "AUTHORS.txt",
        "LICENSE.txt",
    }

    missing = must_have - names
    assert not missing

    extra = names - must_have - {"PKG-INFO", "virtualenv.egg-info"}
    assert not extra, " | ".join(extra)


def test_wheel_contains(extracted_wheel):
    content = set(extracted_wheel.iterdir())

    names = {i.name for i in content}
    must_have = {
        # sources
        "virtualenv.py",
        "virtualenv_support",
        "virtualenv-{}.dist-info".format(virtualenv.__version__),
    }
    assert must_have == names

    support = {i.name for i in (extracted_wheel / "virtualenv_support").iterdir()}
    assert "__init__.py" in support
    for package in ("pip", "wheel", "setuptools"):
        assert any(package in i for i in support)

    meta = {i.name for i in (extracted_wheel / "virtualenv-{}.dist-info".format(virtualenv.__version__)).iterdir()}
    assert {"entry_points.txt", "WHEEL", "RECORD", "METADATA", "top_level.txt", "zip-safe", "LICENSE.txt"} == meta
