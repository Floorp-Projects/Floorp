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
