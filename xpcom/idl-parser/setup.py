# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import find_packages, setup

setup(
    name="xpidl",
    version="1.0",
    description="Parser and header generator for xpidl files.",
    author="Mozilla Foundation",
    license="MPL 2.0",
    packages=find_packages(),
    install_requires=["ply>=3.6,<4.0"],
    url="https://github.com/pelmers/xpidl",
    entry_points={"console_scripts": ["header.py = xpidl.header:main"]},
    keywords=["xpidl", "parser"],
)
