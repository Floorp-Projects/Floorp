# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup

PACKAGE_VERSION = '0.7'

# dependencies
deps = ['mozfile >= 0.3',
        'mozlog']

setup(name='mozcrash',
      version=PACKAGE_VERSION,
      description="Library for printing stack traces from minidumps left behind by crashed processes",
      long_description="see http://mozbase.readthedocs.org/",
      classifiers=[], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='mozilla',
      author='Mozilla Automation and Tools team',
      author_email='tools@lists.mozilla.org',
      url='https://wiki.mozilla.org/Auto-tools/Projects/MozBase',
      license='MPL',
      packages=['mozcrash'],
      include_package_data=True,
      zip_safe=False,
      install_requires=deps,
      )
