# -*- coding: utf-8 -*-
# Copyright (C) 2016 Adrien Vergé
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

from setuptools import find_packages, setup

from yamllint import (__author__, __license__,
                      APP_NAME, APP_VERSION, APP_DESCRIPTION)


setup(
    name=APP_NAME,
    version=APP_VERSION,
    author=__author__,
    description=APP_DESCRIPTION.split('\n')[0],
    long_description=APP_DESCRIPTION,
    license=__license__,
    keywords=['yaml', 'lint', 'linter', 'syntax', 'checker'],
    url='https://github.com/adrienverge/yamllint',
    python_requires='>=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Environment :: Console',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: GNU General Public License v3 (GPLv3)',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.4',
        'Programming Language :: Python :: 3.5',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Topic :: Software Development',
        'Topic :: Software Development :: Debuggers',
        'Topic :: Software Development :: Quality Assurance',
        'Topic :: Software Development :: Testing',
    ],

    packages=find_packages(exclude=['tests', 'tests.*']),
    entry_points={'console_scripts': ['yamllint=yamllint.cli:run']},
    package_data={'yamllint': ['conf/*.yaml']},
    install_requires=['pathspec >=0.5.3', 'pyyaml'],
    test_suite='tests',
)
