# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from setuptools import setup

VERSION = '1.0.0'
DEPS = ['six >= 1.10.0']

setup(
    name='mozterm',
    description='Terminal abstractions built around the blessings module.',
    license='MPL 2.0',
    author='Andrew Halberstadt',
    author_email='ahalberstadt@mozilla.com',
    url='',
    packages=['mozterm'],
    version=VERSION,
    classifiers=[
        'Environment :: Console',
        'Development Status :: 3 - Alpha',
        'License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)',
        'Natural Language :: English',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.5',
    ],
    install_requires=DEPS,
)
