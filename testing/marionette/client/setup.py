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
        r'__version__ = "([\d\.]+)"', read("marionette_driver", "__init__.py"), re.M
    )[0]


setup(
    name="marionette_driver",
    version=get_version(),
    description="Marionette Driver",
    long_description="""Note marionette_driver is no longer supported.

For more information see https://firefox-source-docs.mozilla.org/python/marionette_driver.html""",
    # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
    classifiers=[
        "Development Status :: 7 - Inactive",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)",
        "Operating System :: MacOS :: MacOS X",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX",
        "Topic :: Software Development :: Quality Assurance",
        "Topic :: Software Development :: Testing",
        "Topic :: Utilities",
        "Programming Language :: Python",
        "Programming Language :: Python :: 2.7",
    ],
    keywords="mozilla",
    author="Auto-tools",
    author_email="dev-webdriver@mozilla.org",
    url="https://wiki.mozilla.org/Auto-tools/Projects/Marionette",
    license="MPL",
    packages=find_packages(),
    include_package_data=True,
    zip_safe=False,
    install_requires=read("requirements.txt").splitlines(),
)
