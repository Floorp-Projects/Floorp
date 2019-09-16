from __future__ import absolute_import
import sys
from setuptools import setup, find_packages

PY3 = sys.version_info.major == 3

if PY3:
    entry_points = """
      [console_scripts]
      cp-creator = condprof.runner:main
      cp-client = condprof.client:main
      """
else:
    entry_points = """
      [console_scripts]
      cp-client = condprof.client:main
      """

setup(
    name="conditioned-profile",
    version="0.1",
    packages=find_packages(),
    description="Firefox Heavy Profile creator",
    include_package_data=True,
    zip_safe=False,
    install_requires=[],  # use requirements files
    entry_points=entry_points,
)
