from __future__ import absolute_import

import os
import re

from setuptools import find_packages, setup


THIS_DIR = os.path.dirname(os.path.realpath(__name__))


def read(*parts):
    with open(os.path.join(THIS_DIR, *parts)) as f:
        return f.read()


def get_version():
    return re.findall("__version__ = '([\d\.]+)'",
                      read('marionette_harness', '__init__.py'), re.M)[0]


setup(name='marionette-harness',
      version=get_version(),
      description="Marionette test automation harness",
      long_description=open('README.rst').read(),
      # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      classifiers=[
          'Development Status :: 5 - Production/Stable',
          'Intended Audience :: Developers',
          'License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)',
          'Operating System :: MacOS :: MacOS X',
          'Operating System :: Microsoft :: Windows',
          'Operating System :: POSIX',
          'Topic :: Software Development :: Quality Assurance',
          'Topic :: Software Development :: Testing',
          'Topic :: Utilities',
          'Programming Language :: Python',
          'Programming Language :: Python :: 2.7',
      ],
      keywords='mozilla',
      author='Auto-tools',
      author_email='tools-marionette@lists.mozilla.org',
      url='https://wiki.mozilla.org/Auto-tools/Projects/Marionette',
      license='Mozilla Public License 2.0 (MPL 2.0)',
      packages=find_packages(),
      # Needed to include package data as specified in MANIFEST.in
      include_package_data=True,
      install_requires=read('requirements.txt').splitlines(),
      zip_safe=False,
      entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      marionette = marionette_harness.runtests:cli
      """,
      )
