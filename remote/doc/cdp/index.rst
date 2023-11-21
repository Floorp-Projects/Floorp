=====================
Remote Protocol (CDP)
=====================

The Firefox **remote protocol (CDP)** is a low-level debugging interface
you can use to inspect the state and control execution of documents
running in web content, instrument the browser in interesting ways,
simulate user interaction for automation purposes, and for subscribing
to updates in the browser such as network- or console logs.

It complements the existing Firefox Developer Tools :ref:`Remote Debugging
Protocol <Remote Debugging Protocol>` (RDP) by implementing a subset of the
`Chrome DevTools Protocol`_ (CDP).

To use Firefox remote protocol with Fission, CDP client authors should read the
`Required Preferences`_ page.

.. _Chrome DevTools Protocol: https://chromedevtools.github.io/devtools-protocol/
.. _Required Preferences: /remote/cdp/RequiredPreferences.html

.. toctree::
  :maxdepth: 1

  Usage.md
  Architecture.md
  RequiredPreferences.md
