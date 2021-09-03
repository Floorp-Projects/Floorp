import json
import os
from setuptools import setup

package_json = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'package.json')
with open(package_json) as f:
    version = json.load(f)['version']

setup(
    name='taskcluster-urls',
    description='Standardized url generator for taskcluster resources.',
    long_description=open(os.path.join(os.path.dirname(__file__), 'README.md')).read(),
    long_description_content_type='text/markdown',
    url='https://github.com/taskcluster/taskcluster-lib-urls',
    version=version,
    packages=['taskcluster_urls'],
    author='Brian Stack',
    author_email='bstack@mozilla.com',
    license='MPL2',
    classifiers=[
        'License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.5',
        'Programming Language :: Python :: 3.6',
    ],
)
