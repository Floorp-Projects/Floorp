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
