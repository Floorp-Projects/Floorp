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








