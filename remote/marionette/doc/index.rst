==========
Marionette
==========

Marionette is a remote `protocol`_ that lets out-of-process programs
communicate with, instrument, and control Gecko-based browsers.

It provides interfaces for interacting with both the internal JavaScript
runtime and UI elements of Gecko-based browsers, such as Firefox
and Fennec.  It can control both the chrome- and content documents,
giving a high level of control and ability to emulate user interaction.

Within the central tree, Marionette is used in most TaskCluster
test jobs to instrument Gecko.  It can additionally be used to
write different kinds of functional tests:

  * The `Marionette Python client`_ is used in the `Mn` job, which
    is generally what you want to use for interacting with web documents

Outside the tree, Marionette is used by `geckodriver`_ to implement
`WebDriver`_.

Marionette supports to various degrees all the Gecko based applications,
including Firefox, Thunderbird, Fennec, and Fenix.

.. _protocol: Protocol.html
.. _Marionette Python client: /python/marionette_driver.html
.. _geckodriver: /testing/geckodriver/
.. _WebDriver: https://w3c.github.io/webdriver/

Some further documentation can be found here:

.. toctree::
   :maxdepth: 1

   Intro.md
   Building.md
   PythonTests.md
   Protocol.md
   Contributing.md
   NewContributors.md
   Patches.md
   Debugging.md
   Testing.md
   Taskcluster.md
   CodeStyle.md
   SeleniumAtoms.md
   Prefs.md
   internals/index


Bugs
====

Bugs are tracked in the `Testing :: Marionette` component.


Communication
=============

The mailing list for Marionette discussion is
tools-marionette@lists.mozilla.org (`subscribe`_, `archive`_).

If you prefer real-time chat, ask your questions
on `#interop:mozilla.org <https://chat.mozilla.org/#/room/#interop:mozilla.org>`__.

.. _subscribe: https://lists.mozilla.org/listinfo/tools-marionette
.. _archive: https://lists.mozilla.org/pipermail/tools-marionette/
