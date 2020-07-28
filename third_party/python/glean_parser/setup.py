#!/usr/bin/env python
# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""The setup script."""

import sys

from setuptools import setup, find_packages


if sys.version_info < (3, 5):
    print("glean_parser requires at least Python 3.5", file=sys.stderr)
    sys.exit(1)


with open("README.rst") as readme_file:
    readme = readme_file.read()

with open("HISTORY.rst") as history_file:
    history = history_file.read()

requirements = [
    "appdirs>=1.4.3",
    "Click>=7.0",
    "diskcache>=4.0.0",
    "iso8601>=0.1.12",
    "Jinja2>=2.10.1,<3.0",
    "jsonschema>=3.0.2",
    # 'markupsafe' is required by Jinja2. From version 2.0.0 on
    # py3.5 support is dropped.
    "markupsafe>=1.1,<2.0.0",
    "pep487==1.0.1",
    "PyYAML>=3.13",
    "yamllint>=1.18.0",
    # 'zipp' is required by jsonschema->importlib_metadata,
    # it drops py3.5 in newer versions.
    "zipp>=0.5,<2.0",
]

setup_requirements = ["pytest-runner", "setuptools-scm"]

test_requirements = [
    "pytest",
]

setup(
    author="Michael Droettboom",
    author_email="mdroettboom@mozilla.com",
    classifiers=[
        "Development Status :: 2 - Pre-Alpha",
        "Intended Audience :: Developers",
        "Natural Language :: English",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
    ],
    description="Parser tools for Mozilla's Glean telemetry",
    entry_points={"console_scripts": ["glean_parser=glean_parser.__main__:main",],},
    install_requires=requirements,
    long_description=readme + "\n\n" + history,
    include_package_data=True,
    keywords="glean_parser",
    name="glean_parser",
    packages=find_packages(include=["glean_parser"]),
    setup_requires=setup_requirements,
    test_suite="tests",
    tests_require=test_requirements,
    url="https://github.com/mozilla/glean_parser",
    zip_safe=False,
    use_scm_version=True,
)
