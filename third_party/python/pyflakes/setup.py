#!/usr/bin/env python
# Copyright 2005-2011 Divmod, Inc.
# Copyright 2013 Florent Xicluna.  See LICENSE file for details
from __future__ import with_statement

import os.path

try:
    from setuptools import setup
except ImportError:
    from distutils.core import setup
    extra = {'scripts': ["bin/pyflakes"]}
else:
    extra = {
        'test_suite': 'pyflakes.test',
        'entry_points': {
            'console_scripts': ['pyflakes = pyflakes.api:main'],
        },
    }


def get_version(fname=os.path.join('pyflakes', '__init__.py')):
    with open(fname) as f:
        for line in f:
            if line.startswith('__version__'):
                return eval(line.split('=')[-1])


def get_long_description():
    descr = []
    for fname in ('README.rst',):
        with open(fname) as f:
            descr.append(f.read())
    return '\n\n'.join(descr)


setup(
    name="pyflakes",
    license="MIT",
    version=get_version(),
    description="passive checker of Python programs",
    long_description=get_long_description(),
    author="A lot of people",
    author_email="code-quality@python.org",
    url="https://github.com/PyCQA/pyflakes",
    packages=["pyflakes", "pyflakes.scripts", "pyflakes.test"],
    python_requires='>=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*',
    classifiers=[
        "Development Status :: 6 - Mature",
        "Environment :: Console",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.4",
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: Implementation :: CPython",
        "Programming Language :: Python :: Implementation :: PyPy",
        "Topic :: Software Development",
        "Topic :: Utilities",
    ],
    **extra)
