
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from setuptools import setup

PACKAGE_NAME = 'mozpower'
PACKAGE_VERSION = '1.1.2'

deps = ['mozlog >= 6.0', 'mozdevice >= 4.0.0,<5']

setup(name=PACKAGE_NAME,
      version=PACKAGE_VERSION,
      description="Mozilla-authored power usage measurement tools",
      long_description="see https://firefox-source-docs.mozilla.org/mozbase/index.html",
      classifiers=['Programming Language :: Python :: 2.7',
                   'Programming Language :: Python :: 3.5'],
      # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='',
      author='Mozilla Performance Test Engineering Team',
      author_email='tools@lists.mozilla.org',
      url='https://wiki.mozilla.org/Auto-tools/Projects/Mozbase',
      license='MPL',
      packages=['mozpower'],
      include_package_data=True,
      zip_safe=False,
      install_requires=deps,
      entry_points="""
      # -*- Entry points: -*-
      """,
      )
