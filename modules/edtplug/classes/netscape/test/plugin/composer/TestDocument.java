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

package netscape.test.plugin.composer;

import java.io.*;
import java.awt.*;
import java.net.*;
import netscape.plugin.composer.*;
/** An implementation of Document which is used for testing.
 */

class TestDocument extends Document {
    public TestDocument(Test test, String document, URL base, String workDirectory,
        URL workDirectoryURL, URL documentURL, String eventName){
        this.test = test;
        this.text = document;
        this.workDirectory = workDirectory;
        this.base = base;
        this.workDirectoryURL = workDirectoryURL;
        this.documentURL = documentURL;
        this.eventName = eventName;
    }
    public URL getBase(){
        return base;
    }
    public File getWorkDirectory(){
        try {
            return new File(workDirectory);
        } catch ( Throwable e ) {
            e.printStackTrace();
            return null;
        }
    }
    public URL getWorkDirectoryURL(){
        return workDirectoryURL;
    }

    public Reader getInput() throws IOException {
        if ( in != null ) {
            throw new IOException("Must close existing input stream first.");
        }
        in = new TestDocumentReader(this, text.toCharArray());
        return in;
    }
    public Writer getOutput() throws IOException {
        doOutputCheck();
        out = new TestDocumentWriter(this);
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
        internalSetText(text);
    }

    private void internalSetText(String text) {
        this.text = text;
        test.newText(text);
    }

    void inputDone(TestDocumentReader in) {
        if ( in == this.in ) {
            this.in = null;
        }
        else {
            // Do nothing.
        }
    }
    void outputDone(TestDocumentWriter out, String text) {
        if ( out == this.out ) {
            this.out = null;
            internalSetText(text);
        }
        else {
            // Do nothing.
        }
    }

    public String getEventName(){
        return eventName;
    }
    public URL getDocumentURL(){
        return documentURL;
    }
    public void redirectDocumentOpen(String newURL){
        test.reportRedirectDocumentOpen(eventName, newURL);
    }

    private Test test;
    private String workDirectory;
    private URL workDirectoryURL;
    private URL documentURL;

    private String text;
    private URL base;
    private TestDocumentWriter out;
    private TestDocumentReader in;
    private String eventName;
}

class TestDocumentReader extends CharArrayReader {
    public TestDocumentReader(TestDocument document, char[] text){
        super(text);
        this.document = document;
    }

    public void close() {
        document.inputDone(this);
        super.close();
    }
    TestDocument document;
}

class TestDocumentWriter extends CharArrayWriter {
    public TestDocumentWriter(TestDocument document){
        this.document = document;
    }
    public void close() {
        document.outputDone(this, toString());
        super.close();
    }
    TestDocument document;
}
