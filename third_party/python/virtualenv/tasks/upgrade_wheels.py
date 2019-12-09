"""
Helper script to rebuild virtualenv_support. Downloads the wheel files using pip
"""
from __future__ import absolute_import, unicode_literals

import glob
import os
import subprocess


def virtualenv_support_path():
    return os.path.join(os.path.dirname(__file__), "..", "virtualenv_support")


def collect_wheels():
    for filename in glob.glob(os.path.join(virtualenv_support_path(), "*.whl")):
        name, version = os.path.basename(filename).split("-")[:2]
        yield filename, name, version


def remove_wheel_files():
    old_versions = {}
    for filename, name, version in collect_wheels():
        old_versions[name] = version
        os.remove(filename)
    return old_versions


def download(package):
    subprocess.call(["pip", "download", "-d", virtualenv_support_path(), package])


def run():
    old = remove_wheel_files()
    for package in ("pip", "wheel", "setuptools"):
        download(package)
    new = {name: version for _, name, version in collect_wheels()}

    changes = []
    for package, version in old.items():
        if new[package] != version:
            changes.append((package, version, new[package]))

    print("\n".join(" * upgrade {} from {} to {}".format(p, o, n) for p, o, n in changes))


if __name__ == "__main__":
    run()
