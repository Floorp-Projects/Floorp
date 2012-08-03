# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup

VERSION = '0.1'

setup(
    name='mozbuild',
    description='Mozilla build system functionality.',
    license='MPL 2.0',
    packages=['mozbuild'],
    version=VERSION
)
