# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup, find_packages

VERSION = '0.2'

setup(
    author='Mozilla Foundation',
    author_email='dev-builds@lists.mozilla.org',
    name='mozbuild',
    description='Mozilla build system functionality.',
    license='MPL 2.0',
    packages=find_packages(),
    version=VERSION,
    install_requires=[
        'jsmin',
        'mozfile',
    ],
    classifiers=[
        'Development Status :: 3 - Alpha',
        'Topic :: Software Development :: Build Tools',
        'License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: Implementation :: CPython',
    ],
    keywords='mozilla build',
)
