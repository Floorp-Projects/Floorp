==========
Marionette
==========

Marionette is the remote protocol that lets out-of-process programs
communicate with, instrument, and control Gecko-based browsers.

It provides interfaces for interacting with both the internal
JavaScript runtime and UI elements of Gecko-based browsers, such
as Firefox and Fennec.  It can control both the chrome- and content
documents, giving a high level of control and ability to replicate,
or emulate, user interaction.

Marionette can be activated by passing the `-marionette` flag. To
start Firefox with the remote protocol turned on::

	% firefox -marionette
	…
	1491228343089   Marionette  INFO    Listening on port 2828

This binds to a TCP socket, over which clients can communicate with
Marionette using the `protocol`_.

.. _protocol: Protocol.html

.. toctree::
   :maxdepth: 1

   Intro.md
   PythonTests.md
   Protocol.md
   Contributing.md
   Debugging.md
   Testing.md
   Taskcluster.md
   CodeStyle.md
   Patching.md
   SeleniumAtoms.md
   internals/index

See also:

* Documentation for `Marionette Python client`_, which is used
  in-tree to write many kinds of Marionette-based tests.
* Documentation for `Firefox Puppeteer`_, which is used to in-tree
  to write Firefox UI tests.

.. _Marionette Python client: http://marionette-client.readthedocs.io
.. _Firefox Puppeteer: http://firefox-puppeteer.readthedocs.io


Bugs
====

Bugs are tracked in the `Testing :: Marionette` component.


Communication
=============

The mailing list for Marionette discussion is
tools-marionette@lists.mozilla.org (`subscribe`_, `archive`_).

If you prefer real-time chat, there is often someone in the #ateam IRC
channel on irc.mozilla.org.  Don’t ask if you may ask a question; just go ahead
and ask, and please wait for an answer as we might not be in your timezone.

.. _subscribe: https://lists.mozilla.org/listinfo/tools-marionette
.. _archive: https://groups.google.com/group/mozilla.tools.marionette
