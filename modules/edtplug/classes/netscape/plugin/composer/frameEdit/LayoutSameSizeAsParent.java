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

/** A simple layout manager for the MonoGlyph pattern.
 * Lays out the child view(s) to be the same size as the parent view.
 */

class LayoutSameSizeAsParent implements LayoutManager {
    public void layoutView(View view, int deltawidth, int deltaheight){
        Vector v = view.subviews();
        int length = v.size();
        int h = view.height();
        int w = view.width();
        for(int i = 0; i < length; i++){
            View child = (View) v.elementAt(i);
            child.setBounds(0,0,w,h);
        }
    }
    public void addSubview(View view) {}
    public void removeSubview(View view) {}
}
