#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The real details are in manifestparser.py; this is just a front-end
# BUT use this file when you want to distribute to python!
# otherwise setuptools will complain that it can't find setup.py
# and result in a useless package

from setuptools import setup, find_packages
import sys
import os

here = os.path.dirname(os.path.abspath(__file__))
try:
    filename = os.path.join(here, 'README.txt')
    description = file(filename).read()
except:
    description = ''

PACKAGE_NAME = "ManifestDestiny"
PACKAGE_VERSION = "0.5.4"

setup(name=PACKAGE_NAME,
      version=PACKAGE_VERSION,
      description="Universal manifests for Mozilla test harnesses",
      long_description=description,
      classifiers=[], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='mozilla manifests',
      author='Jeff Hammel',
      author_email='jhammel@mozilla.com',
      url='https://github.com/mozilla/mozbase/tree/master/manifestdestiny',
      license='MPL',
      zip_safe=False,
      packages=find_packages(exclude=['legacy']),
      install_requires=[
      # -*- Extra requirements: -*-
      ],
      entry_points="""
      [console_scripts]
      manifestparser = manifestparser:main
      """,
     )
