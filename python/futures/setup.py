#!/usr/bin/env python

extras = {}
try:
    from setuptools import setup
    extras['zip_safe'] = False
except ImportError:
    from distutils.core import setup

setup(name='futures',
      version='3.0.2',
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
