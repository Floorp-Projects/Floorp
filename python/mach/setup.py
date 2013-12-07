# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup

VERSION = '0.1'

setup(
    name='mach',
    description='CLI frontend to mozilla-central.',
    license='MPL 2.0',
    packages=['mach'],
    version=VERSION,
    tests_require=['mock']
)

