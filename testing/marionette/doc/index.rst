==========
Marionette
==========

Marionette is the remote protocol that lets OOP programs communicate
with, instrument, and control Gecko.


Description
===========

Marionette is an automation driver for Mozilla’s Gecko engine.
It can remotely control either the UI or the internal JavaScript of
Gecko-based browsers, such as Firefox and Fennec.  It can control
both the chrome and the content document, giving a high level of
control and ability to replicate user interaction.  In addition
to performing actions on the browser, Marionette can also ready
properties and attributes of the DOM.


For users
=========

.. toctree::
   :maxdepth: 1

   `Marionette README`_
   Intro.md
   Protocol.md

See also:

* Documentation for `Marionette Python client`_, which is used
  in-tree to write many kinds of Marionette-based tests.
* Documentation for `Firefox Puppeteer`_, which is used to in-tree
  to write Firefox UI tests.

.. _Marionette README: https://searchfox.org/mozilla-central/source/testing/marionette/README.md
.. _Marionette Python client: http://marionette-client.readthedocs.io
.. _Firefox Puppeteer: http://firefox-puppeteer.readthedocs.io


For developers
==============

.. toctree::
   :maxdepth: 1

   ../CONTRIBUTING.md
   NewContributors.md
   Debugging.md
   PythonTests.md
   SeleniumAtoms.md


Bugs
====

Bugs are tracked in the `Testing :: Marionette` component.


Communication
=============

The mailing list for discussion is tools-marionette@lists.mozilla.org
(subscribe_, archive_).  If you prefer real-time chat, there
is often someone in the #ateam IRC channel on irc.mozilla.org.
Don’t ask if you can ask a question, just ask, and please wait
for an answer as we might not be in your timezone.

.. _subscribe: https://lists.mozilla.org/listinfo/tools-marionette
.. _archive: https://groups.google.com/group/mozilla.tools.marionette
