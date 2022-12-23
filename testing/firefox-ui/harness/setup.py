# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import re

from setuptools import find_packages, setup

THIS_DIR = os.path.dirname(os.path.realpath(__name__))


def read(*parts):
    with open(os.path.join(THIS_DIR, *parts)) as f:
        return f.read()


def get_version():
    return re.findall(
        '__version__ = "([\d\.]+)"', read("firefox_ui_harness", "__init__.py"), re.M
    )[0]


long_description = """Custom Marionette runner classes and entry scripts for Firefox Desktop
specific Marionette tests.
"""

setup(
    name="firefox-ui-harness",
    version=get_version(),
    description="Firefox UI Harness",
    long_description=long_description,
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.5",
        "License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)",
    ],
    # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
    keywords="mozilla",
    author="DevTools",
    author_email="dev-webdriver@mozilla.org",
    license="MPL",
    packages=find_packages(),
    include_package_data=True,
    zip_safe=False,
    install_requires=read("requirements.txt").splitlines(),
    entry_points="""
        [console_scripts]
        firefox-ui-functional = firefox_ui_harness.cli_functional:cli
      """,
)
