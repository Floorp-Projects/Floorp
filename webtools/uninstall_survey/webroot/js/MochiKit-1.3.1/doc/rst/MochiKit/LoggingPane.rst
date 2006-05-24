.. title:: MochiKit.LoggingPane - Interactive MochiKit.Logging pane

Name
====

MochiKit.LoggingPane - Interactive MochiKit.Logging pane


Synopsis
========

::

    // open a pop-up window
    createLoggingPane()
    // use a div at the bottom of the document
    createLoggingPane(true);


Description
===========

MochiKit.Logging does not have any browser dependencies and is completely
unobtrusive. MochiKit.LoggingPane is a browser-based colored viewing pane
for your :mochiref:`MochiKit.Logging` output that can be used as a pop-up or
inline.

It also allows for regex and level filtering!  MochiKit.LoggingPane is used
as the default :mochiref:`MochiKit.Logging.debuggingBookmarklet()` if it is
loaded.


Dependencies
============

- :mochiref:`MochiKit.Base`
- :mochiref:`MochiKit.Logging`


API Reference
=============

Constructors
------------

:mochidef:`LoggingPane(inline=false, logger=MochiKit.Logging.logger)`:

    A listener for a :mochiref:`MochiKit.Logging` logger with an interactive
    DOM representation.

    If ``inline`` is ``true``, then the ``LoggingPane`` will be a ``DIV``
    at the bottom of the document. Otherwise, it will be in a pop-up
    window with a name based on the calling page's URL. If there is an
    element in the document with an id of ``_MochiKit_LoggingPane``,
    it will be used instead of appending a new ``DIV`` to the body. 

    ``logger`` is the reference to the :mochiref:`MochiKit.Logging.Logger` to
    listen to. If not specified, the global default logger is used.
    
    Properties:

        ``win``:
            Reference to the pop-up window (``undefined`` if ``inline``)

        ``inline``:
            ``true`` if the ``LoggingPane`` is inline

        ``colorTable``:
            An object with property->value mappings for each log level
            and its color. May also be mutated on ``LoggingPane.prototype``
            to affect all instances. For example::

                MochiKit.LoggingPane.LoggingPane.prototype.colorTable = {
                    DEBUG: "green",
                    INFO: "black",
                    WARNING: "blue",
                    ERROR: "red",
                    FATAL: "darkred"
                };


:mochidef:`LoggingPane.prototype.closePane()`:

    Close the :mochiref:`LoggingPane` (close the child window, or
    remove the ``_MochiKit_LoggingPane`` ``DIV`` from the document).


Functions
---------


:mochidef:`createLoggingPane(inline=false)`:

    Create or return an existing :mochiref:`LoggingPane` for this document
    with the given inline setting. This is preferred over using
    :mochiref:`LoggingPane` directly, as only one :mochiref:`LoggingPane`
    should be present in a given document.


Authors
=======

- Bob Ippolito <bob@redivi.com>


Copyright
=========

Copyright 2005 Bob Ippolito <bob@redivi.com>. This program is dual-licensed
free software; you can redistribute it and/or modify it under the terms of the
`MIT License`_ or the `Academic Free License v2.1`_.

.. _`MIT License`: http://www.opensource.org/licenses/mit-license.php
.. _`Academic Free License v2.1`: http://www.opensource.org/licenses/afl-2.1.php
