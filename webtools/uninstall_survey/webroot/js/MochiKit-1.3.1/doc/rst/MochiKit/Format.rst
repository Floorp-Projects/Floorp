.. title:: MochiKit.Format - string formatting goes here

Name
====

MochiKit.Format - string formatting goes here


Synopsis
========

::

   assert( truncToFixed(0.12345, 4) == "0.1234" );
   assert( roundToFixed(0.12345, 4) == "0.1235" );
   assert( twoDigitAverage(1, 0) == "0" );
   assert( twoDigitFloat(1.2345) == "1.23" );
   assert( twoDigitFloat(1) == "1" );
   assert( percentFormat(1.234567) == "123.46%" );
   assert( numberFormatter("###,###%")(125) == "12,500%" );
   assert( numberFormatter("##.000")(1.25) == "1.250" );


Description
===========

Formatting strings and stringifying numbers is boring, so a couple useful
functions in that domain live here.


Dependencies
============

None.


Overview
========

Formatting Numbers
------------------

MochiKit provides an extensible number formatting facility, modeled loosely
after the Number Format Pattern Syntax [1]_ from Java.
:mochiref:`numberFormatter(pattern[, placeholder=""[, locale="default"])`
returns a function that converts Number to string using the given information.
``pattern`` is a string consisting of the following symbols:

+-----------+---------------------------------------------------------------+
| Symbol    |   Meaning                                                     |
+===========+===============================================================+
| ``-``     |   If given, used as the position of the minus sign            |
|           |   for negative numbers. If not given, the position            |
|           |   to the left of the first number placeholder is used.        |
+-----------+---------------------------------------------------------------+
| ``#``     |   The placeholder for a number that does not imply zero       |
|           |   padding.                                                    |
+-----------+---------------------------------------------------------------+
| ``0``     |   The placeholder for a number that implies zero padding.     |
|           |   If it is used to the right of a decimal separator, it       |
|           |   implies trailing zeros, otherwise leading zeros.            |
+-----------+---------------------------------------------------------------+
| ``,``     |   The placeholder for a "thousands separator". May be used    |
|           |   at most once, and it must be to the left of a decimal       |
|           |   separator. Will be replaced by ``locale.separator`` in the  |
|           |   result (the default is also ``,``).                         |
+-----------+---------------------------------------------------------------+
| ``.``     |   The decimal separator. The quantity of ``#`` or ``0``       |
|           |   after the decimal separator will determine the precision of |
|           |   the result. If no decimal separator is present, the         |
|           |   fractional precision is ``0`` -- meaning that it will be    |
|           |   rounded to the nearest integer.                             |
+-----------+---------------------------------------------------------------+
| ``%``     |   If present, the number will be multiplied by ``100`` and    |
|           |   the ``%`` will be replaced by ``locale.percent``.           |
+-----------+---------------------------------------------------------------+


API Reference
=============

Functions
---------

:mochidef:`formatLocale(locale="default")`:

    Return a locale object for the given locale. ``locale`` may be either a
    string, which is looked up in the ``MochiKit.Format.LOCALE`` object, or
    a locale object. If no locale is given, ``LOCALE.default`` is used
    (equivalent to ``LOCALE.en_US``).


:mochidef:`lstrip(str, chars="\\s")`:

    Returns a string based on ``str`` with leading whitespace stripped.

    If ``chars`` is given, then that expression will be used instead of
    whitespace. ``chars`` should be a string suitable for use in a ``RegExp``
    ``[character set]``.


:mochidef:`numberFormatter(pattern, placeholder="", locale="default")`:

    Return a function ``formatNumber(aNumber)`` that formats numbers
    as a string according to the given pattern, placeholder and locale.

    ``pattern`` is a string that describes how the numbers should be formatted,
    for more information see `Formatting Numbers`_.

    ``locale`` is a string of a known locale (en_US, de_DE, fr_FR, default) or
    an object with the following fields:

    +-----------+-----------------------------------------------------------+
    | separator | The "thousands" separator for this locale (en_US is ",")  |
    +-----------+-----------------------------------------------------------+
    | decimal   | The decimal separator for this locale (en_US is ".")      |
    +-----------+-----------------------------------------------------------+
    | percent   | The percent symbol for this locale (en_US is "%")         |
    +-----------+-----------------------------------------------------------+


:mochidef:`percentFormat(someFloat)`:

    Roughly equivalent to: ``sprintf("%.2f%%", someFloat * 100)``

    In new code, you probably want to use:
    :mochiref:`numberFormatter("#.##%")(someFloat)` instead.


:mochidef:`roundToFixed(aNumber, precision)`:

    Return a string representation of ``aNumber``, rounded to ``precision``
    digits with trailing zeros. This is similar to
    ``Number.toFixed(aNumber, precision)``, but this has implementation
    consistent rounding behavior (some versions of Safari round 0.5 down!)
    and also includes preceding ``0`` for numbers less than ``1`` (Safari,
    again).

    For example, :mochiref:`roundToFixed(0.1357, 2)` returns ``0.14`` on every
    supported platform, where some return ``.13`` for ``(0.1357).toFixed(2)``.


:mochidef:`rstrip(str, chars="\\s")`:

    Returns a string based on ``str`` with trailing whitespace stripped.

    If ``chars`` is given, then that expression will be used instead of
    whitespace. ``chars`` should be a string suitable for use in a ``RegExp``
    ``[character set]``.


:mochidef:`strip(str, chars="\\s")`:

    Returns a string based on ``str`` with leading and trailing whitespace
    stripped (equivalent to :mochiref:`lstrip(rstrip(str, chars), chars)`).

    If ``chars`` is given, then that expression will be used instead of
    whitespace. ``chars`` should be a string suitable for use in a ``RegExp``
    ``[character set]``.


:mochidef:`truncToFixed(aNumber, precision)`:

    Return a string representation of ``aNumber``, truncated to ``precision``
    digits with trailing zeros. This is similar to
    ``aNumber.toFixed(precision)``, but this truncates rather than rounds and
    has implementation consistent behavior for numbers less than 1.
    Specifically, :mochiref:`truncToFixed(aNumber, precision)` will always have a
    preceding ``0`` for numbers less than ``1``.

    For example, :mochiref:`toFixed(0.1357, 2)` returns ``0.13``.


:mochidef:`twoDigitAverage(numerator, denominator)`:

    Calculate an average from a numerator and a denominator and return
    it as a string with two digits of precision (e.g. "1.23").

    If the denominator is 0, "0" will be returned instead of ``NaN``.


:mochidef:`twoDigitFloat(someFloat)`:

    Roughly equivalent to: ``sprintf("%.2f", someFloat)``

    In new code, you probably want to use
    :mochiref:`numberFormatter("#.##")(someFloat)` instead.


See Also
========

.. [1] Java Number Format Pattern Syntax:
       http://java.sun.com/docs/books/tutorial/i18n/format/numberpattern.html


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
