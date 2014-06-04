# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup

PACKAGE_VERSION = '0.5'

dependencies = ['mozdevice >= 0.29',
                'mozfile >= 1.0',
                'mozlog >= 1.5']

setup(name='mozversion',
      version=PACKAGE_VERSION,
      description='Library to get version information for applications',
      long_description='See http://mozbase.readthedocs.org',
      classifiers=[],
      keywords='mozilla',
      author='Mozilla Automation and Testing Team',
      author_email='tools@lists.mozilla.org',
      url='https://wiki.mozilla.org/Auto-tools/Projects/Mozbase',
      license='MPL',
      packages=['mozversion'],
      include_package_data=True,
      zip_safe=False,
      install_requires=dependencies,
      entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      mozversion = mozversion:cli
      """)
