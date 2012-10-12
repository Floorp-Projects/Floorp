# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
from setuptools import setup

PACKAGE_NAME = "mozrunner"
PACKAGE_VERSION = '5.13'

desc = """Reliable start/stop/configuration of Mozilla Applications (Firefox, Thunderbird, etc.)"""
# take description from README
here = os.path.dirname(os.path.abspath(__file__))
try:
    description = file(os.path.join(here, 'README.md')).read()
except (OSError, IOError):
    description = ''

deps = ['mozinfo == 0.4',
        'mozprocess == 0.7',
        'mozprofile == 0.4',
       ]

# we only support python 2 right now
assert sys.version_info[0] == 2

setup(name=PACKAGE_NAME,
      version=PACKAGE_VERSION,
      description=desc,
      long_description=description,
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
      url='https://wiki.mozilla.org/Auto-tools/Projects/MozBase',
      license='MPL 2.0',
      packages=['mozrunner'],
      zip_safe=False,
      install_requires = deps,
      entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      mozrunner = mozrunner:cli
      """,
    )
