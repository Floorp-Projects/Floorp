# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup

PACKAGE_NAME = "manifestparser"
PACKAGE_VERSION = '0.6'

setup(name=PACKAGE_NAME,
      version=PACKAGE_VERSION,
      description="Library to create and manage test manifests",
      long_description="see http://mozbase.readthedocs.org/",
      classifiers=[], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='mozilla manifests',
      author='Mozilla Automation and Testing Team',
      author_email='tools@lists.mozilla.org',
      url='https://wiki.mozilla.org/Auto-tools/Projects/Mozbase',
      license='MPL',
      zip_safe=False,
      packages=['manifestparser'],
      install_requires=[],
      entry_points="""
      [console_scripts]
      manifestparser = manifestparser.manifestparser:main
      """,
     )
