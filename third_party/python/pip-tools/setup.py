"""
pip-tools keeps your pinned dependencies fresh.
"""
from os.path import abspath, dirname, join
from setuptools import find_packages, setup

def read_file(filename):
    """Read the contents of a file located relative to setup.py"""
    with open(join(abspath(dirname(__file__)), filename)) as thefile:
        return thefile.read()

setup(
    name='pip-tools',
    use_scm_version=True,
    url='https://github.com/jazzband/pip-tools/',
    license='BSD',
    author='Vincent Driessen',
    author_email='me@nvie.com',
    description=__doc__.strip(),
    long_description=read_file('README.rst'),
    packages=find_packages(exclude=['tests']),
    package_data={},
    python_requires=">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*",
    setup_requires=['setuptools_scm'],
    install_requires=[
        'click>=6',
        'six',
    ],
    zip_safe=False,
    entry_points={
        'console_scripts': [
            'pip-compile = piptools.scripts.compile:cli',
            'pip-sync = piptools.scripts.sync:cli',
        ],
    },
    platforms='any',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'Intended Audience :: System Administrators',
        'License :: OSI Approved :: BSD License',
        'Operating System :: OS Independent',
        'Programming Language :: Python',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.4',
        'Programming Language :: Python :: 3.5',
        'Programming Language :: Python :: 3.6',
        'Topic :: System :: Systems Administration',
    ]
)
