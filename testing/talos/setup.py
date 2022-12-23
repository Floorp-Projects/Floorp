# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from setuptools import find_packages, setup

try:
    here = os.path.dirname(os.path.abspath(__file__))
    description = open(os.path.join(here, "README")).read()
except OSError:
    description = ""

version = "0.0"

with open(os.path.join(here, "requirements.txt")) as f:
    dependencies = f.read().splitlines()

dependency_links = []

setup(
    name="talos",
    version=version,
    description="Performance testing framework for Windows, Mac and Linux.",
    long_description=description,
    classifiers=[
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 2 :: Only",
    ],
    # Get strings from http://www.python.org/pypi?%3Aaction=list_classifiers
    author="Mozilla Foundation",
    author_email="tools@lists.mozilla.org",
    url="https://wiki.mozilla.org/Buildbot/Talos",
    license="MPL",
    packages=find_packages(exclude=["ez_setup", "examples", "tests"]),
    include_package_data=True,
    package_data={
        "": [
            "*.config",
            "*.css",
            "*.gif",
            "*.htm",
            "*.html",
            "*.ico",
            "*.js",
            "*.json",
            "*.manifest",
            "*.php",
            "*.png",
            "*.rdf",
            "*.sqlite",
            "*.svg",
            "*.xml",
            "*.xul",
        ]
    },
    zip_safe=False,
    install_requires=dependencies,
    dependency_links=dependency_links,
    entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      talos = talos.run_tests:main
      talos-results = talos.results:main
      """,
    test_suite="tests",
)
