# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup

PACKAGE_VERSION = '0.2'

# dependencies
deps = ['mozinfo']
try:
    import json
except ImportError:
    deps.append('simplejson')

setup(name='moztest',
      version=PACKAGE_VERSION,
      description="Package for storing and outputting Mozilla test results",
      long_description="see http://mozbase.readthedocs.org/",
      classifiers=[], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='mozilla',
      author='Mozilla Automation and Tools team',
      author_email='tools@lists.mozilla.org',
      url='https://wiki.mozilla.org/Auto-tools/Projects/Mozbase',
      license='MPL',
      packages=['moztest'],
      include_package_data=True,
      zip_safe=False,
      install_requires=deps,
      )
