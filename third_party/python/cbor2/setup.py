# coding: utf-8
import os.path

from setuptools import setup, find_packages


here = os.path.dirname(__file__)
readme_path = os.path.join(here, 'README.rst')
readme = open(readme_path).read()

setup(
    name='cbor2',
    use_scm_version={
        'version_scheme': 'post-release',
        'local_scheme': 'dirty-tag'
    },
    description='Pure Python CBOR (de)serializer with extensive tag support',
    long_description=readme,
    author=u'Alex Grönholm',
    author_email='alex.gronholm@nextday.fi',
    url='https://github.com/agronholm/cbor2',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.3',
        'Programming Language :: Python :: 3.4',
        'Programming Language :: Python :: 3.5',
        'Programming Language :: Python :: 3.6'
    ],
    keywords='serialization cbor',
    license='MIT',
    packages=find_packages(exclude=['tests']),
    setup_requires=[
        'setuptools_scm'
    ],
    extras_require={
        'testing': ['pytest', 'pytest-cov']
    }
)
