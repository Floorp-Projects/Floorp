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
import java.util.*;
import java.io.*;

import netscape.application.*;
import netscape.util.*;

/** The frame selection. Used to specify a part of the frame model and
 * perform operations on the frame model.
 */

class FrameSelection {
    /** Select top.
     */

    public FrameSelection(FrameModel model){
        this(model, model.frames());
    }
    public FrameSelection(FrameSelection selection){
        this(selection.model(), selection.element());
    }
    public FrameSelection(FrameModel model, FrameElement element){
        model_ = model;
        element_ = element;
    }
    public boolean equals(Object other){
        boolean result = false;
        if ( other instanceof FrameSelection ){
            FrameSelection o = (FrameSelection) other;
            result = model_ == o.model_ && element_ == o.element_;
        }
        return result;
    }

    public void selectParent(){
        Frameset parent = parent();
        if ( parent != null ) {
            model().select(new FrameSelection(model(), parent));
        }
    }
    public void selectPreviousSibling(){
        Frameset parent = parent();
        if ( parent != null ) {
            int index = index();
            if ( index > 0 ) {
                model().select(new FrameSelection(model(), parent.elementAt(index - 1)));
            }
        }
    }
    public void selectNextSibling(){
        Frameset parent = parent();
        if ( parent != null ) {
            int index = index();
            if ( index < parent.length() - 1 ) {
                model().select(new FrameSelection(model(), parent.elementAt(index + 1)));
            }
        }
    }
    public void selectFirstChild(){
        if ( element() instanceof Frameset ){
            Frameset frameset = (Frameset) element();
            if ( frameset.length() > 0 ) {
                model().select(new FrameSelection(model(), frameset.elementAt(0)));
            }
        }
    }
    public void selectRoot(){
        Frameset parent = parent();
        Frameset lastFound = null;
        while ( parent != null ){
            lastFound = parent;
            parent = parent.parent();
        }
        if ( lastFound != null ) {
            model().select(new FrameSelection(model(), lastFound));
        }
    }

    public void propertyChanged(){
        model().propertyChanged(element());
    }
    public void sizeChanged(){
        model().sizeChanged(element());
    }
    public void replace(FrameElement newElement){
        begin();
        try {
            Frameset parent = parent();
            FrameElement element = element();
            if ( parent != null) {
                parent.setElementAt(newElement, index());
            }
            else {
                model().setFrames((Frameset) newElement);
            }
            if ( element != null && newElement instanceof Frameset ) {
                Frameset newFrameset = (Frameset) newElement;
                newFrameset.setElementAt(element, 0);
                model().select(newFrameset.elementAt(1));
            }
            else {
                model().select(newElement);
            }
            model().sync(FrameModel.NOTE_STRUCTURE);
        } finally {
            end();
        }
    }
    public void replaceInline(Frameset frameset){
        begin();
        Frameset parent = parent();
        FrameElement element = element();
        String size = element.size();
        int index = parent.indexOf(element);
        parent.replace(frameset, element);
        parent.setElementAt(element, index);
        element.setSize(size);
        model().select(parent.elementAt(index + 1));
        model().sync(FrameModel.NOTE_STRUCTURE);
        end();
    }
    public void delete(){
        begin();
        Frameset parent = parent();
        if ( parent != null) {
            int index = index();
            parent.removeElementAt(index());
            int length = parent.length();
            if ( index > 0 ) {
                index--;
            }
            if ( index < length){
                model().select(new FrameSelection(model(), parent.elementAt(index)));
            }
            else {
                model().select(new FrameSelection(model(), parent));
            }
        }
        else {
            model().clearFrames();
            model().select(parent);
        }
        model().normalize();
        model().structureChanged();
        end();
    }
    public void split(boolean horizontal, double parametric)
    {
        begin();
        FrameElement element = element();
        Frameset parent = null;
        if ( element instanceof Frameset ) {
            parent = (Frameset) element;
            int children = parent.length();
            if ( children > 0 ) {
                element = parent.elementAt(children-1);
            }
            else {
                element = null;
            }
        }
        else {
            parent = element != null ? element.parent() : null;
        }
        if ( parent != null && element != null && parent.horizontal() == horizontal ) {
            // insert inline
            SizeString size = parent.sizeString();
            int index = parent.indexOf(element);
            size.splitAt(parametric, index);
            Frame copy = new Frame(model().newFrameName()); // (Frame) element.clone();
            parent.insertElementAt(copy, size.sizeAt(index+1), index+1);
            parent.elementAt(index).setSize(size.sizeAt(index));
            model().select(copy);
            model().structureChanged();
        }
        else if ( parent != null ){
            // insert child
            Frameset frameset = new Frameset(horizontal);
            int index = 0;
            if ( element == null ) {
                element = new Frame(model().newFrameName());
            }
            else {
                index = parent.indexOf(element);
            }
            Frame secondFrame = new Frame(model().newFrameName());
            SizeString size = new SizeString("100%");
            size.splitAt(parametric, 0);
            parent.setElementAt(frameset, index);
            frameset.addElement(element, size.sizeAt(0));
            frameset.addElement(secondFrame, size.sizeAt(1));
            model().select(secondFrame);
        }
        else {
            Frame firstFrame = new Frame(model().newFrameName());
            Frame secondFrame = new Frame(model().newFrameName());
            SizeString size = new SizeString("100%");
            size.splitAt(parametric, 0);
            Frameset frameset = new Frameset(horizontal);
            frameset.addElement(firstFrame, size.sizeAt(0));
            frameset.addElement(secondFrame, size.sizeAt(1));
            model().setFrames(frameset);
            model().select(secondFrame);
        }
        model().structureChanged();
        end();
    }
    public Frameset parent() {
        if ( element_ == null ) {
            return null;
        }
        return element_.parent();
    }
    public FrameElement element() {
        return element_;
    }
    public int index() {
        Frameset parent = parent();
        if ( parent == null ) return 0;
        return parent.indexOf(element_);
    }
    public FrameModel model() {
        return model_;
    }

    public void swap(FrameSelection other){
        begin();
        Frameset.swap(element(), other.element());
        model().structureChanged();
        model().select(other.element());
        end();
    }

    public void setName(String name){
        Frame frame = (Frame) element();
        frame.setName(name);
        propertyChanged();
    }

    public void setSource(String url){
        Frame frame = (Frame) element();
        frame.setSource(url);
        propertyChanged();
    }

    public void setFrameBorder(boolean value){
        element().setFrameBorder(value);
        propertyChanged();
    }

    public void setSize(String value){
        element().setSize(value);
        propertyChanged();
    }

    public void setBorderColor(boolean value){
        element().setBorderColor(value);
        propertyChanged();
    }

    public void setBorderColor(String value){
        element().setBorderColor(value);
        propertyChanged();
    }

    public void setBorderColor(Color color){
        StringBuffer buffer = new StringBuffer("#");
        append2HexDigets(buffer, color.red());
        append2HexDigets(buffer, color.green());
        append2HexDigets(buffer, color.blue());
        setBorderColor(buffer.toString());
    }
    private void append2HexDigets(StringBuffer buffer, int x){
        String string = Integer.toHexString(x);
        if ( string.length() < 2 ) {
            buffer.append('0');
        }
        buffer.append(string);
    }
    public void setMarginWidth(String value){
        element().setMarginWidth(value);
        propertyChanged();
    }

    public void setMarginHeight(String value){
        element().setMarginHeight(value);
        propertyChanged();
    }

    public void setBorder(String value){
        element().setBorder(value);
        propertyChanged();
    }

    public void setExtraHTML(String value){
        element().setExtraHTML(value);
        propertyChanged();
    }

    public void setAllowResize(boolean value){
        ((Frame) element()).setAllowResize(value);
        propertyChanged();
    }

    public void setScrolling(int value){
        ((Frame) element()).setScrolling(value);
        propertyChanged();
    }

    public void begin() {
        model_.beginChanges();
    }
    public void end() {
        model_.endChanges();
    }
    private FrameModel model_;
    private FrameElement element_;
}
