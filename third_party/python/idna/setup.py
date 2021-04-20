"""
A library to support the Internationalised Domain Names in Applications
(IDNA) protocol as specified in RFC 5890 et.al. This new methodology,
known as IDNA 2008, can generate materially different results to the
previous standard. The library can act as a drop-in replacement for
the "encodings.idna" module.
"""

import io, sys
from setuptools import setup


def main():

    python_version = sys.version_info[:2]
    if python_version < (2,7):
        raise SystemExit("Sorry, Python 2.7 or newer required")

    package_data = {}
    exec(open('idna/package_data.py').read(), package_data)

    arguments = {
        'name': 'idna',
        'packages': ['idna'],
        'version': package_data['__version__'],
        'description': 'Internationalized Domain Names in Applications (IDNA)',
        'long_description': io.open("README.rst", encoding="UTF-8").read(),
        'author': 'Kim Davies',
        'author_email': 'kim@cynosure.com.au',
        'license': 'BSD-like',
        'url': 'https://github.com/kjd/idna',
        'classifiers': [
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
            'Programming Language :: Python :: 3.7',
            'Programming Language :: Python :: 3.8',
            'Programming Language :: Python :: Implementation :: CPython',
            'Programming Language :: Python :: Implementation :: PyPy',
            'Topic :: Internet :: Name Service (DNS)',
            'Topic :: Software Development :: Libraries :: Python Modules',
            'Topic :: Utilities',
        ],
        'python_requires': '>=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*',
    }

    setup(**arguments)

if __name__ == '__main__':
    main()
