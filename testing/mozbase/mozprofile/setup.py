# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
from setuptools import setup, find_packages

version = '0.1'

# we only support python 2 right now
assert sys.version_info[0] == 2

deps = ["ManifestDestiny >= 0.5.4"]
# version-dependent dependencies
try:
    import json
except ImportError:
    deps.append('simplejson')
try:
    import sqlite3
except ImportError:
    deps.append('pysqlite')


# take description from README
here = os.path.dirname(os.path.abspath(__file__))
try:
    description = file(os.path.join(here, 'README.md')).read()
except (OSError, IOError):
    description = ''

setup(name='mozprofile',
      version=version,
      description="handling of Mozilla XUL app profiles",
      long_description=description,
      classifiers=[], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='',
      author='Mozilla Automation + Testing Team',
      author_email='mozmill-dev@googlegroups.com',
      url='http://github.com/mozautomation/mozmill',
      license='MPL',
      packages=find_packages(exclude=['ez_setup', 'examples', 'tests']),
      include_package_data=True,
      zip_safe=False,
      install_requires=deps,
      entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      mozprofile = mozprofile:cli
      """,
      )
