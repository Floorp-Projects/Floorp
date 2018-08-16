from setuptools import setup

import sys
import io
import os
import atexit
sys.path.insert(0, '.')
version = __import__('voluptuous').__version__


with io.open('README.md', encoding='utf-8') as f:
    long_description = f.read()
    description = long_description.splitlines()[0].strip()


setup(
    name='voluptuous',
    url='https://github.com/alecthomas/voluptuous',
    download_url='https://pypi.python.org/pypi/voluptuous',
    version=version,
    description=description,
    long_description=long_description,
    long_description_content_type='text/markdown',
    license='BSD',
    platforms=['any'],
    packages=['voluptuous'],
    author='Alec Thomas',
    author_email='alec@swapoff.org',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: BSD License',
        'Operating System :: OS Independent',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
    ]
)
