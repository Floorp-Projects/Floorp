# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os

from setuptools import setup

PACKAGE_VERSION = "1.0.0"

try:
    pwd = os.path.dirname(os.path.abspath(__file__))
    description = open(os.path.join(pwd, "README.rst")).read()
except Exception:
    description = ""

setup(
    name="mozsystemmonitor",
    description="Monitor system resource usage.",
    long_description="see https://firefox-source-docs.mozilla.org/mozbase/index.html",
    classifiers=[
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3.5",
    ],
    # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
    license="MPL 2.0",
    keywords="mozilla",
    author="Mozilla Automation and Tools Team",
    author_email="tools@lists.mozilla.org",
    url="https://wiki.mozilla.org/Auto-tools/Projects/Mozbase",
    packages=["mozsystemmonitor"],
    version=PACKAGE_VERSION,
    install_requires=["psutil >= 3.1.1"],
)
