.. title:: MochiKit Documentation Index

Distribution
============

MochiKit - makes JavaScript suck a bit less

- :mochiref:`MochiKit.Async` - manage asynchronous tasks
- :mochiref:`MochiKit.Base` - functional programming and useful comparisons
- :mochiref:`MochiKit.DOM` - painless DOM manipulation API
- :mochiref:`MochiKit.Color` - color abstraction with CSS3 support
- :mochiref:`MochiKit.DateTime` - "what time is it anyway?"
- :mochiref:`MochiKit.Format` - string formatting goes here
- :mochiref:`MochiKit.Iter` - itertools for JavaScript; iteration made HARD,
  and then easy
- :mochiref:`MochiKit.Logging` - we're all tired of ``alert()``
- :mochiref:`MochiKit.LoggingPane` - interactive :mochiref:`MochiKit.Logging`
  pane
- :mochiref:`MochiKit.Signal` - simple universal event handling
- :mochiref:`MochiKit.Visual` - visual effects


Notes
=====

To turn on MochiKit's compatibility mode, do this before loading MochiKit::

    <script type="text/javascript">MochiKit = {__compat__: true};</script>

When compatibility mode is on, you must use fully qualified names for all
MochiKit functions (e.g. ``MochiKit.Base.map(...)``).


Screencasts
===========

- `MochiKit 1.1 Intro`__

.. __: http://mochikit.com/screencasts/MochiKit_Intro-1

See Also
========

.. _`mochikit.com`: http://mochikit.com/
.. _`from __future__ import *`: http://bob.pythonmac.org/
.. _`MochiKit on JSAN`: http://openjsan.org/doc/b/bo/bob/MochiKit/
.. _`MochiKit tag on del.icio.us`: http://del.icio.us/tag/mochikit
.. _`MochiKit tag on Technorati`: http://technorati.com/tag/mochikit
.. _`Google Groups: MochiKit`: http://groups.google.com/group/mochikit

- `Google Groups: MochiKit`_: The official mailing list for discussions
  related to development of and with MochiKit
- `mochikit.com`_: MochiKit's home on the web
- `from __future__ import *`_: Bob Ippolito's blog
- `MochiKit on JSAN`_: the JSAN distribution page for MochiKit
- `MochiKit tag on del.icio.us`_: Recent bookmarks related to MochiKit
- `MochiKit tag on Technorati`_: Recent blog entries related to MochiKit


Version History
===============

.. include:: VersionHistory.rst


Copyright
=========

Copyright 2005 Bob Ippolito <bob@redivi.com>. This program is dual-licensed
free software; you can redistribute it and/or modify it under the terms of the
`MIT License`_ or the `Academic Free License v2.1`_.

.. _`MIT License`: http://www.opensource.org/licenses/mit-license.php
.. _`Academic Free License v2.1`: http://www.opensource.org/licenses/afl-2.1.php
