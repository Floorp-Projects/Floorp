# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from distutils.core import setup

VERSION = '0.1'

setup(
    name='mozboot',
    description='System bootstrap for building Mozilla projects.',
    license='MPL 2.0',
    packages=['mozboot'],
    version=VERSION,
    scripts=['bin/bootstrap.py'],
)
