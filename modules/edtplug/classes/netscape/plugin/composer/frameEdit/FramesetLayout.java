/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package netscape.plugin.composer.frameEdit;

import netscape.plugin.composer.*;
import netscape.plugin.composer.io.*;
import java.io.*;
import java.util.Observer;
import java.util.Observable;

import netscape.application.*;
import netscape.util.*;

/** An IFC layout manager that implements the HTML Frames layout policy.
 */

class FramesetLayout implements LayoutManager {
    public void layoutView(View view, int deltawidth, int deltaheight){
        FramesetView baseView = (FramesetView) view;
        Frameset frameset = baseView.frameset();
        boolean horizontal = frameset.horizontal();
        Vector v = view.subviews();
        int length = v.size();
        int h = view.height();
        int w = view.width();
        int margin = 5;
        int totalSize = horizontal ? w : h;
        int totalMargin = (length + 1 ) * margin;
        int[] sizes = frameset.calcSizes(totalSize - totalMargin );
        int x = margin;
        int y = margin;
        if ( horizontal ) {
            h -= 2 * margin;
        }
        else {
            w -= 2 * margin;
        }
        for(int i = 0; i < length; i++){
            int size = sizes[i];
            if ( horizontal ) {
                w = size;
            }
            else {
                h = size;
            }
            View child = (View) v.elementAt(i);
            child.setBounds(x,y,w,h);
            if ( horizontal ) {
                x = x + size + margin;
            }
            else {
                y = y + size + margin;
            }
        }
    }

    public void addSubview(View view) {}
    public void removeSubview(View view) {}
}
