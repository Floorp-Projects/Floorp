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

/** The base view for frame elements.
 */

class FrameBaseView extends View implements DragSource, DragDestination, Observer, Target{
    public FrameBaseView(FrameSelection hook){
        this.hook = hook;
        hook.model().addObserver(this);
        sync();
    }
    public void syncSizes(){
        LayoutManager manager = layoutManager();
        if ( manager != null ) {
            manager.layoutView(this, 0, 0);
            Vector subviews = subviews();
            int length = subviews.size();
            for(int i = 0; i < length; i++ ) {
                FrameBaseView child = (FrameBaseView)
                    subviews.elementAt(i);
                child.syncSizes();
            }
        }
    }
    public void update(Observable o, Object arg){
        if ( o == hook.model() ) {
            String string = (String) arg;
            Application.application().performCommandLater(this, string,null);
        }
    }
    public void performCommand(String command, Object arg){
        if ( FrameModel.NOTE_SELECTION.equals(command)){
            sync();
        }
        else if ( SWAP.equals(command) ){
            doSwap((FrameSelection) arg);
        }
    }

    protected void doSwap(FrameSelection selection){
        hook.swap(selection);
    }

    public boolean mouseDown(MouseEvent event){
        mouseDownInRegion(event, whatClicked(event.x, event.y));
        return true;
    }

    protected void mouseDownInRegion(MouseEvent event, int region){
        if ( region == REGION_CENTER ){
            select(event);
        } else {
            split(event, region);
        }
    }

    protected void select(MouseEvent event){
        hook.model().select(new FrameSelection(hook));
        setFocusedView();
        Rect bounds = new Rect(localBounds());
        if ( hook.parent() != null ) {
            new DragSession(this, new FrameImage(bounds),
                            bounds.x, bounds.y,
                            event.x, event.y,
                            SWAP, hook);
        }
    }

    protected void split(MouseEvent event, int what){
        boolean horizontal = what == REGION_TOP || what == REGION_BOTTOM;
        Rect bounds = localBounds();
        int total = horizontal ? bounds.width : bounds.height;
        int first = horizontal ? event.x - bounds.x : event.y - bounds.y;
        hook.split(horizontal, first * 1.0 / total);
    }
    protected void sync(){
        boolean oldSelected = selected();
        setSelected(hook.model().selection().equals(hook));
    }
    public FrameBaseView getSelectedView() {
        if ( hook.model().selection().element() == hook.element() ){
            return this;
        }
        else {
            return null;
        }
    }
    protected boolean selected(){
        return selected_;
    }
    protected void setSelected(boolean selected){
        if ( selected_ != selected ) {
            selected_ = selected;
            setDirty(true);
        }
    }
    protected void drawSelectionFeedback(Graphics g){
        // Draw selection feedback
        if ( selected() ){
            Rect bounds = localBounds();
            g.setColor(Color.blue);
            for(int i = 0; i < 4; i++){
                bounds.growBy(-1,-1);
                g.drawRect(bounds);
            }
       }
    }

    public boolean isTransparent(){return false;}

    public void drawView(Graphics g){
        if ( dropFeedback ) {
            g.setColor(Color.white);
        }
        else {
            g.setColor(getBackground());
        }
        g.fillRect(0,0,width(),height());
        drawSelectionFeedback(g);
    }
    protected Color getBackground() {
        return Color.lightGray;
    }
    public DragDestination acceptsDrag(DragSession session, int x, int y) {
        if ( acceptData(session) ){
            return this;
        }
        return super.acceptsDrag(session, x, y);
    }
    public boolean dragDropped(DragSession session) {
        boolean result = acceptData(session);
        setDropFeedback(false);
        if (result ) {
            Application.application().performCommandLater(this,
                session.dataType(), session.data());
        }
        return result;
    }
    public void keyDown(KeyEvent event){
        if ( !isInViewHierarchy() ){
           return;
        }
        if ( event.isDeleteKey() ) {
            hook.delete();
        }
        else if ( event.isUpArrowKey() ) {
            hook.selectParent();
        }
        else if ( event.isLeftArrowKey() ) {
            hook.selectPreviousSibling();
        }
        else if ( event.isRightArrowKey() ) {
            hook.selectNextSibling();
        }
        else if ( event.isDownArrowKey() ) {
            hook.selectFirstChild();
        }
    }
    public int cursorForPoint(int x, int y){
        return cursorForRegion(whatClicked(x, y));
    }
    protected int cursorForRegion(int region){
        if ( region == REGION_CENTER ) {
            return ARROW_CURSOR;
        }
        else {
            return CROSSHAIR_CURSOR;
        }
    }
    public boolean dragEntered(DragSession session) {
        boolean result = acceptData(session);
        setDropFeedback(result);
        return result;
    }
    public void dragExited(DragSession session) {
        setDropFeedback(false);
    }
    public boolean dragMoved(DragSession session) {
        return dragEntered(session);
    }

    protected boolean acceptData(DragSession session){
        String dataType = session.dataType();
        return SWAP.equals(dataType) &&
                ! ((FrameSelection) session.data()).element().
                    directlyRelated(element());
    }

    protected void doDrop(FrameElement frameset){
    }

    private void setDropFeedback(boolean feedback){
        if ( feedback != dropFeedback ){
            dropFeedback = feedback;
            setDirty(true);
        }
    }

    FrameElement element() {
        return hook.element();
    }
    public void dragWasAccepted(DragSession session){

    }
    public boolean dragWasRejected(DragSession session){
        return true;
    }
    public View sourceView(DragSession session){
        return this;
    }
    protected int whatClicked(int x, int y){
        return REGION_CENTER;
        /*
        Rect temp = new Rect(localBounds());
        int xMargin = 5 + Math.min(5, temp.width/10);
        int yMargin = 5 + Math.min(5, temp.height/10);
        if ( temp.width >= 20 && temp.height >= 20 ) {
            temp.growBy(-xMargin,-yMargin);
        }
        if ( temp.contains(x, y) ){
            return REGION_CENTER;
        }
        else if ( x < xMargin ) {
            return REGION_LEFT;
        }
        else if ( x > temp.maxX()  ) {
            return REGION_RIGHT;
        }
        else if ( y < yMargin ) {
            return REGION_TOP;
        }
        else {
            return REGION_BOTTOM;
        }
        //*/
    }

    protected static final int REGION_CENTER = 0;
    protected static final int REGION_BAR = 1;
    protected static final int REGION_TOP = 2;
    protected static final int REGION_BOTTOM = 3;
    protected static final int REGION_LEFT = 4;
    protected static final int REGION_RIGHT = 5;

    protected FrameSelection hook;
    private boolean dropFeedback;
    private boolean selected_;
    private final static String SWAP = "swap";
}
