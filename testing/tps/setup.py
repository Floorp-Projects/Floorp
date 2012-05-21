# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from setuptools import setup, find_packages

version = '0.2.40'

deps = ['pulsebuildmonitor >= 0.62', 'MozillaPulse == 0.61', 
        'mozinfo == 0.3.1', 'mozprofile == 0.1t',
        'mozprocess == 0.1a', 'mozrunner == 3.0a', 'mozregression == 0.3',
        'mozautolog >= 0.2.1']

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
      dependency_links = [
         "http://people.mozilla.org/~jgriffin/packages/"
      ],
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
