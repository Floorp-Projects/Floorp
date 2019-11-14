#!/usr/bin/env python
"""
responses
=========

A utility library for mocking out the `requests` Python library.

:copyright: (c) 2015 David Cramer
:license: Apache 2.0
"""

import sys

from setuptools import setup
from setuptools.command.test import test as TestCommand

setup_requires = []

if "test" in sys.argv:
    setup_requires.append("pytest")

install_requires = [
    "cookies; python_version < '3.4'",
    "mock; python_version < '3.3'",
    "requests>=2.0",
    "six",
]

tests_require = [
    "pytest",
    "coverage >= 3.7.1, < 5.0.0",
    "pytest-cov",
    "pytest-localserver",
    "flake8",
]

extras_require = {"tests": tests_require}


class PyTest(TestCommand):
    def finalize_options(self):
        TestCommand.finalize_options(self)
        self.test_args = ["test_responses.py"]
        self.test_suite = True

    def run_tests(self):
        # import here, cause outside the eggs aren't loaded
        import pytest

        errno = pytest.main(self.test_args)
        sys.exit(errno)


setup(
    name="responses",
    version="0.10.6",
    author="David Cramer",
    description=("A utility library for mocking out the `requests` Python library."),
    url="https://github.com/getsentry/responses",
    license="Apache 2.0",
    long_description=open("README.rst").read(),
    py_modules=["responses"],
    zip_safe=False,
    python_requires=">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*",
    install_requires=install_requires,
    extras_require=extras_require,
    tests_require=tests_require,
    setup_requires=setup_requires,
    cmdclass={"test": PyTest},
    include_package_data=True,
    classifiers=[
        "Intended Audience :: Developers",
        "Intended Audience :: System Administrators",
        "Operating System :: OS Independent",
        "Programming Language :: Python",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.4",
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Topic :: Software Development",
    ],
)
