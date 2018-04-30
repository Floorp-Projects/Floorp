# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import sys
from setuptools import setup, find_packages

PACKAGE_NAME = 'mozrunner'
PACKAGE_VERSION = '7.0.1'

desc = """Reliable start/stop/configuration of Mozilla Applications (Firefox, Thunderbird, etc.)"""

deps = [
    'mozdevice>=1.*',
    'mozfile==1.*',
    'mozinfo>=0.7,<1',
    'mozlog==3.*',
    'mozprocess>=0.23,<1',
    'mozprofile>=1.1.0,<2',
    'six>=1.10.0,<2',
]

EXTRAS_REQUIRE = {'crash': ['mozcrash >= 1.0']}

# we only support python 2 right now
assert sys.version_info[0] == 2

setup(name=PACKAGE_NAME,
      version=PACKAGE_VERSION,
      description=desc,
      long_description="see https://firefox-source-docs.mozilla.org/mozbase/index.html",
      classifiers=['Environment :: Console',
                   'Intended Audience :: Developers',
                   'License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)',
                   'Natural Language :: English',
                   'Operating System :: OS Independent',
                   'Programming Language :: Python',
                   'Topic :: Software Development :: Libraries :: Python Modules',
                   ],
      keywords='mozilla',
      author='Mozilla Automation and Tools team',
      author_email='tools@lists.mozilla.org',
      url='https://wiki.mozilla.org/Auto-tools/Projects/Mozbase',
      license='MPL 2.0',
      packages=find_packages(),
      zip_safe=False,
      install_requires=deps,
      extras_require=EXTRAS_REQUIRE,
      entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      mozrunner = mozrunner:cli
      """)
