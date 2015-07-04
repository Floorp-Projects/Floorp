import os
from setuptools import setup, find_packages

try:
    here = os.path.dirname(os.path.abspath(__file__))
    description = open(os.path.join(here, 'README.txt')).read()
except IOError:
    description = ''

import mozharness
version = mozharness.version_string

dependencies = ['virtualenv', 'mock', "coverage", "nose", "pylint", "pyflakes"]
try:
    import json
except ImportError:
    dependencies.append('simplejson')

setup(name='mozharness',
      version=version,
      description="Mozharness is a configuration-driven script harness with full logging that allows production infrastructure and individual developers to use the same scripts. ",
      long_description=description,
      classifiers=[],  # Get strings from http://www.python.org/pypi?%3Aaction=list_classifiers
      author='Aki Sasaki',
      author_email='aki@mozilla.com',
      url='https://hg.mozilla.org/build/mozharness/',
      license='MPL',
      packages=find_packages(exclude=['ez_setup', 'examples', 'tests']),
      include_package_data=True,
      zip_safe=False,
      install_requires=dependencies,
      entry_points="""
      # -*- Entry points: -*-
      """,
      )
