#!/usr/bin/env python3

import sys

import fontforge

em = 1000

for urgency in range(0, 7 + 1):
    font = fontforge.font()
    font.em = em
    font.ascent = font.hhea_ascent = font.os2_typoascent = em
    font.descent = font.hhea_descent = font.os2_typodescent = 0
    size = (urgency + 1) * em
    name = "font_%dem" % (urgency + 1)
    font.fontname = name
    font.familyname = name
    font.fullname = name

    glyph = font.createChar(ord(" "), "space")
    glyph.width = size

    glyph = font.createChar(ord("M"))
    pen = glyph.glyphPen()
    pen.moveTo(0, 0)
    pen.lineTo(0, size)
    pen.lineTo(size, size)
    pen.lineTo(size, 0)
    pen.closePath()
    glyph.width = size

    path = "%s.woff2" % name
    print("Generating %s..." % path, end="")
    font.generate(path)
    if font.validate() == 0:
        print(" done.")
    else:
        print(" validation error!")
        sys.exit(1)
