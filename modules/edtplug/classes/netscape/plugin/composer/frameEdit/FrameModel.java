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
import java.util.*;
import java.io.*;

import netscape.application.*;
import netscape.util.*;

/** The model for the frameset document. Delegates to FrameElements to
 * hold the individual FRAMESET, FRAME, and NOFRAMES data.
 */

class FrameModel extends Observable {
    public FrameModel(){
        selection_ = new FrameSelection(this);
    }

    public String newFrameName(){
        return FrameEdit.getString("new frame name") + Integer.toString(++newFrameIndex_);
    }

    public FrameSelection selection() {
        return selection_;
    }

    public void select(FrameElement element){
        select( new FrameSelection(this, element));
    }
    public void select(FrameSelection selection){
        selection_ = selection;
        sync(NOTE_SELECTION);
    }
    public void clearSelection(){
        select(new FrameSelection(this));
    }

    public void read(Reader reader){
        // Search for a frameset
        LexicalStream stream = new LexicalStream(reader);
        Token token;
        String frameSetName = "FRAMESET";
        StringBuffer b = new StringBuffer();
        boolean foundFrameset = false;
        try {
            while ((token = stream.next()) != null){
                if ( ! foundFrameset ) {
                    if ( token instanceof Tag ) {
                        Tag tag = (Tag) token;
                        if ( tag.isOpen() && tag.getName().equals(frameSetName)){
                            foundFrameset = true;
                            prefix = b.toString();
                            b = new StringBuffer();
                            frameset = new Frameset(tag);
                            frameset.read(stream);
                            continue;
                        }
                    }
                }
                b.append(token.toString());
            }
        } catch ( IOException e){
        }
        if ( foundFrameset ) {
            postfix = b.toString();
        }
        else {
            prefix = b.toString();
        }
        if ( prefix.length() == 0 ){
            prefix = "<HTML>\n<BODY>\n</BODY>\n</HTML>\n";
        }
        normalize();
    }
    public void write(Writer writer){
        try {
            writer.write(prefix);
            if ( frameset != null ) {
                frameset.write(writer, 0);
            }
            if ( postfix != null ) {
                writer.write(postfix);
            }
        } catch (IOException e){
        }
    }
    public boolean hasFrames(){
        return frameset != null;
    }
    public Frameset frames(){
        return frameset;
    }
    public void clearFrames(){
        setFrames(null);
    }
    public void setFrames(Frameset frameset){
        if ( frameset != null && this.frameset == null ) {
            // Adding frames for the first time. Pull the
            // body out of the prefix and put it into the frameset.
            int bodyStart = prefix.indexOf("<BODY");
            int bodyEnd = prefix.lastIndexOf("</BODY>");
            if ( bodyEnd < 0 ) {
                bodyEnd = prefix.length();
            }
            if ( bodyStart >= 0 && bodyEnd >= 0 && bodyStart < bodyEnd ){
                postfix = prefix.substring(bodyEnd + 7, prefix.length());
                String body = prefix.substring(bodyStart, bodyEnd+7);
                prefix = prefix.substring(0, bodyStart);
                frameset.setNoFrames(new NoFrames(body));
            }
        }
        else if ( frameset == null && this.frameset != null ) {
            // Removing all frames. Pull the body out of the frameset.
            NoFrames noFrames = this.frameset.noFrames();
            if ( noFrames != null ) {
                prefix = prefix + noFrames.text() + postfix;
                postfix = null;
            }
        }
        this.frameset = frameset;
        structureChanged();
    }

    void normalize(){
        if ( frameset != null ) {
            normalize(frameset);
            if ( frameset.length() == 0 ){
                clearFrames();
                select(new FrameSelection(this));
                structureChanged();
            }
        }
    }
    void normalize(Frameset frameset){
        int length = frameset.length();
        for(int i = length - 1; i >= 0; i--){
            FrameElement e = frameset.elementAt(i);
            if ( e instanceof Frameset){
                Frameset f = (Frameset) e;
                normalize(f);
            }
        }
        // If sizes are percentage, make the sizes add up to 100%
        SizeString sizeString = frameset.sizeString();
        sizeString.normalizePercent();
        frameset.setSizeString(sizeString);
        Frameset parent = frameset.parent();
        if ( parent != null ) {
            length = frameset.length();
            if ( length == 0 ) {
                if ( frameset == selection_.element() ){
                    select(new FrameSelection(this, parent));
                }
                parent.remove(frameset);
                structureChanged();
            }
            else if ( length == 1 ) {
                FrameElement child = frameset.elementAt(0);
                int index = parent.indexOf(frameset);
                String size = parent.sizeAt(index);
                frameset.remove(child);
                parent.insertElementAt(child, size, index + 1);
                parent.removeElementAt(index);
                if ( frameset == selection_.element() ){
                    select(new FrameSelection(this, child));
                }
            }
        }
    }

    void sync(String notification) {
        setChanged();
        notifyObservers(notification);
    }

    void structureChanged(){
        sync(NOTE_STRUCTURE);
    }
    void propertyChanged(FrameElement element){
        sync(NOTE_PROPERTY);
    }

    void sizeChanged(FrameElement element){
        sync(NOTE_SIZE);
    }

    // Undo/Redo logic
    void beginChanges(){
        if ( changeLevel_++ == 0 ) {
            undoStore_ = recordDocument();
        }
    }

    void endChanges(){
        if (changeLevel_>0) {
            changeLevel_--;
        }
    }

    char[] recordDocument(){
        CharArrayWriter writer = new CharArrayWriter(500);
        write(writer);
        return writer.toCharArray();
    }

    void undo(){
        if ( undoStore_ != null ){
            changeLevel_ = 0;
            char[] redoStore = recordDocument();
            read(new CharArrayReader(undoStore_));
            undoStore_ = redoStore;
            clearSelection();
            structureChanged();
        }
    }

    private String prefix;
    private Frameset frameset;
    private String postfix;
    private FrameSelection selection_;
    private char[] undoStore_;
    private int changeLevel_;
    private int newFrameIndex_;

    public final static String NOTE_STRUCTURE= "NOTE_STRUCTURE";
    public final static String NOTE_PROPERTY= "NOTE_PROPERTY";
    public final static String NOTE_SIZE= "NOTE_SIZE";
    public final static String NOTE_SELECTION= "NOTE_SELECTION";
}

