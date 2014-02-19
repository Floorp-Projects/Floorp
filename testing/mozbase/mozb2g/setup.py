# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup

PACKAGE_VERSION = '0.3'

deps = ['mozdevice >= 0.16', 'marionette_client >= 0.5.2']

setup(name='mozb2g',
      version=PACKAGE_VERSION,
      description="B2G specific code for device automation",
      long_description="see http://mozbase.readthedocs.org/",
      classifiers=[], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='',
      author='Mozilla Automation and Testing Team',
      author_email='tools@lists.mozilla.org',
      url='https://wiki.mozilla.org/Auto-tools/Projects/Mozbase',
      license='MPL',
      packages=['mozb2g'],
      include_package_data=True,
      zip_safe=False,
      install_requires=deps
      )
