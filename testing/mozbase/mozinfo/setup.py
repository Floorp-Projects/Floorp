# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.


import os
from setuptools import setup, find_packages

PACKAGE_VERSION = '0.3.3'

# get documentation from the README
try:
    here = os.path.dirname(os.path.abspath(__file__))
    description = file(os.path.join(here, 'README.md')).read()
except (OSError, IOError):
    description = ''

# dependencies
deps = []
try:
    import json
except ImportError:
    deps = ['simplejson']

setup(name='mozinfo',
      version=PACKAGE_VERSION,
      description="file for interface to transform introspected system information to a format pallatable to Mozilla",
      long_description=description,
      classifiers=[], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='mozilla',
      author='Jeff Hammel',
      author_email='jhammel@mozilla.com',
      url='https://wiki.mozilla.org/Auto-tools',
      license='MPL',
      packages=find_packages(exclude=['legacy']),
      include_package_data=True,
      zip_safe=False,
      install_requires=deps,
      entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      mozinfo = mozinfo:main
      """,
      )
