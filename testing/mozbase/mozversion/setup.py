# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from setuptools import setup

PACKAGE_VERSION = '2.0.0'


setup(name='mozversion',
      version=PACKAGE_VERSION,
      description='Library to get version information for applications',
      long_description="see https://firefox-source-docs.mozilla.org/mozbase/index.html",
      classifiers=['Programming Language :: Python :: 2.7',
                   'Programming Language :: Python :: 3'],
      keywords='mozilla',
      author='Mozilla Automation and Testing Team',
      author_email='tools@lists.mozilla.org',
      url='https://wiki.mozilla.org/Auto-tools/Projects/Mozbase',
      license='MPL',
      packages=['mozversion'],
      include_package_data=True,
      zip_safe=False,
      install_requires=['mozlog ~= 3.0',
                        'six >= 1.10.0'],
      entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      mozversion = mozversion:cli
      """)
