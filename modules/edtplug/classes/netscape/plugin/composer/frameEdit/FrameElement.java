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

/** Base class for frame elements.
 */

class FrameElement implements Cloneable {
    public final static String BORDERCOLOR="BORDERCOLOR";
    public final static String FRAMEBORDER="FRAMEBORDER";
    public final static String MARGINHEIGHT="MARGINHEIGHT";
    public final static String MARGINWIDTH="MARGINWIDTH";
    public final static String BORDER="BORDER";
    public FrameElement(Tag tag){
        this(null, tag);
    }
    public FrameElement(Frameset parent, Tag tag){
        this.tag = tag;
        this.parent_ = parent;
    }

    public Frameset parent() {
        return parent_;
    }

    protected void internalSetParent(Frameset parent) {
        if ( parent != null && parent_ != null ){
            System.err.println("Already in tree.");
            return;
        }
        parent_ = parent;
    }
    public Object clone() {
        FrameElement result = null;
        try {
            result = (FrameElement) super.clone();
        } catch (CloneNotSupportedException e){
            e.printStackTrace();
        }
        result.parent_ = null;
        // result.tag = tag.clone();
        result.tag = new Tag(tag.getName());
        java.util.Enumeration e = tag.getAttributes();
        while(e.hasMoreElements()){
            String attribute = (String) e.nextElement();
            result.tag.addAttribute(attribute, tag.lookupAttribute(attribute));
        }
        return result;
    }

    public void read(LexicalStream stream) throws IOException {
    }

    public void write (Writer writer, int depth) throws IOException {
        writeTag(writer, depth, true);
    }

    protected void writeTag(Writer writer, int depth, boolean open)
     throws IOException {
        writePrefix(writer, depth);
        if ( open ) {
            writer.write(tag.toString());
        }
        else {
            writer.write("</"+tag.getName()+">");
        }
    }

    protected void writePrefix(Writer writer, int depth)
     throws IOException {
        for(int i = 0; i < depth; i++){
            writer.write("  ");
        }
    }
    public String size(){
        if ( parent() != null ){
            return parent().sizeAt(parent().indexOf(this));
        }
        else {
            return "*";
        }
    }
    public String setSize(String newSize){
        if ( parent() != null ){
            return parent().setSizeAt(newSize, parent().indexOf(this));
        }
        else {
            return "*";
        }
    }

    public String borderColor() {
        return tag.lookupAttribute(BORDERCOLOR);
    }

    public boolean hasBorderColor() {
        return tag.containsAttribute(BORDERCOLOR);
    }

    public void setBorderColor(boolean value){
        if ( tag.containsAttribute(BORDERCOLOR) != value ){
            if ( value ) {
                tag.addAttribute(BORDERCOLOR, "#000000");
            }
            else {
                tag.removeAttribute(BORDERCOLOR);
            }
        }
    }

    public void setBorderColor(String value){
        tag.addAttribute(BORDERCOLOR, value);
    }

    public String marginWidth() {
        String result = tag.lookupAttribute(MARGINWIDTH);
        if ( result == null ) {
            result = "";
        }
        return result;
    }

    public void setMarginWidth(String value){
        tag.addAttribute(MARGINWIDTH, value);
    }

    public String marginHeight() {
        String result = tag.lookupAttribute(MARGINHEIGHT);
        if ( result == null ) {
            result = "";
        }
        return result;
    }

    public void setMarginHeight(String value){
        tag.addAttribute(MARGINHEIGHT, value);
    }

    public String border() {
        String result = tag.lookupAttribute(BORDER);
        if ( result == null ) {
            result = "";
        }
        return result;
    }

    public void setBorder(String value){
        tag.addAttribute(BORDER, value);
    }

    public boolean frameBorder(){
        String frameBorder = tag.lookupAttribute(FRAMEBORDER);
        if ( frameBorder == null ){
            return true;
        }
        if ( frameBorder.equals("NO") || frameBorder.equals("0") ){
            return false;
        }
        return true;
    }

    public void setFrameBorder(boolean value){
        tag.addAttribute(FRAMEBORDER, value ? "YES" : "NO");
    }

    public String extraHTML(){
        return getAttributes(false);
    }

    public String getAttributes(boolean known){
        StringBuffer buffer = new StringBuffer();
        java.util.Enumeration attributes = tag.getAttributes();
        Tag tag2 = new Tag("Z");
        while(attributes.hasMoreElements()){
            String attribute = (String) attributes.nextElement();
            if ( knownAttribute(attribute) == known ){
                String value = tag.lookupAttribute(attribute);
                tag2.addAttribute(attribute, value);
            }
        }
        String temp = tag2.toString();
        // <Z xxxxx>
        int length = temp.length();
        if ( length <= 3 ){
            temp = "";
        }
        else {
            temp = temp.substring(3,length-1);
        }
        return temp;
    }

    public void setExtraHTML(String html){
        StringBuffer newBuffer = new StringBuffer();
        newBuffer.append("<");
        newBuffer.append(tag.getName());
        newBuffer.append(" ");
        newBuffer.append(getAttributes(true));
        newBuffer.append(" ");
        newBuffer.append(html);
        newBuffer.append(">");
        String newTagString = newBuffer.toString();
        StringReader stringReader = new StringReader(newTagString);
        LexicalStream stream = new LexicalStream(stringReader);
        try {
            Token token = stream.next();
            if ( token instanceof Tag ) {
                Tag newTag = (Tag) token;
                if ( newTag.getName().equals(tag.getName())){
                    tag = newTag;
                }
            }
        } catch ( IOException e) {
        }
    }
    protected boolean knownAttribute(String string){
        return false;
    }

    public boolean ancestorOf(FrameElement other){
        while(other != null ) {
            if ( other == this ) {
                return true;
            }
            other = other.parent();
        }
        return false;
    }
    public boolean directlyRelated(FrameElement other){
        return ancestorOf(other) || other.ancestorOf(this);
    }
    protected void registerTags(netscape.util.Hashtable table){
        table.put(BORDER,BORDER);
        table.put(BORDERCOLOR,BORDERCOLOR);
        table.put(FRAMEBORDER,FRAMEBORDER);
        table.put(MARGINHEIGHT,MARGINHEIGHT);
        table.put(MARGINWIDTH,MARGINWIDTH);
    }
    public Tag tag;
    private Frameset parent_;
}
