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
import netscape.constructor.*;
import netscape.plugin.composer.io.*;
import java.io.*;
import java.util.Observer;
import java.util.Observable;

import netscape.application.*;
import netscape.util.*;

/** An adapter class for use with the Constructor Plan file. Receives commands
 * from the Plan objects and turns them into method calls on the
 * current selection.
 */

class FramePropertyTarget implements Observer, Target, ExtendedTarget, TextFieldOwner {
    public FramePropertyTarget(FrameModel model, Plan plan){
        model_ = model;
        plan_ = plan;
        frameProps_ = (ContainerView) plan_.componentNamed("Frame Controls");
        framesetProps_ = (ContainerView) plan_.componentNamed("Frameset Controls");
        framePropsParent_ = frameProps_.superview();
        framesetPropsParent_ = framesetProps_.superview();
        /* Drop target set up specially */
        ColorWell well = (ColorWell) plan_.componentNamed(BORDER_COLOR_WELL);
        well.setCommand(BORDER_COLOR_WELL);
        well.setTarget(TargetChain.applicationChain());
        sync();
    }
    public void update(Observable o, Object arg){
        performCommand((String) arg, o);
    }

    public boolean canPerformCommand(String command){
        return NAME.equals(command)
            || SOURCE_URL.equals(command)
            || RESIZABLE.equals(command)
            || SCROLLING.equals(command)
            || VISIBLE_BORDER.equals(command)
            || BORDER_COLOR.equals(command)
            || FRAME_SIZE.equals(command)
            || MARGIN_WIDTH.equals(command)
            || MARGIN_HEIGHT.equals(command)
            || BORDER_SIZE.equals(command)
            || BORDER_COLOR_WELL.equals(command)
            || EXTRA_HTML.equals(command)
            ;
    }
    public void performCommand(String command, Object arg){
        FrameSelection hook = model_.selection();
        FrameElement element = hook.element();

        if ( NAME.equals(command) ){
            hook.setName(trim(arg));
        }
        else if ( SOURCE_URL.equals(command) ){
            hook.setSource(trim(arg));
        }
        else if ( RESIZABLE.equals(command) ){
            hook.setAllowResize(((Button) arg).state());
        }
        else if ( SCROLLING.equals(command) ){
            hook.setScrolling(((Popup) arg).selectedIndex());
        }
        else if ( VISIBLE_BORDER.equals(command) ){
            hook.setFrameBorder(((Button) arg).state());
        }
        else if ( BORDER_COLOR.equals(command) ){
            hook.setBorderColor(((Button) arg).state());
        }
        else if ( FRAME_SIZE.equals(command) ){
            hook.setSize(trim(arg));
        }
        else if ( MARGIN_WIDTH.equals(command) ){
            hook.setMarginWidth(trim(arg));
        }
        else if ( MARGIN_HEIGHT.equals(command) ){
            hook.setMarginHeight(trim(arg));
        }
        else if ( BORDER_SIZE.equals(command) ){
            hook.setBorder(trim(arg));
        }
        else if ( BORDER_COLOR_WELL.equals(command) ){
            hook.setBorderColor(((ColorWell) arg).color());
        }
        else if ( EXTRA_HTML.equals(command) ){
            hook.setExtraHTML(trim(arg));
        }
        else if ( FrameModel.NOTE_PROPERTY.equals(command) ) {
            sync();
        }
        else if ( FrameModel.NOTE_SELECTION.equals(command) ) {
            resignFocus();
            sync();
        }
        else if ( FrameModel.NOTE_SIZE.equals(command) ) {
            syncSize();
        }
    }
    private String trim(Object obj){
        return ((TextField) obj).stringValue();
    }
    private void showProps(boolean showFrameset, boolean showFrame){
        showProps(showFrameset, framesetProps_, framesetPropsParent_);
        showProps(showFrame, frameProps_, framePropsParent_);
    }

    private void showProps(boolean show, ContainerView view, View parent){
        if ( show != view.isInViewHierarchy()){
            if ( show ) {
                parent.addSubview(view);
            }
            else {
                // If this field contains the focused view,
                // resign focus.
                resignFocus(view);
                view.removeFromSuperview();
            }
            parent.addDirtyRect(view.bounds());
        }
    }

    private void resignFocus(){
        resignFocus(framesetProps_);
        resignFocus(frameProps_);
    }

    private void resignFocus(View view){
        InternalWindow window = view.window();
        if ( window != null ) {
            View focusedView = window.focusedView();
            if ( focusedView != null ) {
                View temp = focusedView;
                while ( temp != null && temp != window && temp != view ) {
                    temp = temp.superview();
                }
                if ( temp == view ) {
                    // Yep, the focused view is here.
                    if ( focusedView instanceof TextField ) {
                        TextField focusedField = (TextField) focusedView;
                        // Have to do this, or else we'll crash later when
                        // the
                        focusedField.setStringValue("");
                        focusedField.selectText();
                    }
                    else {
                        focusedView.stopFocus();
                    }
                }
            }
        }
    }
    private void sync(){
        FrameElement element = element();
        if ( element == null ){
            showProps(false,false);
        }
        else {
            setIfNew(FRAME_SIZE, element.size());
            setIfNew(MARGIN_WIDTH, element.marginWidth());
            setIfNew(MARGIN_HEIGHT, element.marginHeight());
            setIfNew(BORDER_COLOR, element.hasBorderColor());
            setIfNewColor(BORDER_COLOR_WELL, element.borderColor());
            setIfNew(BORDER_SIZE, element.border());
            String newExtra = element.extraHTML();
            setIfNew(EXTRA_HTML, newExtra);
            setIfNew(VISIBLE_BORDER, element.frameBorder());
            if ( element instanceof Frame ) {
                showProps(true,true);
                Frame frame = (Frame) element;
                setIfNew(NAME, frame.name());
                setIfNew(SOURCE_URL, frame.source());
                setIfNew(RESIZABLE, frame.allowResize());
                setIfNewPopup(SCROLLING, frame.scrolling());
            }
            else {
                showProps(true,false);
            }
        }
    }
    private void syncSize(){
        FrameElement element = element();
        if ( element != null ){
          setIfNew(FRAME_SIZE, element.size());
        }
    }
    protected FrameElement element(){
        return  model_.selection().element();
    }
    protected void setIfNew(String fieldName, String value){
        TextField field = (TextField) plan_.componentNamed(fieldName);
        field.setOwner(this);
        if ( ! field.stringValue().equals(value)){
            field.setStringValue(value);
        }
    }
    public void textEditingDidBegin(TextField textField){
    }
    public void textWasModified(TextField textField){
        String command = textField.command();
        // Since EXTRA_HTML is a synthetic property, it is better to
        // allow it to complete before accessing it.
        if ( ! command.equals(EXTRA_HTML) ){
            performCommand(command,textField);
        }
    }
    public boolean textEditingWillEnd(TextField textField, int endCondition,
                                      boolean contentsChanged){
        return true;
    }
    public void textEditingDidEnd(TextField textField, int endCondition,
                                  boolean contentsChanged){
                                  }

    protected void setIfNew(String checkBoxName, boolean value){
        Button checkBox = (Button) plan_.componentNamed(checkBoxName);
        if ( checkBox.state() != value ){
            checkBox.setState(value);
        }
    }
    protected void setIfNewColor(String fieldName, String value){
        ColorWell well = (ColorWell) plan_.componentNamed(fieldName);
        Color color = well.color();
        Color newColor = stringToColor(value);
        if ( ! well.color().equals(newColor)){
            well.setColor(newColor);
        }
    }
    private Color stringToColor(String value){
        if (value != null && value.length() == 7 && value.charAt(0) == '#' ){
            try {
                return new Color(Integer.parseInt(value.substring(1),16));
            } catch ( NumberFormatException e){
            }
        }
        return Color.black;
    }
    protected void setIfNewPopup(String fieldName, int value){
        Popup popup = (Popup) plan_.componentNamed(fieldName);
        if ( popup.selectedIndex() != value){
            popup.selectItemAt(value);
        }
    }
    Plan plan_;
    FrameModel model_;
    ContainerView frameProps_;
    ContainerView framesetProps_;
    View framePropsParent_;
    View framesetPropsParent_;
    private final static String NAME="Name";
    private final static String SOURCE_URL="Source URL";
    private final static String RESIZABLE="Resizable";
    private final static String SCROLLING="Scrolling";
    private final static String VISIBLE_BORDER="Visible Border";
    private final static String BORDER_COLOR="Border Color";
    private final static String FRAME_SIZE="Frame Size";
    private final static String MARGIN_WIDTH="Margin Width";
    private final static String MARGIN_HEIGHT="Margin Height";
    private final static String BORDER_SIZE="Border Size";
    private final static String BORDER_COLOR_WELL="Border Color Well";
    private final static String EXTRA_HTML="Extra HTML";
}
