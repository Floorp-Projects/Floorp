===
CDP
===

In addition to the Firefox Developer Tools _Remote Debugging Protocol_,
also known as RDP, Firefox also has a partial implementation of
the Chrome DevTools Protocol (CDP).

The Firefox remote agent is a low-level debugging interface based on
the CDP protocol.  With it, you can inspect the state and control
execution of documents running in web content, instrument Gecko in
interesting ways, simulate user interaction for automation purposes,
and debug JavaScript execution.


.. toctree::
  :maxdepth: 1

  Building.md
  Debugging.md
  Testing.md
  Prefs.md


Bugs
====

Bugs are tracked under the `Remote Protocol`_ product.

.. _Remote Protocol: https://bugzilla.mozilla.org/describecomponents.cgi?product=Remote%20Protocol


Communication
=============

The mailing list for Firefox remote debugging discussion is
`remote@lists.mozilla.org`_ (`subscribe`_, `archive`_).

If you prefer real-time chat, there is often someone in the
#devtools IRC channel on irc.mozilla.org.  Don’t ask if you may
ask a question just go ahead and ask, and please wait for an answer
as we might not be in your timezone.

.. _remote@lists.mozilla.org: mailto:remote@lists.mozilla.org
.. _subscribe: https://lists.mozilla.org/listinfo/remote
.. _archive: https://lists.mozilla.org/pipermail/remote/
