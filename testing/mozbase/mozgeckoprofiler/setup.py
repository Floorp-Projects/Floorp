# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from setuptools import setup

PACKAGE_NAME = "mozgeckoprofiler"
PACKAGE_VERSION = "1.0.0"

setup(
    name=PACKAGE_NAME,
    version=PACKAGE_VERSION,
    description="Library to generate and view performance data in the Firefox Profiler",
    long_description="see https://firefox-source-docs.mozilla.org/mozgeckoprofiler/index.html",
    classifiers=[
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.5",
        "License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)",
    ],
    keywords="mozilla",
    author="Mozilla Automation and Tools team",
    author_email="tools@lists.mozilla.org",
    url="https://wiki.mozilla.org/Auto-tools/Projects/Mozbase",
    license="MPL",
    packages=["mozgeckoprofiler"],
    include_package_data=True,
    zip_safe=False,
    install_requires=[],
    tests_require=[],
)
