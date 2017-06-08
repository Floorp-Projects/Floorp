# -*- coding: utf-8 -*-

import os
import re
import sys
from setuptools import setup
from setuptools.command.test import test as TestCommand

HERE = os.path.dirname(os.path.realpath(__file__))


class PyTest(TestCommand):
    """
    Run py.test with the "python setup.py test command"
    """
    user_options = [('pytest-args=', 'a', "Arguments to pass to py.test")]

    def initialize_options(self):
        TestCommand.initialize_options(self)
        self.pytest_args = ''

    def finalize_options(self):
        TestCommand.finalize_options(self)
        self.pytest_args += (' ' + self.distribution.test_suite)

    def run_tests(self):
        import pytest
        errno = pytest.main(self.pytest_args)
        sys.exit(errno)


def read(*parts):
    with open(os.path.join(HERE, *parts)) as f:
        return f.read()


def parse_requirements(data, exclude=()):
    return [line for line in data.splitlines()
            if line and not line.startswith("#") and line not in exclude]


def version():
    return re.findall(r"__version__ = \"([\d.]+)\"",
                      read("dlmanager", "__init__.py"))[0]

setup(
    name="dlmanager",
    version=version(),
    description="download manager library",
    long_description=read("README.rst"),
    author="Julien Pagès",
    author_email="j.parkouss@gmail.com",
    url="http://github.com/parkouss/dlmanager",
    license="GPL/LGPL",
    install_requires=parse_requirements(read("requirements.txt")),
    cmdclass={'test': PyTest},
    tests_require=parse_requirements(read("requirements.txt"),
                                     exclude=("-e .",)),
    test_suite='tests',
)
