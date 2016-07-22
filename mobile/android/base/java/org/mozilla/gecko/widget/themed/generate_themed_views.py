#!/bin/python

# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Script to generate Themed*.java source files for Fennec.

This script runs the preprocessor on a input template and writes
updated files into the source directory.

To update the themed views, update the input template
(ThemedView.java.frag) and run the script using 'mach python <script.py>'.  Use version control to
examine the differences, and don't forget to commit the changes to the
template and the outputs.
'''

from __future__ import (
    print_function,
    unicode_literals,
)

import os

from mozbuild.preprocessor import Preprocessor

__DIR__ = os.path.dirname(os.path.abspath(__file__))

template = os.path.join(__DIR__, 'ThemedView.java.frag')
dest_format_string = 'Themed%(VIEW_NAME_SUFFIX)s.java'

views = [
    dict(VIEW_NAME_SUFFIX='EditText',
         BASE_TYPE='android.widget.EditText',
         STYLE_CONSTRUCTOR=1),
    dict(VIEW_NAME_SUFFIX='FrameLayout',
         BASE_TYPE='android.widget.FrameLayout',
         STYLE_CONSTRUCTOR=1),
    dict(VIEW_NAME_SUFFIX='ImageButton',
         BASE_TYPE='android.widget.ImageButton',
         STYLE_CONSTRUCTOR=1,
         TINT_FOREGROUND_DRAWABLE=1,
         BOOKMARK_NO_TINT=1),
    dict(VIEW_NAME_SUFFIX='ImageView',
         BASE_TYPE='android.widget.ImageView',
         STYLE_CONSTRUCTOR=1,
         TINT_FOREGROUND_DRAWABLE=1),
    dict(VIEW_NAME_SUFFIX='LinearLayout',
         BASE_TYPE='android.widget.LinearLayout'),
    dict(VIEW_NAME_SUFFIX='RelativeLayout',
         BASE_TYPE='android.widget.RelativeLayout',
         STYLE_CONSTRUCTOR=1),
    dict(VIEW_NAME_SUFFIX='TextSwitcher',
         BASE_TYPE='android.widget.TextSwitcher'),
    dict(VIEW_NAME_SUFFIX='TextView',
         BASE_TYPE='android.widget.TextView',
         STYLE_CONSTRUCTOR=1),
    dict(VIEW_NAME_SUFFIX='View',
         BASE_TYPE='android.view.View',
         STYLE_CONSTRUCTOR=1),
]

for view in views:
    pp = Preprocessor(defines=view, marker='//#')

    dest = os.path.join(__DIR__, dest_format_string % view)
    with open(template, 'rU') as input:
        with open(dest, 'wt') as output:
            pp.processFile(input=input, output=output)
            print('%s' % dest)
