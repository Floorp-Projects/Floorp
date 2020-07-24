#!/usr/bin/env python
import sys
import os
import os.path
# appdirs is a dependency of setuptools, so allow installing without it.
try:
    from setuptools import setup
except ImportError:
    from distutils.core import setup
import appdirs

tests_require = []
if sys.version_info < (2, 7):
    tests_require.append("unittest2")


def read(fname):
    inf = open(os.path.join(os.path.dirname(__file__), fname))
    out = "\n" + inf.read().replace("\r\n", "\n")
    inf.close()
    return out


setup(
    name='appdirs',
    version=appdirs.__version__,
    description='A small Python module for determining appropriate ' + \
        'platform-specific dirs, e.g. a "user data dir".',
    long_description=read('README.rst') + '\n' + read('CHANGES.rst'),
    classifiers=[c.strip() for c in """
        Development Status :: 4 - Beta
        Intended Audience :: Developers
        License :: OSI Approved :: MIT License
        Operating System :: OS Independent
        Programming Language :: Python :: 2
        Programming Language :: Python :: 2.6
        Programming Language :: Python :: 2.7
        Programming Language :: Python :: 3
        Programming Language :: Python :: 3.2
        Programming Language :: Python :: 3.3
        Programming Language :: Python :: 3.4
        Programming Language :: Python :: 3.5
        Programming Language :: Python :: 3.6
        Programming Language :: Python :: Implementation :: PyPy
        Programming Language :: Python :: Implementation :: CPython
        Topic :: Software Development :: Libraries :: Python Modules
        """.split('\n') if c.strip()],
    test_suite='test.test_api',
    tests_require=tests_require,
    keywords='application directory log cache user',
    author='Trent Mick',
    author_email='trentm@gmail.com',
    maintainer='Trent Mick; Sridhar Ratnakumar; Jeff Rouse',
    maintainer_email='trentm@gmail.com; github@srid.name; jr@its.to',
    url='http://github.com/ActiveState/appdirs',
    license='MIT',
    py_modules=["appdirs"],
)
