#!/usr/bin/env python

from setuptools import setup

setup(name='fluent',
      version='0.4.2',
      description='Localization library for expressive translations.',
      author='Mozilla',
      author_email='l10n-drivers@mozilla.org',
      license='APL 2',
      url='https://github.com/projectfluent/python-fluent',
      keywords=['fluent', 'localization', 'l10n'],
      classifiers=[
          'Development Status :: 3 - Alpha',
          'Intended Audience :: Developers',
          'License :: OSI Approved :: Apache Software License',
          'Programming Language :: Python :: 2.7',
          'Programming Language :: Python :: 3.5',
      ],
      packages=['fluent', 'fluent.syntax', 'fluent.migrate'],
      package_data={
          'fluent.migrate': ['cldr_data/*']
      }
      )
