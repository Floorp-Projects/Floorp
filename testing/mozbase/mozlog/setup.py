# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from setuptools import setup, find_packages

PACKAGE_NAME = 'mozlog'
PACKAGE_VERSION = '3.6'

setup(name=PACKAGE_NAME,
      version=PACKAGE_VERSION,
      description="Robust log handling specialized for logging in the Mozilla universe",
      long_description="see https://firefox-source-docs.mozilla.org/mozbase/index.html",
      author='Mozilla Automation and Testing Team',
      author_email='tools@lists.mozilla.org',
      url='https://wiki.mozilla.org/Auto-tools/Projects/Mozbase',
      license='MPL 1.1/GPL 2.0/LGPL 2.1',
      packages=find_packages(),
      zip_safe=False,
      install_requires=['blessings >= 1.3',
                        'six >= 1.10.0'],
      tests_require=['mozfile'],
      platforms=['Any'],
      classifiers=['Development Status :: 4 - Beta',
                   'Environment :: Console',
                   'Intended Audience :: Developers',
                   'License :: OSI Approved :: Mozilla Public License 1.1 (MPL 1.1)',
                   'Operating System :: OS Independent',
                   'Programming Language :: Python :: 2.7',
                   'Programming Language :: Python :: 3',
                   'Topic :: Software Development :: Libraries :: Python Modules'],
      package_data={"mozlog": ["formatters/html/main.js",
                               "formatters/html/style.css"]},
      entry_points={
          "console_scripts": [
              "structlog = mozlog.scripts:main"
          ],
          'pytest11': [
              'mozlog = mozlog.pytest_mozlog.plugin',
          ]}
      )
