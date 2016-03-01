.. py:currentmodule:: firefox_puppeteer

Firefox Puppeteer
=================

Firefox Puppeteer is a library built on top of the `Marionette python client`_.
It aims to make automation of Firefox's browser UI simpler. It does **not**
make sense to use Firefox Puppeteer if:

* You are manipulating something other than Firefox (like Firefox OS)
* You are only manipulating elements in content scope (like a webpage)

Roughly speaking, Firefox Puppeteer provides a library to manipulate each
visual section of Firefox's browser UI. For example, there are different
libraries for the tab bar, the navigation bar, etc.


Installation
------------

Currently Firefox Puppeteer lives in the `firefox-ui-tests`_ repository,
along with instructions for installation and usage.
There are plans to move it alongside the `Marionette python client`_.

.. _Marionette python client: http://marionette-client.readthedocs.org/en/latest/
.. _firefox-ui-tests: https://github.com/mozilla/firefox-ui-tests/tree/mozilla-central/firefox_puppeteer


Libraries
---------

The following libraries are currently implemented. More will be added in the
future. Each library is available from an instance of the FirefoxTestCase class.

.. toctree::

   ui/about_window/window
   ui/deck
   ui/menu
   ui/pageinfo/window
   ui/browser/tabbar
   ui/browser/toolbars
   ui/browser/window
   ui/update_wizard/dialog
   ui/windows
   api/appinfo
   api/keys
   api/l10n
   api/places
   api/prefs
   api/security
   api/software_update
   api/utils


Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
