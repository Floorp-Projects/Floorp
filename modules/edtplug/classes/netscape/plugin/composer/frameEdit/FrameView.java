/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
