import os
from setuptools import setup, find_packages
import sys

version = '0.0.0'

# dependencies
with open('requirements.txt') as f:
    deps = f.read().splitlines()

setup(name='taskcluster_graph',
      version=version,
      description='',
      classifiers=[],
      keywords='mozilla',
      license='MPL',
      packages=['taskcluster_graph'],
      install_requires=deps,
      )
