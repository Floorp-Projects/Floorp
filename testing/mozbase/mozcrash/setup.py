# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from setuptools import setup

PACKAGE_NAME = 'mozcrash'
PACKAGE_VERSION = '2.0.0'

# dependencies
deps = ['mozfile >= 1.0',
        'mozlog >= 6.0']

setup(name=PACKAGE_NAME,
      version=PACKAGE_VERSION,
      description="Library for printing stack traces from minidumps "
      "left behind by crashed processes",
      long_description="see https://firefox-source-docs.mozilla.org/mozbase/index.html",
      classifiers=['Programming Language :: Python :: 2.7',
                   'Programming Language :: Python :: 3.5'],
      # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='mozilla',
      author='Mozilla Automation and Tools team',
      author_email='tools@lists.mozilla.org',
      url='https://wiki.mozilla.org/Auto-tools/Projects/Mozbase',
      license='MPL',
      packages=['mozcrash'],
      include_package_data=True,
      zip_safe=False,
      install_requires=deps,
      )
