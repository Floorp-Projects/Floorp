# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from setuptools import setup, find_packages

version = '0.3'

deps = ['mozinfo == 0.3.3', 'mozprofile == 0.4',
        'mozprocess == 0.4', 'mozrunner == 5.8', 'mozregression == 0.6.3',
        'mozautolog == 0.2.4', 'mozautoeslib == 0.1.1']

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
      author='Jonathan Griffin',
      author_email='jgriffin@mozilla.com',
      url='http://hg.mozilla.org/services/services-central',
      license='MPL',
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
