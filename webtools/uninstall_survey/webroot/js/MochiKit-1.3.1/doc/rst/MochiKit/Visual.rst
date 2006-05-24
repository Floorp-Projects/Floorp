.. title:: MochiKit.Visual - visual effects

Name
====

MochiKit.Visual - visual effects


Synopsis
========

::

    // round the corners of all h1 elements
    roundClass("h1", null);
    
    // round the top left corner of the element with the id "title"
    roundElement("title", {corners: "tl"});
    

Description
===========

MochiKit.Visual provides visual effects and support functions for visuals.


Dependencies
============

- :mochiref:`MochiKit.Base`
- :mochiref:`MochiKit.Iter`
- :mochiref:`MochiKit.DOM`
- :mochiref:`MochiKit.Color`

Overview
========

At this time, MochiKit.Visual provides one visual effect: rounded corners
for your HTML elements. These rounded corners are created completely
through CSS manipulations and require no external images or style sheets.
This implementation was adapted from Rico_.

.. _Rico: http://www.openrico.org


API Reference
=============

Functions
---------

:mochidef:`roundClass(tagName[, className[, options]])`:

    Rounds all of the elements that match the ``tagName`` and ``className``
    specifiers, using the options provided. ``tagName`` or ``className`` can
    be ``null`` to match all tags or classes. For more information about
    the options, see the :mochiref:`roundElement` function.


:mochidef:`roundElement(element[, options])`:

    Immediately round the corners of the specified element.
    The element can be given as either a string 
    with the element ID, or as an element object.
    
    The options mapping has the following defaults:

    ========= =================
    corners   ``"all"``
    color     ``"fromElement"``
    bgColor   ``"fromParent"``
    blend     ``true``
    border    ``false``
    compact   ``false``
    ========= =================
    
    corners:

        specifies which corners of the element should be rounded.
        Choices are:
        
        - all
        - top
        - bottom
        - tl (top left)
        - bl (bottom left)
        - tr (top right)
        - br (bottom right)

        Example:
            ``"tl br"``: top-left and bottom-right corners are rounded
    
    blend:
        specifies whether the color and background color should be blended
        together to produce the border color.
    

See Also
========

.. [1] Application Kit Reference - NSColor: http://developer.apple.com/documentation/Cocoa/Reference/ApplicationKit/ObjC_classic/Classes/NSColor.html
.. [2] SVG 1.0 color keywords: http://www.w3.org/TR/SVG/types.html#ColorKeywords
.. [3] W3C CSS3 Color Module: http://www.w3.org/TR/css3-color/#svg-color


Authors
=======

- Kevin Dangoor <dangoor@gmail.com>
- Bob Ippolito <bob@redivi.com>
- Originally adapted from Rico <http://openrico.org/> (though little remains)


Copyright
=========

Copyright 2005 Bob Ippolito <bob@redivi.com>. This program is dual-licensed
free software; you can redistribute it and/or modify it under the terms of the
`MIT License`_ or the `Academic Free License v2.1`_.

.. _`MIT License`: http://www.opensource.org/licenses/mit-license.php
.. _`Academic Free License v2.1`: http://www.opensource.org/licenses/afl-2.1.php

Portions adapted from `Rico`_ are available under the terms of the
`Apache License, Version 2.0`_.

.. _`Apache License, Version 2.0`: http://www.apache.org/licenses/LICENSE-2.0.html
