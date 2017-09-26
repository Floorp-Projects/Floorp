# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from setuptools import setup

PACKAGE_NAME = "manifestparser"
PACKAGE_VERSION = '1.2'

setup(name=PACKAGE_NAME,
      version=PACKAGE_VERSION,
      description="Library to create and manage test manifests",
      long_description="see http://mozbase.readthedocs.org/",
      classifiers=['Programming Language :: Python :: 2.7',
                   'Programming Language :: Python :: 2 :: Only'],
                  # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='mozilla manifests',
      author='Mozilla Automation and Testing Team',
      author_email='tools@lists.mozilla.org',
      url='https://wiki.mozilla.org/Auto-tools/Projects/Mozbase',
      license='MPL',
      zip_safe=False,
      packages=['manifestparser'],
      install_requires=['six >= 1.10.0'],
      entry_points="""
      [console_scripts]
      manifestparser = manifestparser.cli:main
      """,
      )
