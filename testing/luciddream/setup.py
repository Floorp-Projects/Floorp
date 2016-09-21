#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
#

import os
from setuptools import setup, find_packages

try:
    here = os.path.dirname(os.path.abspath(__file__))
    description = file(os.path.join(here, 'README.md')).read()
except IOError:
    description = ''

version = '0.1'

dependencies = open('requirements.txt', 'r').read().splitlines()

setup(
    name='luciddream',
    version=version,
    description='''
    Luciddream is a test harness for running tests between
    a Firefox browser and another device, such as a Firefox OS
    emulator.
    ''',
      long_description=description,
    classifiers=[], # Get strings from http://www.python.org/pypi?%3Aaction=list_classifiers
    author='Ted Mielczarek',
    author_email='ted@mielczarek.org',
    url='',
    license='MPL 2.0',
    packages=find_packages(exclude=['ez_setup', 'examples', 'tests']),
    include_package_data=True,
    package_data={'': ['*.js', '*.css', '*.html', '*.txt', '*.xpi', '*.rdf', '*.xul', '*.jsm', '*.xml'],},
    zip_safe=False,
    install_requires=dependencies,
    entry_points = {
        'console_scripts': ['runluciddream=luciddream.runluciddream:main'],
    }
  )
