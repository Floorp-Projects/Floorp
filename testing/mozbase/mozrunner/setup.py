# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from setuptools import setup, find_packages

PACKAGE_NAME = 'mozrunner'
PACKAGE_VERSION = '6.3'

desc = """Reliable start/stop/configuration of Mozilla Applications (Firefox, Thunderbird, etc.)"""

deps = ['mozcrash >= 0.11',
        'mozdevice >= 0.37',
        'mozfile >= 1.0',
        'mozinfo >= 0.7',
        'mozlog >= 1.5',
        'mozprocess >= 0.17',
        'mozprofile >= 0.18',
       ]

# we only support python 2 right now
assert sys.version_info[0] == 2

setup(name=PACKAGE_NAME,
      version=PACKAGE_VERSION,
      description=desc,
      long_description="see http://mozbase.readthedocs.org/",
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
      package_data={'mozrunner': [
            'resources/metrotestharness.exe'
      ]},
      zip_safe=False,
      install_requires = deps,
      entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      mozrunner = mozrunner:cli
      """,
    )
