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
import java.util.StringTokenizer;
import netscape.application.*;
import netscape.util.*;

/** Frameset element. Represents the Frameset. Keeps track of the tag and the
 * children of the frameset.
 */

class Frameset extends FrameElement {
    public final static String ROWS="ROWS";
    public final static String COLS="COLS";
    public Frameset(){
        this(true);
    }
    public Frameset(boolean horizontal) {
        this(new Tag("FRAMESET"));
        horizontal_ = horizontal;
        children.insertElementAt(new String("\n"), 0);
    }
    public Frameset(Tag tag){
        super(tag);
        if ( knownTags == null ) {
            knownTags = new netscape.util.Hashtable();
            registerTags(knownTags);
        }
        horizontal_ = tag.containsAttribute(COLS);
    }

    protected void registerTags(netscape.util.Hashtable table){
        super.registerTags(table);
        table.put(ROWS,ROWS);
        table.put(COLS,COLS);
    }

    public Object clone() {
        Frameset result = (Frameset) super.clone();
        result.children = (Vector) children.clone();
        int length = children.size();
        for(int i = 0; i < length; i++ ){
            Object element = children.elementAt(i);
            Object clown = null;
            if ( element instanceof String ) {
                // Don't have to copy. Strings are immutable.
            }
            else if ( element instanceof FrameElement) {
                clown = ((FrameElement) element).clone();
                ((FrameElement) clown).internalSetParent(result);
            }
            else {
                System.err.println("Can't clone");
            }
            if ( clown != null ) {
                if ( clown == result ) {
                    System.err.println("Circularity!");
                }
                result.children.setElementAt(clown, i);
            }
        }
        return result;
    }

    public void read(LexicalStream stream) throws IOException {
        Token token;
        StringBuffer junkBuffer = new StringBuffer();
        String framesetString = "FRAMESET";
        String frameString = "FRAME";
        String noFrameString = "NOFRAMES";
        while ( (token = stream.next() ) != null){
            FrameElement child = null;
            if ( token instanceof Tag ) {
                Tag tag = (Tag) token;
                String tagName = tag.getName();
                if ( tag.isClose() && tagName.equals(framesetString) ) {
                    break;
                }
                else if ( tag.isOpen() ) {
                    if ( tagName.equals(framesetString) ) {
                        child = new Frameset(tag);
                    }
                    else if ( tagName.equals(frameString) ){
                        child = new Frame(tag);
                    }
                    else if ( tagName.equals(noFrameString) ){
                        noFramesJunk_ = junkBuffer.toString();
                        junkBuffer = new StringBuffer();
                        noFrames_ = new NoFrames(tag);
                        noFrames_.read(stream);
                        continue;
                    }
                }
            }
            if ( child == null ) {
                junkBuffer.append(token.toString());
            }
            else {
                child.internalSetParent(this);
                children.addElement(junkBuffer.toString());
                junkBuffer = new StringBuffer();
                child.read(stream);
                children.addElement(child);
            }
        }
        children.addElement(junkBuffer.toString());
    }

    public void write(Writer writer,int depth) throws IOException {
        writeTag(writer, depth, true);
        Enumeration e = children.elements();
        int newDepth = depth + 1;
        while(e.hasMoreElements()){
            Object o = e.nextElement();
            if ( o instanceof FrameElement ) {
                FrameElement child = (FrameElement) o;
                child.write(writer, newDepth);
                if ( ! e.hasMoreElements() ) {
                    if ( noFrames_ != null ) {
                        writer.write('\n');
                        writer.write(noFramesJunk_);
                        noFrames_.write(writer, newDepth);
                        writer.write('\n');
                    }
                }
            }
            else if ( o instanceof String ) {
                if ( ! e.hasMoreElements() ) {
                    if ( noFrames_ != null ) {
                        writer.write(noFramesJunk_);
                        noFrames_.write(writer, newDepth);
                    }
                }
                String s = (String) o;
                writer.write(s);
            }
        }
        writeTag(writer, depth, false);
    }

    // Attribtes

    public boolean horizontal() {
        return tag.containsAttribute(COLS);
    }

    // Operations on children
    public int length() { return children.size() / 2; }

    public FrameElement elementAt(int index ) {
        return (FrameElement) children.elementAt(index * 2 + 1);
    }

    public FrameElement setElementAt(FrameElement element, int index) {
        FrameElement old = (FrameElement) children.elementAt(index * 2+1);
        old.internalSetParent(null);
        element.internalSetParent(this);
        children.setElementAt(element, index * 2+1);
        return old;
    }

    public void addElement(FrameElement element, String size) {
        insertElementAt(element, size, length());
    }

    public void insertElementAt(FrameElement element, String size, int index){
        if ( element.parent() == this ) {
            return;
        }
        element.internalSetParent(this);
        insertSizeAt(size, index);
        children.insertElementAt(new String("\n"), index * 2);
        children.insertElementAt(element, index * 2 + 1);
    }
    public FrameElement removeElementAt(int index ){
        removeSizeAt(index);
        FrameElement element = (FrameElement) children.removeElementAt(index * 2 + 1);
        children.removeElementAt(index * 2);
        element.internalSetParent(null);
        return element;
    }

    public FrameElement remove(FrameElement element ){
        return removeElementAt(indexOf(element));
    }

    public int indexOf(FrameElement element){
        int result = children.indexOf(element);
        if (result > 0 ) {
            result = (result - 1) / 2;
        }
        return result;
    }

    /** Destructively Moves the elements from the set into this.
     * Replaces the item that's already at the index.
     */
    public FrameElement replace(Frameset set, FrameElement child){
        int index = indexOf(child);
        FrameElement result = removeElementAt(index);
        int length = set.length();
        for(int i = 0; i < length; i++ ){
            String size = set.sizeAt(0);
            insertElementAt(set.removeElementAt(0), size, index + i);
        }
        return result;
    }

    static void swap(FrameElement a, FrameElement b){
        if ( a == b ) return;
        Frameset pa = a.parent();
        Frameset pb = b.parent();
        int ia = pa.indexOf(a);
        int ib = pb.indexOf(b);
        String sa = pa.sizeAt(ia);
        String sb = pb.sizeAt(ib);
        Frame dummy = new Frame(new Tag("A"));
        pa.setElementAt(dummy, ia);
        pb.setElementAt(a, ib);
        pa.setElementAt(b, ia);
        if ( pa == pb ) {
            pb.setSizeAt(sa, ib);
            pa.setSizeAt(sb, ia);
        }
    }

    NoFrames noFrames() {
        return noFrames_;
    }

    String noFramesJunk() {
        return noFramesJunk_;
    }

    NoFrames setNoFrames(NoFrames noFrames){
        NoFrames result = noFrames_;
        noFrames_ = noFrames;
        if ( noFrames_ != null && noFramesJunk_ == null ) {
            noFramesJunk_ = "\n";
        }
        return result;
    }

    // Size info
    public String sizeAt(int index) {
        return sizeString().sizeAt(index);
    }
    public String removeSizeAt(int index) {
        SizeString sizeString = sizeString();
        String result = sizeString.removeSizeAt(index);
        setSizeString(sizeString);
        return result;
    }
    public void insertSizeAt(String size, int index) {
        if ( length() == 0 ) {
            setSizeString(new SizeString(size));
        }
        else {
            SizeString sizeString = sizeString();
            sizeString.insertSizeAt(size, index);
            setSizeString(sizeString);
        }
    }
    public String setSizeAt(String size, int index) {
        SizeString sizeString = sizeString();
        String oldSize = sizeString.setSizeAt(size, index);
        setSizeString(sizeString);
        return oldSize;
    }
    protected SizeString sizeString() {
        String result = tag.lookupAttribute(COLS);
        if ( result == null ) {
            result = tag.lookupAttribute(ROWS);
            if ( result == null ) {
                result = "100%";
            }
        }
        return new SizeString(result);
    }
    protected void setSizeString(SizeString sizeString) {
        String string = sizeString.toString();
        if ( horizontal_ || tag.containsAttribute(COLS) ){
            tag.addAttribute(COLS, string);
            horizontal_ = true;
        }
        else {
            tag.addAttribute(ROWS, string);
        }
    }
    public int[] calcSizes(int total){
        SizeString sizeString = sizeString();
        sizeString.normalize(length());
        return sizeString.calcSizes(total);
    }
    protected boolean knownAttribute(String string){
        return knownTags.containsKey(string);
    }

    private Vector children = new Vector(); // even elements are Strings, odd elements are FrameElements
    private String noFramesJunk_;
    private NoFrames noFrames_;
    private static netscape.util.Hashtable knownTags;
    private boolean horizontal_;
}

