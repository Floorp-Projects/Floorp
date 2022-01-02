# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from setuptools import setup

PACKAGE_VERSION = "1.3.0"

setup(
    name="mozprocess",
    version=PACKAGE_VERSION,
    description="Mozilla-authored process handling",
    long_description="see https://firefox-source-docs.mozilla.org/mozbase/index.html",
    classifiers=[
        "Environment :: Console",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)",
        "Natural Language :: English",
        "Operating System :: OS Independent",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3.5",
        "Topic :: Software Development :: Libraries :: Python Modules",
    ],
    keywords="mozilla",
    author="Mozilla Automation and Tools team",
    author_email="tools@lists.mozilla.org",
    url="https://wiki.mozilla.org/Auto-tools/Projects/Mozbase",
    license="MPL 2.0",
    packages=["mozprocess"],
    include_package_data=True,
    zip_safe=False,
    install_requires=["mozinfo"],
    entry_points="""
      # -*- Entry points: -*-
      """,
)
