# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup, find_packages
import sys

version = '0.5'

deps = ['httplib2 >= 0.7.3',
        'mozfile >= 1.1',
        'mozhttpd >= 0.7',
        'mozinfo >= 0.7',
        'mozinstall >= 1.9',
        'mozprocess >= 0.18',
        'mozprofile >= 0.21',
        'mozrunner >= 5.35',
       ]

# we only support python 2.6+ right now
assert sys.version_info[0] == 2
assert sys.version_info[1] >= 6

setup(name='tps',
      version=version,
      description='run automated multi-profile sync tests',
      long_description="""\
""",
      classifiers=[], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='',
      author='Mozilla Automation and Tools team',
      author_email='tools@lists.mozilla.org',
      url='https://developer.mozilla.org/en-US/docs/TPS',
      license='MPL 2.0',
      packages=find_packages(exclude=['ez_setup', 'examples', 'tests']),
      include_package_data=True,
      zip_safe=False,
      install_requires=deps,
      entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      runtps = tps.cli:main
      """,
      data_files=[
        ('tps', ['config/config.json.in']),
      ],
      )
