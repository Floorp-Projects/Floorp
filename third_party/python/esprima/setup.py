# -*- coding: utf-8 -*-

try:
    from setuptools import setup
except ImportError:
    from distutils.core import setup

import os

from esprima import version


def read(fname):
    try:
        with open(os.path.join(os.path.dirname(__file__), fname), "r") as fp:
            return fp.read().strip()
    except IOError:
        return ''


setup(
    name="esprima",
    version=version,
    author="German M. Bravo (Kronuz)",
    author_email="german.mb@gmail.com",
    url="https://github.com/Kronuz/esprima-python",
    license="BSD License",
    keywords="esprima ecmascript javascript parser ast",
    description="ECMAScript parsing infrastructure for multipurpose analysis in Python",
    long_description=read("README.rst"),
    packages=["esprima"],
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: BSD License",
        "Operating System :: OS Independent",
        "Topic :: Software Development :: Code Generators",
        "Topic :: Software Development :: Compilers",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "Topic :: Text Processing :: General",
        "Programming Language :: Python",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.3",
        "Programming Language :: Python :: 3.4",
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
    ],
    entry_points={
        'console_scripts': [
            'esprima = esprima.__main__:main',
        ]
    },
)
