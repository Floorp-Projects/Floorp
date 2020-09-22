===============
Remote Protocol
===============

The Firefox **remote protocol** is a low-level debugging interface
you can use to inspect the state and control execution of documents
running in web content, instrument the browser in interesting ways,
simulate user interaction for automation purposes, and for subscribing
to updates in the browser such as network- or console logs.

It complements the existing Firefox Developer Tools :ref:`Remote Debugging
Protocol <Remote Debugging Protocol>` (RDP) by implementing a subset of the
`Chrome DevTools Protocol`_ (CDP).

.. _Chrome DevTools Protocol: https://chromedevtools.github.io/devtools-protocol/

.. toctree::
  :maxdepth: 1

  Usage.md
  Prefs.md


Internals
=========

.. toctree::
  :maxdepth: 1

  Building.md
  Debugging.md
  Testing.md
  Architecture.md
  Prefs.md
  CodeStyle.md
  PuppeteerVendor.md
  Security.md


Bugs
====

Bugs are tracked under the `Remote Protocol product`_.

.. _Remote Protocol product: https://bugzilla.mozilla.org/describecomponents.cgi?product=Remote%20Protocol


Communication
=============

See `Communication`_ on `our project wiki`_.

.. _Communication: https://wiki.mozilla.org/Remote#Communication
.. _our project wiki: https://wiki.mozilla.org/Remote
