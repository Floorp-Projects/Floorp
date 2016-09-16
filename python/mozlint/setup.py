# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup

VERSION = 0.1
DEPS = ["mozlog>=3.4"]

setup(
    name='mozlint',
    description='Framework for registering and running micro lints',
    license='MPL 2.0',
    author='Andrew Halberstadt',
    author_email='ahalberstadt@mozilla.com',
    url='',
    packages=['mozlint'],
    version=VERSION,
    classifiers=[
        'Environment :: Console',
        'Development Status :: 3 - Alpha',
        'License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)',
        'Natural Language :: English',
    ],
    install_requires=DEPS,
)
