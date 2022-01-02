# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from setuptools import setup

PACKAGE_VERSION = "0.7.1"
deps = ["moznetwork >= 0.24", "mozinfo >= 1.0.0", "six >= 1.13.0"]

setup(
    name="mozhttpd",
    version=PACKAGE_VERSION,
    description="Python webserver intended for use with Mozilla testing",
    long_description="see https://firefox-source-docs.mozilla.org/mozbase/index.html",
    classifiers=[
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 2 :: Only",
    ],
    # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
    keywords="mozilla",
    author="Mozilla Automation and Testing Team",
    author_email="tools@lists.mozilla.org",
    url="https://wiki.mozilla.org/Auto-tools/Projects/Mozbase",
    license="MPL",
    packages=["mozhttpd"],
    include_package_data=True,
    zip_safe=False,
    install_requires=deps,
    entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      mozhttpd = mozhttpd:main
      """,
)
