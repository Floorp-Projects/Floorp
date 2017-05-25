#!/usr/bin/env python
from warnings import warn
import sys

if sys.version_info[0] > 2:
    warn('This backport is meant only for Python 2.\n'
         'Python 3 users do not need it, as the concurrent.futures '
         'package is available in the standard library.')

extras = {}
try:
    from setuptools import setup
    extras['zip_safe'] = False
except ImportError:
    from distutils.core import setup

setup(name='futures',
      version='3.0.5',
      description='Backport of the concurrent.futures package from Python 3.2',
      author='Brian Quinlan',
      author_email='brian@sweetapp.com',
      maintainer='Alex Gronholm',
      maintainer_email='alex.gronholm+pypi@nextday.fi',
      url='https://github.com/agronholm/pythonfutures',
      packages=['concurrent', 'concurrent.futures'],
      license='BSD',
      classifiers=['License :: OSI Approved :: BSD License',
                   'Development Status :: 5 - Production/Stable',
                   'Intended Audience :: Developers',
                   'Programming Language :: Python :: 2.6',
                   'Programming Language :: Python :: 2.7',
                   'Programming Language :: Python :: 2 :: Only'],
      **extras
      )
