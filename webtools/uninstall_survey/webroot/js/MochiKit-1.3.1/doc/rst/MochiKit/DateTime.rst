.. title:: MochiKit.DateTime - "what time is it anyway?"

Name
====

MochiKit.DateTime - "what time is it anyway?"


Synopsis
========

::

   stringDate = toISOTimestamp(new Date());
   dateObject = isoTimestamp(stringDate);


Description
===========

Remote servers don't give you JavaScript Date objects, and they certainly
don't want them from you, so you need to deal with string representations
of dates and timestamps. MochiKit.Date does that.


Dependencies
============

None.


API Reference
=============

Functions
---------

:mochidef:`isoDate(str)`:

    Convert an ISO 8601 date (YYYY-MM-DD) to a ``Date`` object.


:mochidef:`isoTimestamp(str)`:

    Convert any ISO 8601 [1]_ timestamp (or something reasonably close to it)
    to a ``Date`` object. Will accept the "de facto" form:

        YYYY-MM-DD hh:mm:ss

    or (the proper form):

        YYYY-MM-DDThh:mm:ssZ

    If a time zone designator ("Z" or "[+-]HH:MM") is not present, then the
    local timezone is used.


:mochidef:`toISOTime(date)`:

    Convert a ``Date`` object to a string in the form of hh:mm:ss


:mochidef:`toISOTimestamp(date, realISO=false)`:

    Convert a ``Date`` object to something that's ALMOST but not quite an
    ISO 8601 [1]_timestamp. If it was a proper ISO timestamp it would be:

        YYYY-MM-DDThh:mm:ssZ

    However, we see junk in SQL and other places that looks like this:

        YYYY-MM-DD hh:mm:ss

    So, this function returns the latter form, despite its name, unless
    you pass ``true`` for ``realISO``.


:mochidef:`toISODate(date)`:

    Convert a ``Date`` object to an ISO 8601 [1]_ date string (YYYY-MM-DD)


:mochidef:`americanDate(str)`:

    Converts a MM/DD/YYYY date to a ``Date`` object


:mochidef:`toPaddedAmericanDate(date)`:

    Converts a ``Date`` object to an MM/DD/YYYY date, e.g. 01/01/2001


:mochidef:`toAmericanDate(date)`:

    Converts a ``Date`` object to an M/D/YYYY date, e.g. 1/1/2001


See Also
========

.. [1] W3C profile of ISO 8601: http://www.w3.org/TR/NOTE-datetime


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
