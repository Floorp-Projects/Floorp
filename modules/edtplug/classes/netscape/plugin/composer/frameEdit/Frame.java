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

/** The model element for a frame.
 */

class Frame extends FrameElement {
    public final static String SRC="SRC";
    public final static String NAME="NAME";
    public final static String NORESIZE="NORESIZE";
    public final static String SCROLLING="SCROLLING";
    public final static int SCROLLING_NO=0;
    public final static int SCROLLING_YES=1;
    public final static int SCROLLING_AUTO=2;
    public Frame(){
        this(new Tag("FRAME"));
    }
    public Frame(String name){
        this();
        setName(name);
    }
    public Frame(Tag tag){
        super(tag);
        if ( knownTags == null ) {
            knownTags = new netscape.util.Hashtable();
            registerTags(knownTags);
        }
    }
    protected void registerTags(netscape.util.Hashtable table){
        super.registerTags(table);
        table.put(SRC,SRC);
        table.put(NAME,NAME);
        table.put(NORESIZE,NORESIZE);
        table.put(SCROLLING,SCROLLING);
    }
    public String source() {
        String result = tag.lookupAttribute(SRC);
        if ( result == null ) {
            result = new String();
        }
        return result;
    }
    public String name() {
        String result = tag.lookupAttribute(NAME);
        if ( result == null ) {
            result = new String();
        }
        return result;
    }
    public void setName(String name){
        tag.addAttribute(NAME, name);
    }
    public void setSource(String source){
        tag.addAttribute(SRC, source);
    }
    public void setScrolling(int scrolling){
        String s;
        if ( scrolling == SCROLLING_NO ) {
            s = "NO";
        }
        else if (scrolling == SCROLLING_YES) {
            s = "YES";
        }
        else {
            tag.removeAttribute(SCROLLING);
            return;
        }
        tag.addAttribute(SCROLLING, s);
    }
    public int scrolling(){
        String result = tag.lookupAttribute(SCROLLING);
        if ( result == null ) {
            return SCROLLING_AUTO;
        }
        else if (result.equals("NO")){
            return SCROLLING_NO;
        }
        else if (result.equals("YES")){
            return SCROLLING_YES;
        }
        return SCROLLING_AUTO;
    }
    public boolean allowResize(){
        return ! tag.containsAttribute(NORESIZE);
    }

    public void setAllowResize(boolean value){
        if ( value ) {
            tag.removeAttribute(NORESIZE);
        }
        else {
            tag.addAttribute(NORESIZE, "");
        }
    }
    protected boolean knownAttribute(String string){
        return knownTags.containsKey(string);
    }
    static netscape.util.Hashtable knownTags;
    private Button noResize;
}

