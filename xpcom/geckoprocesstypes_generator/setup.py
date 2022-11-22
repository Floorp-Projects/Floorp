# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import find_packages, setup

setup(
    name="geckoprocesstypes",
    version="1.0",
    description="Generator for GeckoProcessTypes related resources.",
    author="Mozilla Foundation",
    license="MPL 2.0",
    packages=find_packages(),
    install_requires=[],
    entry_points={"console_scripts": ["GeckoProcessTypes.py = geckoprocesstypes:main"]},
    keywords=["geckoprocesstypes"],
)
