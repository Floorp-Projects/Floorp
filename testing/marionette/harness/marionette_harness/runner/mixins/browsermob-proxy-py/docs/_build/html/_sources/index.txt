.. BrowserMob Proxy documentation master file, created by
   sphinx-quickstart on Fri May 24 12:37:12 2013.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.
.. highlightlang:: python



Welcome to BrowserMob Proxy's documentation!
============================================

Python client for the BrowserMob Proxy 2.0 REST API.

How to install
--------------

BrowserMob Proxy is available on PyPI_, so you can install it with ``pip``::

    $ pip install browsermob-proxy

Or with `easy_install`::

    $ easy_install browsermob-proxy

Or by cloning the repo from GitHub_::

    $ git clone git://github.com/AutomatedTester/browsermob-proxy-py.git

Then install it by running::

    $ python setup.py install

How to use with selenium-webdriver
----------------------------------

Manually::

    from browsermobproxy import Server
    server = Server("path/to/browsermob-proxy")
    server.start()
    proxy = server.create_proxy()

    from selenium import webdriver
    profile  = webdriver.FirefoxProfile()
    profile.set_proxy(proxy.selenium_proxy())
    driver = webdriver.Firefox(firefox_profile=profile)


    proxy.new_har("google")
    driver.get("http://www.google.co.uk")
    proxy.har # returns a HAR JSON blob

    server.stop()
    driver.quit()

Contents:

.. toctree::
   :maxdepth: 2

   client.rst
   server.rst

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

.. _GitHub: https://github.com/AutomatedTester/browsermob-proxy-py
.. _PyPI: http://pypi.python.org/pypi/browsermob-proxy
