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

/** The view that displays the entire FrameModel. It delegates to child views
 * to display the framesets and frames that make up the whole model.
 */

class FrameEditView extends View implements Observer, Target {
    public FrameEditView(FrameModel model){
        this.model_ = model;
        sync();
        setLayoutManager(new LayoutSameSizeAsParent());
        model_.addObserver(this);
    }
    public void update(Observable o, Object arg){
        if ( o == model_ ) {
            Application.application().performCommandLater(this, (String) arg,null);
        }
    }
    public void performCommand(String command, Object arg){
        if ( FrameModel.NOTE_STRUCTURE.equals(command)){
            sync();
        }
        else if ( FrameModel.NOTE_SELECTION.equals(command)){
            syncSelection();
        }
        else if ( FrameModel.NOTE_SIZE.equals(command)){
            syncSizes();
        }
        else if ( FrameModel.NOTE_PROPERTY.equals(command)){
            syncSizes();
        }
    }

    public boolean isTransparent() { return false; }

    private void syncSizes(){
        FrameBaseView contentView = (FrameBaseView)
            subviews().elementAt(0);
        contentView.syncSizes();
        setDirty(true);
    }
    private void sync(){
        // Remove existing subviews
        while(subviews().size() > 0){
            View subview = (View) subviews().elementAt(0);
            subview.removeFromSuperview();
        }
        // Add new views
        FrameSelection top = new FrameSelection(model_);
        if ( model_.hasFrames() ) {
            child_ = new FramesetView(top);
        }
        else {
            child_ = new PlainDocumentView(top);
        }
        addSubview(child_);
        layoutView(0,0);
        setDirty(true);

        syncSelection();
    }
    void syncSelection(){
        // Find selected view
        FrameBaseView selected = child_.getSelectedView();
        if ( selected != null ) {
            selected.setFocusedView();
        }
    }
    protected final static String SYNC = "sync";
    protected FrameModel model_;
    private FrameBaseView child_;
}








