import json
import os
from setuptools import setup, find_packages

package_json = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'package.json')
with open(package_json) as f:
    version = json.load(f)['version']

setup(name='json-e',
    version=version,
    description='A data-structure parameterization system written for embedding context in JSON objects',
    author='Dustin J. Mitchell',
    url='https://taskcluster.github.io/json-e/',
    author_email='dustin@mozilla.com',
    packages=['jsone'],
    test_suite='nose.collector',
    license='MPL2',
    tests_require=[
        "freezegun",
        "hypothesis",
        "nose",
        "PyYAML",
        "python-dateutil",
        'pep8',
    ]
)
