===========
geckodriver
===========

Proxy for using W3C WebDriver-compatible clients to interact with
Gecko-based browsers.

This program provides the HTTP API described by the `WebDriver protocol`_.
to communicate with Gecko browsers, such as Firefox.  It translates calls
into the `Firefox remote protocol`_ by acting as a proxy between the local-
and remote ends.

You can consult the `change log`_ for a record of all notable changes
to the program.  Releases_ are made available on GitHub.

.. _WebDriver protocol: https://w3c.github.io/webdriver/#protocol
.. _Firefox remote protocol: https://firefox-source-docs.mozilla.org/testing/marionette/marionette/Protocol.html
.. _change log: https://github.com/mozilla/geckodriver/releases
.. _Releases: https://github.com/mozilla/geckodriver/releases


For users
=========
.. toctree::
   :maxdepth: 1

   Support.md
   WebDriver capabilities <https://developer.mozilla.org/en-US/docs/Web/WebDriver/Capabilities>
   Capabilities.md
   Usage.md
   Flags.md
   Bugs.md
   TraceLogs.md
   CrashReports.md


For developers
==============
.. toctree::
   :maxdepth: 1

   Building.md
   Testing.md
   Releasing.md


Communication
=============

The mailing list for geckodriver discussion is
tools-marionette@lists.mozilla.org (`subscribe`_, `archive`_).

If you prefer real-time chat, there is often someone in the #interop IRC
channel on irc.mozilla.org.  Don’t ask if you may ask a question;
just go ahead and ask, and please wait for an answer as we might
not be in your timezone.

.. _subscribe: https://lists.mozilla.org/listinfo/tools-marionette
.. _archive: https://lists.mozilla.org/pipermail/tools-marionette/
