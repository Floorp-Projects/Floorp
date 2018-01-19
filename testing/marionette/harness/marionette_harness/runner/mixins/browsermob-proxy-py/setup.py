from __future__ import absolute_import

from setuptools import setup, find_packages

setup(name='browsermob-proxy',
      version='0.6.0',
      description='A library for interacting with the Browsermob Proxy',
      author='David Burns',
      author_email='david.burns at theautomatedtester dot co dot uk',
      url='http://oss.theautomatedtester.co.uk/browsermob-proxy-py',
      classifiers=['Development Status :: 3 - Alpha',
                  'Intended Audience :: Developers',
                  'License :: OSI Approved :: Apache Software License',
                  'Operating System :: POSIX',
                  'Operating System :: Microsoft :: Windows',
                  'Operating System :: MacOS :: MacOS X',
                  'Topic :: Software Development :: Testing',
                  'Topic :: Software Development :: Libraries',
                  'Programming Language :: Python'],
        packages = find_packages(),
        install_requires=['requests>=1.1.0'],
        )
