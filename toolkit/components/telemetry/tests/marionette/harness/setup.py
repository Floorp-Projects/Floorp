# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from setuptools import setup, find_packages

PACKAGE_VERSION = '0.1'

THIS_DIR = os.path.dirname(os.path.realpath(__name__))


def read(*parts):
    with open(os.path.join(THIS_DIR, *parts)) as f:
        return f.read()

setup(name='telemetry-harness',
      version=PACKAGE_VERSION,
      description=("""Custom Marionette runner classes and entry scripts for Telemetry
specific Marionette tests."""),
      classifiers=[
          'Environment :: Console',
          'Intended Audience :: Developers',
          'License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)',
          'Natural Language :: English',
          'Operating System :: OS Independent',
          'Programming Language :: Python',
          'Topic :: Software Development :: Libraries :: Python Modules',
      ],
      keywords='mozilla',
      author='Firefox Test Engineering Team',
      author_email='firefox-test-engineering@mozilla.org',
      url='https://developer.mozilla.org/en-US/docs/Mozilla/QA/telemetry_harness',
      license='MPL 2.0',
      packages=find_packages(),
      zip_safe=False,
      install_requires=read('requirements.txt').splitlines(),
      include_package_data=True,
      entry_points="""
        [console_scripts]
        telemetry-harness = telemetry_harness.runtests:cli
    """)
