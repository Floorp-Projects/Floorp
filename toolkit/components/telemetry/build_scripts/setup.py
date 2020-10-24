# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from setuptools import setup, find_packages

VERSION = '1.0.0'

with open("README.md", "r") as fh:
    long_description = fh.read()

setup(
    author='Mozilla Telemetry Team',
    author_email='telemetry-client-dev@mozilla.com',
    url=('https://firefox-source-docs.mozilla.org/'
         'toolkit/components/telemetry/telemetry/collection/index.html'),
    name='mozparsers',
    description='Shared parsers for the Telemetry probe regitries.',
    long_description=long_description,
    long_description_content_type="text/markdown",
    license='MPL 2.0',
    packages=find_packages(),
    version=VERSION,
    classifiers=[
        'Topic :: Software Development :: Build Tools',
        'License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)',
        'Programming Language :: Python :: 2.7',
    ],
    keywords=['mozilla', 'telemetry', 'parsers'],
)
