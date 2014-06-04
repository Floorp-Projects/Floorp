from setuptools import setup, find_packages

version = '0.2'

long_description = \
"""Marionette_ is a Mozilla project to enable remote automation in Gecko-based
projects, including desktop Firefox, mobile Firefox, and Firefox OS. It is
inspired by `Selenium Webdriver`_.

This package defines the transport layer used by a Marionette client to
communicate with the Marionette server embedded in Gecko.  It has no entry
points; rather it's designed to be used by a Marionette client implementation.

.. _Marionette: https://developer.mozilla.org/en/Marionette
.. _`Selenium Webdriver`: http://www.seleniumhq.org/projects/webdriver/"""

# dependencies
deps = []

setup(name='marionette-transport',
      version=version,
      description="Transport layer for Marionette client",
      long_description=long_description,
      classifiers=[], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='mozilla',
      author='Mozilla Automation and Tools Team',
      author_email='tools@lists.mozilla.org',
      url='https://developer.mozilla.org/en-US/docs/Marionette',
      license='MPL',
      packages=find_packages(exclude=['ez_setup', 'examples', 'tests']),
      package_data={},
      include_package_data=False,
      zip_safe=False,
      entry_points="""
      """,
      install_requires=deps,
      )

