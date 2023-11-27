# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from setuptools import setup, find_packages

try:
    here = os.path.dirname(os.path.abspath(__file__))
    description = open(os.path.join(here, "README.txt")).read()
except IOError:
    description = ""

import mozharness

version = mozharness.version_string

dependencies = [
    "virtualenv",
    "mock",
    "coverage",
    "nose",
    "pylint",
    "pyflakes",
    "toml",
]
try:
    import json
except ImportError:
    dependencies.append("simplejson")

setup(
    name="mozharness",
    version=version,
    description="Mozharness is a configuration-driven script harness with full logging that allows production infrastructure and individual developers to use the same scripts. ",
    long_description=description,
    classifiers=[
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 2 :: Only",
    ],  # Get strings from http://www.python.org/pypi?%3Aaction=list_classifiers
    author="Aki Sasaki",
    author_email="aki@mozilla.com",
    url="https://hg.mozilla.org/build/mozharness/",
    license="MPL",
    packages=find_packages(exclude=["ez_setup", "examples", "tests"]),
    include_package_data=True,
    zip_safe=False,
    install_requires=dependencies,
    entry_points="""
      # -*- Entry points: -*-
      """,
)
