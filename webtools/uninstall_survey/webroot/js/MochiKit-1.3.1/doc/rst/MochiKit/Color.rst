.. title:: MochiKit.Color - color abstraction with CSS3 support

Name
====

MochiKit.Color - color abstraction with CSS3 support


Synopsis
========

::

    // RGB color expressions are supported
    assert(
        objEqual(Color.whiteColor(), Color.fromString("rgb(255,100%, 255)"))
    );

    // So is instantiating directly from HSL or RGB values.
    // Note that fromRGB and fromHSL take numbers between 0.0 and 1.0!
    assert( objEqual(Color.fromRGB(1.0, 1.0, 1.0), Color.fromHSL(0.0, 0.0, 1.0) );

    // Or even SVG color keyword names, as per CSS3!
    assert( Color.fromString("aquamarine"), "#7fffd4" );
        
    // NSColor-like colors built in
    assert( Color.whiteColor().toHexString() == "#ffffff" );
    

Description
===========

MochiKit.Color is an abstraction for handling colors and strings that
represent colors.


Dependencies
============

- :mochiref:`MochiKit.Base`


Overview
========

MochiKit.Color provides an abstraction of RGB, HSL and HSV colors with alpha.
It supports parsing and generating of CSS3 colors, and has a full CSS3 (SVG)
color table.

All of the functionality in this module is exposed through a Color constructor
and its prototype, but a few of its internals are available for direct use at
module level.


API Reference
=============

Constructors
------------

:mochidef:`Color()`:

    Represents a color. Component values should be integers between ``0.0``
    and ``1.0``. You should use one of the :mochiref:`Color` factory
    functions such as :mochiref:`Color.fromRGB`, :mochiref:`Color.fromHSL`,
    etc. instead of constructing :mochiref:`Color` objects directly.

    :mochiref:`Color` instances can be compared with
    :mochiref:`MochiKit.Base.compare` (though ordering is on RGB, so is not
    particularly meaningful except for equality), and the default ``toString``
    implementation returns :mochiref:`Color.prototype.toHexString()`.

    :mochiref:`Color` instances are immutable, and much of the architecture is
    inspired by AppKit's NSColor [1]_ 


:mochidef:`Color.fromBackground(elem)`:

    Returns a :mochiref:`Color` object based on the background of the provided
    element. Equivalent to::

        c = Color.fromComputedStyle(
            elem, "backgroundColor", "background-color") || Color.whiteColor();
    

:mochidef:`Color.fromComputedStyle(elem, style, mozillaEquivalentCSS)`:
    
    Returns a :mochiref:`Color` object based on the result of 
    :mochiref:`MochiKit.DOM.computedStyle(elem, style, mozillaEquivalentCSS)`
    or ``null`` if not found.


:mochidef:`Color.fromHexString(hexString)`:

    Returns a :mochiref:`Color` object from the given hexadecimal color string.
    For example, ``"#FFFFFF"`` would return a :mochiref:`Color` with
    RGB values ``[255/255, 255/255, 255/255]`` (white).


:mochidef:`Color.fromHSL(hue, saturation, lightness, alpha=1.0)`:

    Return a :mochiref:`Color` object from the given ``hue``, ``saturation``,
    ``lightness`` values. Values should be numbers between ``0.0`` and
    ``1.0``.

    If ``alpha`` is not given, then ``1.0`` (completely opaque) will be used.

    Alternate form:
        :mochiref:`Color.fromHSL({h: hue, s: saturation, l: lightness, a: alpha})`


:mochidef:`Color.fromHSLString(hslString)`:

    Returns a :mochiref:`Color` object from the given decimal hsl color string.
    For example, ``"hsl(0,0%,100%)"`` would return a :mochiref:`Color` with
    HSL values ``[0/360, 0/360, 360/360]`` (white).


:mochidef:`Color.fromHSV(hue, saturation, value, alpha=1.0)`:

    Return a :mochiref:`Color` object from the given ``hue``, ``saturation``,
    ``value`` values. Values should be numbers between ``0.0`` and
    ``1.0``.

    If ``alpha`` is not given, then ``1.0`` (completely opaque) will be used.

    Alternate form:
        :mochiref:`Color.fromHSV({h: hue, s: saturation, v: value, a: alpha})`


:mochidef:`Color.fromName(colorName)`:

    Returns a :mochiref:`Color` object corresponding to the given
    SVG 1.0 color keyword name [2]_ as per the W3C CSS3
    Color Module [3]_. ``"transparent"`` is also accepted
    as a color name, and will return :mochiref:`Color.transparentColor()`.


:mochidef:`Color.fromRGB(red, green, blue, alpha=1.0)`:

    Return a :mochiref:`Color` object from the given ``red``, ``green``,
    ``blue``, and ``alpha`` values. Values should be numbers between ``0``
    and ``1.0``.

    If ``alpha`` is not given, then ``1.0`` (completely opaque) will be used.

    Alternate form:
        :mochiref:`Color.fromRGB({r: red, g: green, b: blue, a: alpha})`


:mochidef:`Color.fromRGBString(rgbString)`:

    Returns a :mochiref:`Color` object from the given decimal rgb color string.
    For example, ``"rgb(255,255,255)"`` would return a :mochiref:`Color` with
    RGB values ``[255/255, 255/255, 255/255]`` (white).


:mochidef:`Color.fromText(elem)`:

    Returns a :mochiref:`Color` object based on the text color of the provided
    element. Equivalent to::

        c = Color.fromComputedStyle(elem, "color") || Color.whiteColor();


:mochidef:`Color.fromString(rgbOrHexString)`:

    Returns a :mochiref:`Color` object from the given RGB, HSL, hex, or name.
    Will return ``null`` if the string can not be parsed by any of these 
    methods.

    See :mochiref:`Color.fromHexString`, :mochiref:`Color.fromRGBString`, 
    :mochiref:`Color.fromHSLString` and :mochiref:`Color.fromName` more
    information.
    

:mochidef:`Color.namedColors()`:

    Returns an object with properties for each SVG 1.0 color keyword
    name [2]_ supported by CSS3 [3]_. Property names are the color keyword
    name in lowercase, and the value is a string suitable for
    :mochiref:`Color.fromString()`.


:mochidef:`Color.prototype.colorWithAlpha(alpha)`:

    Return a new :mochiref:`Color` based on this color, but with the provided
    ``alpha`` value.


:mochidef:`Color.prototype.colorWithHue(hue)`:

    Return a new :mochiref:`Color` based on this color, but with the provided
    ``hue`` value.


:mochidef:`Color.prototype.colorWithSaturation(saturation)`:

    Return a new :mochiref:`Color` based on this color, but with the provided
    ``saturation`` value (using the HSL color model).


:mochidef:`Color.prototype.colorWithLightness(lightness)`:

    Return a new :mochiref:`Color` based on this color, but with the provided
    ``lightness`` value.


:mochidef:`Color.prototype.darkerColorWithLevel(level)`:

    Return a new :mochiref:`Color` based on this color, but darker by the given
    ``level`` (between ``0`` and ``1.0``).


:mochidef:`Color.prototype.lighterColorWithLevel(level)`:

    Return a new :mochiref:`Color` based on this color, but lighter by the given
    ``level`` (between ``0`` and ``1.0``).


:mochidef:`Color.prototype.blendedColor(other, fraction=0.5)`:

    Return a new :mochiref:`Color` whose RGBA component values are a weighted sum
    of this color and ``other``. Each component of the returned color
    is the ``fraction`` of other's value plus ``1 - fraction`` of this
    color's.


:mochidef:`Color.prototype.isLight()`:

    Return ``true`` if the lightness value of this color is greater than
    ``0.5``.

    Note that ``alpha`` is ignored for this calculation (color components
    are not premultiplied).


:mochidef:`Color.prototype.isDark()`:

    Return ``true`` if the lightness value of this color is less than or
    equal to ``0.5``.

    Note that ``alpha`` is ignored for this calculation (color components
    are not premultiplied).


:mochidef:`Color.prototype.toRGBString()`:

    Return the decimal ``"rgb(red, green, blue)"`` string representation of this
    color.
    
    If the alpha component is not ``1.0`` (fully opaque), the
    ``"rgba(red, green, blue, alpha)"`` string representation will be used.

    For example::

        assert( Color.whiteColor().toRGBString() == "rgb(255,255,255)" );


:mochidef:`Color.prototype.toHSLString()`:

    Return the decimal ``"hsl(hue, saturation, lightness)"``
    string representation of this color.

    If the alpha component is not ``1.0`` (fully opaque), the
    ``"hsla(hue, saturation, lightness, alpha)"`` string representation
    will be used.

    For example::

        assert( Color.whiteColor().toHSLString() == "hsl(0,0,360)" );


:mochidef:`Color.prototype.toHexString()`:

    Return the hexadecimal ``"#RRGGBB"`` string representation of this color.

    Note that the alpha component is completely ignored for hexadecimal
    string representations!

    For example::

        assert( Color.whiteColor().toHexString() == "#FFFFFF" );


:mochidef:`Color.prototype.asRGB()`:

    Return the RGB (red, green, blue, alpha) components of this color as an
    object with ``r``, ``g``, ``b``, and ``a`` properties that have
    values between ``0.0`` and ``1.0``.


:mochidef:`Color.prototype.asHSL()`:

    Return the HSL (hue, saturation, lightness, alpha) components of this
    color as an object with ``h``, ``s``, ``l`` and ``a`` properties
    that have values between ``0.0`` and ``1.0``.


:mochidef:`Color.prototype.asHSV()`:

    Return the HSV (hue, saturation, value, alpha) components of this
    color as an object with ``h``, ``s``, ``v`` and ``a`` properties
    that have values between ``0.0`` and ``1.0``.


:mochidef:`Color.blackColor()`:

    Return a :mochiref:`Color` object whose RGB values are 0, 0, 0
    (#000000).


:mochidef:`Color.blueColor()`:
    
    Return a :mochiref:`Color` object whose RGB values are 0, 0, 1
    (#0000ff).


:mochidef:`Color.brownColor()`:

    Return a :mochiref:`Color` object whose RGB values are 0.6, 0.4, 0.2
    (#996633).


:mochidef:`Color.cyanColor()`:

    Return a :mochiref:`Color` object whose RGB values are 0, 1, 1
    (#00ffff).


:mochidef:`Color.darkGrayColor()`:

    Return a :mochiref:`Color` object whose RGB values are 1/3, 1/3, 1/3
    (#555555).


:mochidef:`Color.grayColor()`:

    Return a :mochiref:`Color` object whose RGB values are 0.5, 0.5, 0.5
    (#808080).


:mochidef:`Color.greenColor()`:

    Return a :mochiref:`Color` object whose RGB values are 0, 1, 0.
    (#00ff00).


:mochidef:`Color.lightGrayColor()`:

    Return a :mochiref:`Color` object whose RGB values are 2/3, 2/3, 2/3
    (#aaaaaa).


:mochidef:`Color.magentaColor()`:

    Return a :mochiref:`Color` object whose RGB values are 1, 0, 1
    (#ff00ff).


:mochidef:`Color.orangeColor()`:

    Return a :mochiref:`Color` object whose RGB values are 1, 0.5, 0
    (#ff8000).


:mochidef:`Color.purpleColor()`:

    Return a :mochiref:`Color` object whose RGB values are 0.5, 0, 0.5
    (#800080).


:mochidef:`Color.redColor()`:

    Return a :mochiref:`Color` object whose RGB values are 1, 0, 0
    (#ff0000).


:mochidef:`Color.whiteColor()`:

    Return a :mochiref:`Color` object whose RGB values are 1, 1, 1
    (#ffffff).


:mochidef:`Color.yellowColor()`:

    Return a :mochiref:`Color` object whose RGB values are 1, 1, 0
    (#ffff00).


:mochidef:`Color.transparentColor()`:

    Return a :mochiref:`Color` object that is completely transparent
    (has alpha component of 0).


Functions
---------

:mochidef:`clampColorComponent(num, scale)`:

    Returns ``num * scale`` clamped between ``0`` and ``scale``.

    :mochiref:`clampColorComponent` is not exported by default when using JSAN.


:mochidef:`hslToRGB(hue, saturation, lightness, alpha)`:

    Computes RGB values from the provided HSL values. The return value is a
    mapping with ``"r"``, ``"g"``, ``"b"`` and ``"a"`` keys.
    
    Alternate form:
        :mochiref:`hslToRGB({h: hue, s: saturation, l: lightness, a: alpha})`.

    :mochiref:`hslToRGB` is not exported by default when using JSAN.


:mochidef:`hsvToRGB(hue, saturation, value, alpha)`:

    Computes RGB values from the provided HSV values. The return value is a
    mapping with ``"r"``, ``"g"``, ``"b"`` and ``"a"`` keys.
    
    Alternate form:
        :mochiref:`hsvToRGB({h: hue, s: saturation, v: value, a: alpha})`.

    :mochiref:`hsvToRGB` is not exported by default when using JSAN.


:mochidef:`toColorPart(num)`:

    Convert num to a zero padded hexadecimal digit for use in a hexadecimal
    color string. Num should be an integer between ``0`` and ``255``.

    :mochiref:`toColorPart` is not exported by default when using JSAN.


:mochidef:`rgbToHSL(red, green, blue, alpha)`:

    Computes HSL values based on the provided RGB values. The return value is
    a mapping with ``"h"``, ``"s"``, ``"l"`` and ``"a"`` keys.
    
    Alternate form:
        :mochiref:`rgbToHSL({r: red, g: green, b: blue, a: alpha})`.

    :mochiref:`rgbToHSL` is not exported by default when using JSAN.


:mochidef:`rgbToHSV(red, green, blue, alpha)`:

    Computes HSV values based on the provided RGB values. The return value is
    a mapping with ``"h"``, ``"s"``, ``"v"`` and ``"a"`` keys.
    
    Alternate form:
        :mochiref:`rgbToHSV({r: red, g: green, b: blue, a: alpha})`.

    :mochiref:`rgbToHSV` is not exported by default when using JSAN.


See Also
========

.. [1] Application Kit Reference - NSColor: http://developer.apple.com/documentation/Cocoa/Reference/ApplicationKit/ObjC_classic/Classes/NSColor.html
.. [2] SVG 1.0 color keywords: http://www.w3.org/TR/SVG/types.html#ColorKeywords
.. [3] W3C CSS3 Color Module: http://www.w3.org/TR/css3-color/#svg-color


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
