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


Developers
==========

.. toctree::
  :maxdepth: 1

  Building.md
  Debugging.md
  Testing.md
  Prefs.md
  CodeStyle.md


Bugs
====

Bugs are tracked under the `Remote Protocol`_ product.

.. _Remote Protocol: https://bugzilla.mozilla.org/describecomponents.cgi?product=Remote%20Protocol


Communication
=============

The mailing list for Firefox remote debugging discussion is
`dev-remote@lists.mozilla.org`_ (`subscribe`_, `archive`_).

If you prefer real-time chat, there is often someone in the *#remote*
IRC channel on irc.mozilla.org.  Don’t ask if you may ask a
question just go ahead and ask, and please wait for an answer as
we might not be in your timezone.

.. _dev-remote@lists.mozilla.org: mailto:dev-remote@lists.mozilla.org
.. _subscribe: https://lists.mozilla.org/listinfo/dev-remote
.. _archive: https://lists.mozilla.org/pipermail/dev-remote/
