# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from setuptools import setup

PACKAGE_VERSION = "1.1.0"

deps = [
    "mozinfo",
    "mozlog >= 6.0",
]

setup(
    name="moznetwork",
    version=PACKAGE_VERSION,
    description="Library of network utilities for use in Mozilla testing",
    long_description="see https://firefox-source-docs.mozilla.org/mozbase/index.html",
    classifiers=[
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.5",
        "Development Status :: 5 - Production/Stable",
    ],
    # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
    keywords="mozilla",
    author="Mozilla Automation and Tools team",
    author_email="tools@lists.mozilla.org",
    url="https://wiki.mozilla.org/Auto-tools/Projects/Mozbase",
    license="MPL",
    packages=["moznetwork"],
    include_package_data=True,
    zip_safe=False,
    install_requires=deps,
    entry_points={"console_scripts": ["moznetwork = moznetwork:cli"]},
)
