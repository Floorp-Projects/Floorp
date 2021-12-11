#!/usr/bin/env python

from setuptools import setup

setup(
    name='fluent.migrate',
    version='0.11',
    description='Toolchain to migrate legacy translation to Fluent.',
    author='Mozilla',
    author_email='l10n-drivers@mozilla.org',
    license='APL 2',
    url='https://hg.mozilla.org/l10n/fluent-migration/',
    keywords=['fluent', 'localization', 'l10n'],
    classifiers=[
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: Apache Software License',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.7',
    ],
    packages=['fluent', 'fluent.migrate'],
    install_requires=[
        'compare-locales >=8.1, <9.0',
        'fluent.syntax >=0.18.0, <0.19',
        'six',
    ],
    extras_require={
        'hg': ['python-hglib',],
    },
    tests_require=[
        'mock',
    ],
    test_suite='tests.migrate'
)
