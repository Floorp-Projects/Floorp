.. py:currentmodule:: firefox_puppeteer

Firefox Puppeteer
=================

`Firefox Puppeteer`_ is a library built on top of the `Marionette Python client`_.
It aims to make automation of Firefox's browser UI simpler. It does **not**
make sense to use Firefox Puppeteer if:

* You are manipulating something other than Firefox (like Firefox OS)
* You are only manipulating elements in content scope (like a webpage)

Roughly speaking, Firefox Puppeteer provides a library to manipulate each
visual section of Firefox's browser UI. For example, there are different
libraries for the tab bar, the navigation bar, etc.

.. _Firefox Puppeteer: http://firefox-puppeteer.readthedocs.io/
.. _Marionette Python client: https://firefox-source-docs.mozilla.org/python/marionette_driver.html

Installation
------------

For end-users Firefox Puppeteer can be easily installed as a `Python package`_
from PyPI. If you want to contribute to the project we propose that you clone
the `mozilla-central`_ repository and run the following commands::

$ cd testing/puppeteer/firefox
$ python setup.py develop

In both cases all necessary files including all dependencies will be installed.

.. _Python package: https://pypi.python.org/pypi/firefox-puppeteer
.. _mozilla-central: https://hg.mozilla.org/mozilla-central

Versioning
----------

Puppeteer versions as regularly released from the Python source code, will follow
a specific versioning schema. It means the major version number will always
be identical with the supported Firefox version. Minor releases - the second part
of the version number - are done throughout the life-cycle of a Firefox version
when Puppeteer itself needs API changes for back-end and front-end modules. The
last part of the version number is the patch level, and is only used for bugfix
releases without any API changes.

Examples:

  firefox_puppeteer_45.0.0 - First release for Firefox 45.0 and Firefox 45.xESR
  firefox_puppeteer_46.2.0 - Second release for Firefox 46.0 caused by API changes
  firefox_puppeteer_47.0.1 - First bugfix release for the new Firefox 47.0 support


Libraries
---------

The following libraries are currently implemented. More will be added in the
future. Each library is available from an instance of the FirefoxTestCase class.

.. toctree::

   ui/about_window/window
   ui/deck
   ui/menu
   ui/pageinfo/window
   ui/browser/notifications
   ui/browser/tabbar
   ui/browser/toolbars
   ui/browser/window
   ui/windows
   api/appinfo
   api/keys
   api/l10n
   api/places
   api/security
   api/utils


Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
