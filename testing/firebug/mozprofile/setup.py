# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup, find_packages
import sys

version = '0.1a'

# we only support python 2 right now
assert sys.version_info[0] == 2

deps = []
# version-dependent dependencies
if sys.version_info[1] < 6:
    deps.append('simplejson')

setup(name='mozprofile',
      version=version,
      description="handling of Mozilla XUL app profiles",
      long_description="""\
""",
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
      addon_id = mozprofile:print_addon_ids
      """,
      )
