==========
Marionette
==========

Marionette is an automation driver for Mozilla's Gecko engine.
It can remotely control either the UI or the internal JavaScript of
a Gecko platform, such as Firefox.  It can control both the chrome
(i.e. menus and functions) or the content (the webpage loaded inside
the browsing context), giving a high level of control and ability
to replicate user actions. In addition to performing actions on the
browser, Marionette can also read the properties and attributes of
the DOM.

For users
=========
.. toctree::
   :maxdepth: 1

   Intro.md
   Protocol.md


See also:

*   Documentation for `Marionette Python Client <http://marionette-client.readthedocs.io>`_,
    which is used in-tree to write many kinds of Marionette-based tests.
*   Documentation for `Firefox Puppeteer <http://firefox-puppeteer.readthedocs.io>`_, which is
    used to in-tree to write Firefox UI tests.

For marionette developers
==========================
.. toctree::
   :maxdepth: 1

   NewContributors.md
   Debugging.md
   PythonTests.md
   SeleniumAtoms.md
