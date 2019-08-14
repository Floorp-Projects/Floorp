# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import
import os
import re
from setuptools import setup, find_packages

THIS_DIR = os.path.dirname(os.path.realpath(__name__))


def read(*parts):
    with open(os.path.join(THIS_DIR, *parts)) as f:
        return f.read()


def get_version():
    return re.findall("__version__ = '([\d\.]+)'",
                      read('firefox_ui_harness', '__init__.py'), re.M)[0]


long_description = """Custom Marionette runner classes and entry scripts for Firefox Desktop
specific Marionette tests.
"""

setup(name='firefox-ui-harness',
      version=get_version(),
      description="Firefox UI Harness",
      long_description=long_description,
      classifiers=['Programming Language :: Python :: 2.7',
                   'Programming Language :: Python :: 2 :: Only'],
      # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='mozilla',
      author='Auto-tools',
      author_email='tools-marionette@lists.mozilla.org',
      url='https://wiki.mozilla.org/Auto-tools/Projects/Marionette/Harnesses/FirefoxUI',
      license='MPL',
      packages=find_packages(),
      include_package_data=True,
      zip_safe=False,
      install_requires=read('requirements.txt').splitlines(),
      entry_points="""
        [console_scripts]
        firefox-ui-functional = firefox_ui_harness.cli_functional:cli
      """,
      )
