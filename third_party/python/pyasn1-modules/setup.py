#!/usr/bin/env python
"""A collection of ASN.1-based protocols modules.

   A collection of ASN.1 modules expressed in form of pyasn1 classes.
   Includes protocols PDUs definition (SNMP, LDAP etc.) and various
   data structures (X.509, PKCS etc.).
"""

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
    from setuptools import setup
    params = {
        'install_requires': [ 'pyasn1>=0.1.4' ],
        'zip_safe': True
        }    
except ImportError:
    import sys
    for arg in sys.argv:
        if arg.find('egg') != -1:
            if sys.version_info[0] > 2:
                howto_install_distribute()
            else:
                howto_install_setuptools()
            sys.exit(1)
    from distutils.core import setup
    params = {}
    if sys.version_info[:2] > (2, 4):
        params['requires'] = [ 'pyasn1(>=0.1.4)' ]

doclines = [ x.strip() for x in __doc__.split('\n') if x ]

params.update( {
    'name': 'pyasn1-modules',
    'version': open('pyasn1_modules/__init__.py').read().split('\'')[1],
    'description': doclines[0],
    'long_description': ' '.join(doclines[1:]),
    'maintainer': 'Ilya Etingof <ilya@glas.net>',
    'author': 'Ilya Etingof',
    'author_email': 'ilya@glas.net',
    'url': 'http://sourceforge.net/projects/pyasn1/',
    'platforms': ['any'],
    'classifiers': [ x for x in classifiers.split('\n') if x ],
    'license': 'BSD',
    'packages': [ 'pyasn1_modules' ]
    } )

setup(**params)
