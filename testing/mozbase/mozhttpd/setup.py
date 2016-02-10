# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup

PACKAGE_VERSION = '0.7'
deps = ['moznetwork >= 0.24']

setup(name='mozhttpd',
      version=PACKAGE_VERSION,
      description="Python webserver intended for use with Mozilla testing",
      long_description="see http://mozbase.readthedocs.org/",
      classifiers=[], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='mozilla',
      author='Mozilla Automation and Testing Team',
      author_email='tools@lists.mozilla.org',
      url='https://wiki.mozilla.org/Auto-tools/Projects/Mozbase',
      license='MPL',
      packages=['mozhttpd'],
      include_package_data=True,
      zip_safe=False,
      install_requires=deps,
      entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      mozhttpd = mozhttpd:main
      """,
      )

