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
import java.net.URL;
import netscape.javascript.JSObject;

/** An abstract HTML document.
 * Contains html text. Also contains
 * a work directory. The work directory is where
 * you place images (or other embedded files) that you
 * are inserting into the document.
 *
 */
public abstract class Document {
    /** Get the base URL of the document. This is the url that the
     * relative URLs are relative to.
     * @return the base URL of the document.
     */
    public abstract URL getBase();
   /** Get the work directory of the document. You need this
     * to be able to add files to the document.
     * @return the work directory for the document.
     */
    public abstract File getWorkDirectory();

   /** Get the work directory of the document as a URL. You need this
     * to be able to add files to the document.
     * @return the work directory for the document as a URL.
     */
    public abstract URL getWorkDirectoryURL();

    /** Get an input stream for a document. Use this to read
     * information out of the document.
     * @return an input stream for the document. The caller must call the close()
     * method when finished with the stream. The caller must
     * finish with the stream before creating another input stream
     * for the same document.
     * @throw IOException if there is an existing unclosed input stream for this document.
     */
    public abstract Reader getInput() throws IOException;
    /** Get an output stream for a document. Use this to write
     * information into a document.
     * @return an output stream for the document.The caller must call the close()
     * method when finished with the stream. The caller must
     * finish with the stream before creating another output stream
     * for the same part.
     * @throw IOException if there is an existing unclosed output stream for this document.
     */
    public abstract Writer getOutput() throws IOException;

    /** Get the entire text of the document as a String. Use this to read information
     * out of the document. This is more convenient than getInput if your intent is
     * to perform string-based operations on the document's raw HTML code.
     * getInput is more convenient if you intend to tokenize the HTML.
     * @return the entire raw html text of the document, as a String.
     * @throw IOException if there is a problem reading the document.
     */
    public abstract String getText() throws IOException;

    /** Set the entire text of the document. Use this to write information
     * into the document. This is more convenient than getOutput if you have
     * performed string-based operations on the document's raw HTML text.
     * getOutput is more convenient if you process tokenized HTML.
     * @param text the new entire raw HTML text of the document.
     * @throw IOException if there is a problem writing the document, or if
     * there is an existing getOutput stream that has not been closed.
     */
    public abstract void setText(String text) throws IOException;

    /** Get the event that triggered this plug-in invocation.
     * This method returns null if this is a normal invocation.
     * <pre>
     * The standard events are:
     *     edit - called when an existing document is about to be edited.
     *     open - called when a document (new or old) is opened for editing.
     *     close - called when a document is about to be closed.
     *     publish - called before a document is published.
     *     publishComplete - called after a document is published.
     *     save - called before a document is saved.
     *     send - called before a document is sent (mailed).
     * </pre>
     * @return the event that triggered the plug-in invocation,
     * or null if this is a normal invocation.
     */
    public String getEventName(){
        return null;
    }

    /** Get the document URL (if it is known) for this document.
     * This method returns null if there is no document URL available.
     * During a publish or a save event, the URL will be the URL of
     * the destination to which the document is being published or saved.
     * @return the URL of the document, or null if
     * the document URL is unknown.
     */
    public URL getDocumentURL(){
        return null;
    }
    /** Redirect the open to a new document URL. This method only has an
     * effect when the event is "edit". Useful for situations where the
     * publishing URL is different from the editing URL. This could happen
     * when you use an FTP server to access the backing store of an HTTP server.
     * @param newURL The URL, expressed as a string, that should be used to
     * edit the document.
     */
    public void redirectDocumentOpen(String newURL){
    }

    /** Open a URL for editing. This call is asynchronous. No errors are returned.
     * @param URL The URL, expressed as a string, that is to be edited.
     */
    static public void editDocument(String url){
        try {
            Composer.editDocument(url);
        } catch (Throwable t){
            t.printStackTrace();
        }
    }

    /** get the jsobject from the document.  If no javascript in the
     *  runtime, this will return null
     */
     public JSObject getJSObject(){return null;}
}
