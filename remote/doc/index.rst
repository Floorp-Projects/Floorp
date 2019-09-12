===============
Remote Protocol
===============

The Firefox **remote protocol** is a low-level debugging interface
you can use to inspect the state and control execution of documents
running in web content, instrument the browser in interesting ways,
simulate user interaction for automation purposes, and for subscribing
to updates in the browser such as network- or console logs.

It complements the existing Firefox Developer Tools `Remote Debugging
Protocol`_ (RDP) by implementing a subset of the `Chrome DevTools
Protocol`_ (CDP).

.. _Remote Debugging Protocol: https://docs.firefox-dev.tools/backend/protocol.html
.. _Chrome DevTools Protocol: https://chromedevtools.github.io/devtools-protocol/


Users
=====

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

The mailing list for Firefox remote debugging discussion is
`dev-remote@lists.mozilla.org`_ (`subscribe`_, `archive`_).

If you prefer real-time chat, there is often someone in
the *#remote* channel on the `DevTools Slack`_ instance.

.. _dev-remote@lists.mozilla.org: mailto:dev-remote@lists.mozilla.org
.. _subscribe: https://lists.mozilla.org/listinfo/dev-remote
.. _archive: https://lists.mozilla.org/pipermail/dev-remote/
.. _DevTools Slack: https://devtools-html-slack.herokuapp.com/
