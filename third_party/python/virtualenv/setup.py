import os
import re

from setuptools import setup


def get_version():
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "virtualenv.py")) as file_handler:
        version_file = file_handler.read()
    version_match = re.search(r"^__version__ = ['\"]([^'\"]*)['\"]", version_file, re.M)
    if version_match:
        return version_match.group(1)
    raise RuntimeError("Unable to find version string.")


setup(version=get_version(), py_modules=["virtualenv"], setup_requires=["setuptools >= 40.6.3"])
