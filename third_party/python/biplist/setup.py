#!/usr/bin/env python

try:
    from setuptools import setup, find_packages
except ImportError:
    import ez_setup
    ez_setup.use_setuptools()
    from setuptools import setup, find_packages

import os
import sys

major, minor, micro, releaselevel, serial = sys.version_info

if major <= 1 or (major == 2 and minor < 6) or (major == 3 and minor < 4):
    # N.B.: Haven't tested with older py3k versions.
    print('This module supports Python 2 >= 2.6 and Python 3 >= 3.4.')
    sys.exit(1)

author = 'Andrew Wooster'
email = 'andrew@planetaryscale.com'
version = '1.0.3'
desc = 'biplist is a library for reading/writing binary plists.'

setup(
    name = 'biplist',
    version = version,
    url = 'https://bitbucket.org/wooster/biplist',
    download_url = 'https://bitbucket.org/wooster/biplist/downloads/biplist-%s.tar.gz' % version,
    license = 'BSD',
    description = desc,
    long_description = 
    """`biplist` is a binary plist parser/generator for Python.

Binary Property List (plist) files provide a faster and smaller serialization
format for property lists on OS X. This is a library for generating binary
plists which can be read by OS X, iOS, or other clients.

This module requires Python 2.6 or higher or Python 3.4 or higher.""",
    author = author,
    author_email = email,
    packages = find_packages(),
    include_package_data = True,
    zip_safe = False,
    classifiers = [
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: BSD License',
        'Operating System :: OS Independent',
        'Programming Language :: Python',
        "Topic :: Software Development :: Libraries :: Python Modules",
        "Topic :: Text Processing :: Markup",
    ],
    test_suite = 'nose.collector',
    install_requires = [],
    requires = [],
    tests_require = ['nose', 'coverage'],
)
