#!/usr/bin/env python
"""ASN.1 types and codecs

   A pure-Python implementation of ASN.1 types and DER/BER/CER codecs (X.208).
"""

import os
import sys

classifiers = """\
Development Status :: 5 - Production/Stable
Environment :: Console
Intended Audience :: Developers
Intended Audience :: Education
Intended Audience :: Information Technology
Intended Audience :: Science/Research
Intended Audience :: System Administrators
Intended Audience :: Telecommunications Industry
License :: OSI Approved :: BSD License
Natural Language :: English
Operating System :: OS Independent
Programming Language :: Python :: 2
Programming Language :: Python :: 3
Topic :: Communications
Topic :: Security :: Cryptography
Topic :: Software Development :: Libraries :: Python Modules
"""

def howto_install_distribute():
    print("""
   Error: You need the distribute Python package!

   It's very easy to install it, just type (as root on Linux):

   wget http://python-distribute.org/distribute_setup.py
   python distribute_setup.py

   Then you could make eggs from this package.
""")

def howto_install_setuptools():
    print("""
   Error: You need setuptools Python package!

   It's very easy to install it, just type (as root on Linux):

   wget http://peak.telecommunity.com/dist/ez_setup.py
   python ez_setup.py

   Then you could make eggs from this package.
""")

try:
    from setuptools import setup, Command
    params = {
        'zip_safe': True
    }    
except ImportError:
    for arg in sys.argv:
        if arg.find('egg') != -1:
            if sys.version_info[0] > 2:
                howto_install_distribute()
            else:
                howto_install_setuptools()
            sys.exit(1)
    from distutils.core import setup, Command
    params = {}

doclines = [ x.strip() for x in __doc__.split('\n') if x ]

params.update( {
    'name': 'pyasn1',
    'version': open(os.path.join('pyasn1','__init__.py')).read().split('\'')[1],
    'description': doclines[0],
    'long_description': ' '.join(doclines[1:]),
    'maintainer': 'Ilya Etingof <ilya@glas.net>',
    'author': 'Ilya Etingof',
    'author_email': 'ilya@glas.net',
    'url': 'http://sourceforge.net/projects/pyasn1/',
    'platforms': ['any'],
    'classifiers': [ x for x in classifiers.split('\n') if x ],
    'license': 'BSD',
    'packages': [ 'pyasn1',
                  'pyasn1.type',
                  'pyasn1.compat',
                  'pyasn1.codec',
                  'pyasn1.codec.ber',
                  'pyasn1.codec.cer',
                  'pyasn1.codec.der' ]
} )

# handle unittest discovery feature
if sys.version_info[0:2] < (2, 7) or \
   sys.version_info[0:2] in ( (3, 0), (3, 1) ):
    try:
        import unittest2 as unittest
    except ImportError:
        unittest = None
else:
    import unittest

if unittest:
    class PyTest(Command):
        user_options = []

        def initialize_options(self): pass
        def finalize_options(self): pass

        def run(self):
            suite = unittest.defaultTestLoader.discover('.')
            unittest.TextTestRunner(verbosity=2).run(suite)

    params['cmdclass'] = { 'test': PyTest }

setup(**params)
