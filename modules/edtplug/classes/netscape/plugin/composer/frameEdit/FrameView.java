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

/** A view of a Frame.
 */

class FrameView extends FrameBaseView implements Target {
    public FrameView(FrameSelection hook){
        super(hook);
        if ( ! (hook.element() instanceof Frame)) {
            System.err.println("Bad frame selection.");
        }
    }
    public boolean isTransparent() { return false; }

    public void drawView(Graphics g){
        if ( element() == null || element().parent() == null
            || element().parent().indexOf(element()) < 0 ){
            return;
        }
        super.drawView(g);
        Rect bounds = localBounds();
        Frame frame = frame();
        g.setColor(Color.black);
        Font font = Font.defaultFont();
        g.setFont(font);
        int x = bounds.x + 5;
        int y = bounds.y + 5;
        int lineHeight = font.fontMetrics().height();
        g.drawString(frame.name(), x, y+ lineHeight);
        g.drawString(frame.source(), x, y + lineHeight*2);
        border.drawInRect(g, bounds);
    }

    protected Color getBackground() {
        return bgColor;
    }
    public void mouseDragged(MouseEvent event){
    }
    protected void doDrop(FrameElement element){
        if ( element instanceof Frameset ) {
            Frameset frameset = (Frameset) element;
            Frameset parent = hook.parent();
            if ( parent != null && frameset.horizontal() == parent.horizontal()){
                // Replace me with the children of this frameset.
                hook.replaceInline(frameset);
            }
            else {
                hook.replace(element);
            }
        }
    }
    protected Frame frame() { return (Frame) element(); }

    protected void drawProperty(Graphics g, int x, int y, String lable, String property){
        g.drawString(lable + " " + property, x, y);
    }
    //static private final String nameLable = FrameEdit.getString("nameLable");
    //static private final String sourceLable = FrameEdit.getString("sourceLable");
    //static private final String sizeLable = FrameEdit.getString("sizeLable");
    private static Border border = BezelBorder.loweredBezel();
    private static final Color bgColor = new Color(240,240,240);
}
