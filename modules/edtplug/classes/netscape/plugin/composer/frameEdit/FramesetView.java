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

/** A view of a Frameset.
 */

class FramesetView extends FrameBaseView {
    public FramesetView(FrameSelection hook){
        super(hook);
        if ( ! (hook.element() instanceof Frameset)) {
            System.err.println("Bad frame selection.");
        }
        setLayoutManager(new FramesetLayout());
        sync2();
    }
    public boolean isTransparent() { return false; }

    public void drawView(Graphics g){
        if ( element() == null || (element().parent() != null
            && element().parent().indexOf(element()) < 0 )){
            return;
        }
        super.drawView(g);
        border.drawInRect(g, localBounds());
    }
    private void sync2(){
        // Remove existing subviews
        while(subviews().size() > 0){
            View subview = (View) subviews().elementAt(0);
            subview.removeFromSuperview();
        }
        // Insert new subviews
        Frameset frameset = (Frameset) element();
        int length = frameset.length();
        for(int i = 0; i < length; i++ ){
            View v = null;
            FrameElement e = frameset.elementAt(i);
            if ( e.parent() != frameset){
                System.err.println("Fpoo!");
            }
            FrameSelection hook2 = new FrameSelection(hook.model(), e);
            if ( e instanceof Frame ) {
                v = new FrameView(hook2);
            }
            else if ( e instanceof Frameset ) {
                v = new FramesetView(hook2);
            }
            if ( v != null ) {
                addSubview(v);
            }
        }
    }
    public FrameBaseView getSelectedView() {
        if ( hook.model().selection().element() == hook.element() ){
            return this;
        }
        else {
            Vector children = subviews();
            int length = children.size();
            for(int i = 0; i < length; i++ ){
                FrameBaseView c = (FrameBaseView) children.elementAt(i);
                FrameBaseView c2 = c.getSelectedView();
                if ( c2 != null ) {
                    return c2;
                }
            }
        }
        return null;
    }

    protected int cursorForRegion(int region){
        if ( region == REGION_BAR ) {
            return frameset().horizontal() ? W_RESIZE_CURSOR : N_RESIZE_CURSOR;
        }
        else {
            return ARROW_CURSOR;
        }
    }

    protected int whatClicked(int x, int y){
        if ( inBar(x, y) >= 0 ){
            return REGION_BAR;
        }
        return REGION_CENTER;
    }

    public void mouseDragged(MouseEvent event){
        if ( dragBar >= 0 ) {
            Frameset frameset = frameset();
            boolean horizontal = frameset.horizontal();
            int pos = horizontal ? event.x : event.y;
            int total = horizontal ? width() : height();
            Vector subviews = subviews();
            FrameBaseView view0 = (FrameBaseView) subviews.elementAt(dragBar);
            FrameBaseView view1 = (FrameBaseView) subviews.elementAt(dragBar+1);
            int width0 = horizontal ? view0.width() : view0.height();
            int width1 = horizontal ? view1.width() : view1.height();
            int subTotalWidth = width0 + width1 + 5;
            int dragBase = horizontal ? view0.x() : view0.y();
            double ratio = Math.min(1.0, Math.max(0.0, ((double)(pos - dragBase)) / subTotalWidth));
            int totalSize = horizontal ? width() : height();
            SizeString size = frameset.sizeString();
            size.moveSizeAt(totalSize, ratio, dragBar);
            frameset.setSizeString(size);
            hook.model().sizeChanged(frameset);
        }
    }

    public void mouseUp(MouseEvent event){
        hook.end();
        dragBar = -1;
    }
    protected void moveBar(MouseEvent event){
        dragBar = inBar(event.x, event.y);
    }
    protected void mouseDownInRegion(MouseEvent event, int region){
        if ( region == REGION_BAR ) {
            hook.begin();
            moveBar(event);
        } else {
            super.mouseDownInRegion(event, region);
        }
    }

    /** Returns the bar that has been clicked on... or -1.
     * bar is 0..length - 1;
     */
    protected int inBar(int x, int y){
        Frameset frameset = frameset();
        boolean horizontal = frameset.horizontal();
        int u = horizontal ? x : y;
        int v = horizontal ? y : x;
        int vMax = horizontal ? height() : width();
        if ( v < 5 || v >= vMax - 5 ) {
            return -1;
        }
        Vector subviews = subviews();
        int length = subviews.size();
        for(int i = 0; i < length-1; i++ ) {
            View v0 = (View) subviews.elementAt(i);
            View v1 = (View) subviews.elementAt(i+1);
            Rect bounds0 = v0.bounds();
            Rect bounds1 = v1.bounds();
            int end0 = horizontal ? bounds0.maxX() : bounds0.maxY();
            int start1 = horizontal ? bounds1.x : bounds1.y;
            if ( end0 <= u && u < start1 ) {
                return i;
            }
        }
        return -1;
    }
    public Frameset frameset(){
        return (Frameset) element();
    }
    private static Border border = BezelBorder.raisedBezel();
    private int dragBar;
}
