# encoding: utf-8

import io
from setuptools import setup, find_packages

from pathspec import __author__, __email__, __license__, __project__, __version__

# Read readme and changes files.
with io.open("README.rst", mode='r', encoding='UTF-8') as fh:
	readme = fh.read().strip()
with io.open("CHANGES.rst", mode='r', encoding='UTF-8') as fh:
	changes = fh.read().strip()

setup(
	name=__project__,
	version=__version__,
	author=__author__,
	author_email=__email__,
	url="https://github.com/cpburnz/python-path-specification",
	description="Utility library for gitignore style pattern matching of file paths.",
	long_description=readme + "\n\n" + changes,
	python_requires=">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*, !=3.4.*",
	classifiers=[
		"Development Status :: 4 - Beta",
		"Intended Audience :: Developers",
		"License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)",
		"Operating System :: OS Independent",
		"Programming Language :: Python",
		"Programming Language :: Python :: 2",
		"Programming Language :: Python :: 2.7",
		"Programming Language :: Python :: 3",
		"Programming Language :: Python :: 3.5",
		"Programming Language :: Python :: 3.6",
		"Programming Language :: Python :: 3.7",
		"Programming Language :: Python :: 3.8",
		"Programming Language :: Python :: Implementation :: CPython",
		"Programming Language :: Python :: Implementation :: PyPy",
		"Topic :: Software Development :: Libraries :: Python Modules",
		"Topic :: Utilities",
	],
	license=__license__,
	packages=find_packages(),
	test_suite='pathspec.tests',
)
