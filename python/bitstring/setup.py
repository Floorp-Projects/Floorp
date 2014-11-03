#!/usr/bin/env python
from distutils.core import setup
# from distutils.extension import Extension
# from Cython.Distutils import build_ext
import sys

kwds = {'long_description': open('README.txt').read()}

if sys.version_info[:2] < (2, 6):
    raise Exception('This version of bitstring needs Python 2.6 or later. '
                    'For Python 2.4 / 2.5 please use bitstring version 1.0 instead.')

# macros = [('PYREX_WITHOUT_ASSERTIONS', None)]
# ext_modules = [Extension('bitstring', ["bitstring.pyx"], define_macros=macros)]

setup(name='bitstring',
      version='3.1.3',
      description='Simple construction, analysis and modification of binary data.',
      author='Scott Griffiths',
      author_email='scott@griffiths.name',
      url='http://python-bitstring.googlecode.com',
      download_url='http://python-bitstring.googlecode.com',
      license='The MIT License: http://www.opensource.org/licenses/mit-license.php',
      # cmdclass = {'build_ext': build_ext},
      # ext_modules = ext_modules,
      py_modules=['bitstring'],
      platforms='all',
      classifiers = [
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'Operating System :: OS Independent',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.0',
        'Programming Language :: Python :: 3.1',
        'Programming Language :: Python :: 3.2',
        'Programming Language :: Python :: 3.3',
        'Topic :: Software Development :: Libraries :: Python Modules',
      ],
      **kwds
      )

