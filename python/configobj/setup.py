# setup.py
# Install script for ConfigObj
# Copyright (C) 2005-2010 Michael Foord, Mark Andrews, Nicola Larosa
# E-mail: fuzzyman AT voidspace DOT org DOT uk
#         mark AT la-la DOT com
#         nico AT tekNico DOT net

# This software is licensed under the terms of the BSD license.
# http://www.voidspace.org.uk/python/license.shtml

import sys
from distutils.core import setup
from configobj import __version__ as VERSION

NAME = 'configobj'

MODULES = 'configobj', 'validate'

DESCRIPTION = 'Config file reading, writing and validation.'

URL = 'http://www.voidspace.org.uk/python/configobj.html'

DOWNLOAD_URL = "http://www.voidspace.org.uk/downloads/configobj-%s.zip" % VERSION

LONG_DESCRIPTION = """**ConfigObj** is a simple but powerful config file reader and writer: an *ini
file round tripper*. Its main feature is that it is very easy to use, with a
straightforward programmer's interface and a simple syntax for config files.
It has lots of other features though :

* Nested sections (subsections), to any level
* List values
* Multiple line values
* Full Unicode support
* String interpolation (substitution)
* Integrated with a powerful validation system

    - including automatic type checking/conversion
    - and allowing default values
    - repeated sections

* All comments in the file are preserved
* The order of keys/sections is preserved
* Powerful ``unrepr`` mode for storing/retrieving Python data-types

| Release 4.7.2 fixes several bugs in 4.7.1
| Release 4.7.1 fixes a bug with the deprecated options keyword in
| 4.7.0.
| Release 4.7.0 improves performance adds features for validation and
| fixes some bugs."""

CLASSIFIERS = [
    'Development Status :: 6 - Mature',
    'Intended Audience :: Developers',
    'License :: OSI Approved :: BSD License',
    'Programming Language :: Python',
    'Programming Language :: Python :: 2.3',
    'Programming Language :: Python :: 2.4',
    'Programming Language :: Python :: 2.5',
    'Programming Language :: Python :: 2.6',
    'Operating System :: OS Independent',
    'Topic :: Software Development :: Libraries',
    'Topic :: Software Development :: Libraries :: Python Modules',
]

AUTHOR = 'Michael Foord & Nicola Larosa'

AUTHOR_EMAIL = 'fuzzyman@voidspace.org.uk'

KEYWORDS = "config, ini, dictionary, application, admin, sysadmin, configuration, validation".split(', ')


setup(name=NAME,
      version=VERSION,
      description=DESCRIPTION,
      long_description=LONG_DESCRIPTION,
      download_url=DOWNLOAD_URL,
      author=AUTHOR,
      author_email=AUTHOR_EMAIL,
      url=URL,
      py_modules=MODULES,
      classifiers=CLASSIFIERS,
      keywords=KEYWORDS
     )
