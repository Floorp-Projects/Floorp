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

package netscape.plugin.composer;

import java.io.*;
import java.util.*;
import netscape.plugin.composer.io.*;
import java.net.*;
import netscape.javascript.JSObject;

/** An impelementation of the HTML document.
 */
class ComposerDocument extends Document{
    public ComposerDocument(Composer composer, String document, URL base,
        String workDirectory, URL workDirectoryURL, String eventName, URL documentURL,
        JSObject jsobject){
        this.composer = composer;
        this.text = document;
        this.workDirectory = workDirectory;
        this.base = base;
        this.workDirectoryURL = workDirectoryURL;
        this.eventName = eventName;
        this.documentURL = documentURL;
        this.jsobject=jsobject;
    }

    public URL getBase(){
        return base;
    }

    public File getWorkDirectory(){
        return new File(workDirectory);
    }

    public URL getWorkDirectoryURL(){
        return workDirectoryURL;
    }

    public Reader getInput() throws IOException {
        if ( in != null ) {
            throw new IOException("Must close existing input stream first.");
        }
        in = new ComposerDocumentReader(this, text.toCharArray());
        return in;
    }
    public Writer getOutput() throws IOException {
        doOutputCheck();
        out = new ComposerDocumentWriter(this);
        return out;
    }

    private void doOutputCheck() throws IOException {
        if ( out != null ) {
            throw new IOException("Must close existing output stream first.");
        }
    }
    public String getText() {
        return text;
    }
    public void setText(String text) throws IOException {
        doOutputCheck();
        this.text = text;
        composer.newText(text);
    }
    void inputDone(ComposerDocumentReader in) {
        if ( in == this.in ) {
            this.in = null;
        }
        else {
            // Do nothing.
        }
    }
    void outputDone(ComposerDocumentWriter out, String text) {
        if ( out == this.out ) {
            this.out = null;
            try {
                setText(text);
            }
            catch(IOException e){
                System.err.println("Couldn't set text.");
                e.printStackTrace();
            }
        }
        else {
            // Do nothing.
        }
    }

    public URL getDocumentURL(){
        return documentURL;
    }

    public String getEventName(){
        return eventName;
    }

    public void redirectDocumentOpen(String newURL){
        if ( eventName.equals("edit") ){
            composer.newText(newURL);
        }
    }

    /** get the jsobject from the document.  If no javascript in the
     *  runtime, this will return null
     */
    public JSObject getJSObject(){return jsobject;}

    Composer getComposer() { return composer; }

    private Composer composer;
    private String workDirectory;
    private String text;
    private URL base;
    private URL workDirectoryURL;
    private String eventName;
    private URL documentURL;
    private ComposerDocumentReader in;
    private ComposerDocumentWriter out;
    private PluginManager manager;
    private JSObject jsobject;
}

class ComposerDocumentReader extends CharArrayReader {
    public ComposerDocumentReader(ComposerDocument document, char[] text){
        super(text);
        this.document = document;
    }

    public void close() {
        document.inputDone(this);
        super.close();
    }
    ComposerDocument document;
}

class ComposerDocumentWriter extends CharArrayWriter {
    public ComposerDocumentWriter(ComposerDocument document){
        this.document = document;
    }
    public void close() {
        document.outputDone(this, toString());
        super.close();
    }
    ComposerDocument document;
}
