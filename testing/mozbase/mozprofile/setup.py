# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
from setuptools import setup, find_packages

PACKAGE_VERSION = '0.4'

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
      version=PACKAGE_VERSION,
      description="Handling of Mozilla Gecko based application profiles",
      long_description=description,
      classifiers=['Environment :: Console',
                   'Intended Audience :: Developers',
                   'License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)',
                   'Natural Language :: English',
                   'Operating System :: OS Independent',
                   'Programming Language :: Python',
                   'Topic :: Software Development :: Libraries :: Python Modules',
                   ],
      keywords='mozilla',
      author='Mozilla Automation and Tools team',
      author_email='tools@lists.mozilla.com',
      url='https://github.com/mozilla/mozbase/tree/master/mozprofile',
      license='MPL 2.0',
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
