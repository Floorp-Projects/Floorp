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
import java.util.*;
import java.awt.*;
import java.net.*;
import netscape.plugin.composer.*;
import netscape.plugin.composer.io.*;

/** Calculate some simple statistics about the document.
 * Shows how to call awt (and by extension any user interface
 * toolkit) from a plugin.
 */

public class DocInfo extends Plugin {
    /** Test the plugin. Not required for normal operation of the plugin.
     * You can use this to run the plugin from the command line:
     * java -classpath <your-class-path> <your-plugin-name> args...
     * where args... are passed on to the Test class.
     * You can remove this code before shipping your plugin.
     */
    static public void main(String[] args) {
        Test.perform(args, new DocInfo());
    }
    /** Execute the command.
     * @param document the current document state.
     */
    public boolean perform(Document document) throws IOException{
        DocInfoDialog dialog = new DocInfoDialog(getResource("title"));
        dialog.setText(getStats(document));
        dialog.reshape(50,50,500,300);
        dialog.show(); // make the window visible.
        dialog.requestFocus(); // Make sure the window is on top and gets focus.
        dialog.waitForExit(); //Wait for the user to exit the dialog.
        dialog.dispose(); // Cleans up the native OS window associated with the dialog.
        return true;
    }
    /** Get the human readable name of the plugin. Defaults to the name of the plugin class.
     * @return the human readable name of the plugin.
     */
    public String getName()
    {
        return getResource("name");
    }

    /** Get the human readable category of the plugin. Defaults to the name of the plugin class.
     * @return the human readable category of the plugin.
     */
    public String getCategory()
    {
        return getResource("category");
    }

    /** Get the human readable hint for the plugin. This is a one-sentence description of
     * what the plugin does. Defaults to the name of the plugin class.
     * @return the human readable hint for the plugin.
     */
    public String getHint()
    {
        return getResource("hint");
    }

     /** Get the document stats.
     */
    String getStats(Document document) throws IOException{
        StringBuffer b = new StringBuffer();
        try {
            DocStats stats = new DocStats(document);
            b.append(formatProp("chars", stats.textBytes));
            b.append(formatProp("words", stats.wordCount));
            b.append("\n");
            b.append(formatProp("images", stats.imageCount));
            b.append(formatProp("totalImageBytes", stats.imageBytes));
            int totalBytes = stats.textBytes + stats.imageBytes;
            b.append(formatProp("totalBytes", totalBytes));
            b.append(formatProp("min14400", (totalBytes / 1440 + 1)));
            b.append("\n\n\n");
            b.append(formatProp("base", document.getBase().toString()));
            b.append(formatProp("workDir", document.getWorkDirectory().toString()));
            b.append(formatProp("workDirURL", document.getWorkDirectoryURL().toString()));
        } catch (Throwable t) {
        }
        // If this is an "edit" event, we'll fail when we try to actually get
        // the document statistics. But the following should work anyway.
        try {
            String eventName = document.getEventName();
            b.append(formatProp("eventName", eventName != null ? eventName : "null"));
            URL docURL = document.getDocumentURL();
            b.append(formatProp("documentURL", docURL != null ? docURL.toString() : "null"));
        } catch (Throwable t) {
        }
        return b.toString();
    }
    /** Internationalizable format function.
     */
    String formatProp(String key, int num){
        return formatProp(key, Integer.toString(num));
    }
    String formatProp(String key, String string){
        return format(getResource(key), string);
    }

    static String getResource(String key){
        if ( resources == null ) {
            resources = ResourceBundle.getBundle(
                "netscape.test.plugin.composer.DocInfoResources");
        }
        String result = resources.getString(key);
        if ( result == null ) {
            result = key;
        }

        return result;
    }
    String format(String formatString, String arg){
        String args[] = new String[1];
        args[0] = arg;
        return format(formatString, args);
    }

    /** Poor man's internationalizable formatting.
     * @param formatString The format string. Occurences of ^0 are replaced by
     * args[0], ^1 by args[1], and so on. Use ^^ to signify a singe ^.
     * @param args The argument array.
     * @return the formatted string.
     */
    String format(String formatString, String[] args){
        int len = formatString.length();
        StringBuffer b = new StringBuffer(len);
        for(int index = 0; index < len; index++ ){
            char c = formatString.charAt(index);
            if ( c == '^') {
                if ( index + 1 < len ) {
                    c = formatString.charAt(++index);
                    if ( c >= '0' && c <= '9') {
                        int argIndex = c - '0';
                        if ( argIndex < args.length) {
                            b.append(args[argIndex]);
                            continue;
                        }
                        else {
                            b.append('^');
                            // And fall thru to append the character.
                        }
                    }
                }
            }
            b.append(c);
        }
        return b.toString();
    }

    private static ResourceBundle resources;
}

/** Calculate some simple statistics for a document
 */

class DocStats{
    public DocStats(Document document) throws IOException{
        LexicalStream s = new LexicalStream(document.getInput());
        boolean inBody = false;
        String BODY = "BODY";
        String IMG = "IMG";
        String SRC = "SRC";
        for(;;){
            // Get the next token of the document.
            Token token = s.next();
            if ( token == null ) break; //  Null means we've finished the document.
            // Add to the total byte count.
            // This is not an accurate measurement
            // because many unicode characters
            // will ultimately be encoded as more than
            // one byte.
            textBytes += token.toString().length();
            if (token instanceof Tag ) {
                Tag tag = (Tag) token;
                boolean isOpen = tag.isOpen();
                String tagName = tag.getName();
                if ( tagName.equals(BODY) ) {
                    inBody = isOpen;
                }
                else if ( isOpen ) {
                    // Check for images
                    if ( tagName.equals(IMG) ){
                        imageCount++;
                        String src = tag.lookupAttribute(SRC);
                        if ( src != null ) {
                            try {
                                URL base = document.getBase();
                                URL srcURL = new URL(base, src);
                                imageBytes += getSize(srcURL);
                            } catch(Throwable t){
                                System.err.println("Couldn't open " + src);
                                t.printStackTrace();
                            }
                        }
                    }
                }
            }
            else if ( inBody ) {
                if ( token instanceof Text ) {
                    Text text = (Text) token;
                    if ( ! text.isNewline() ){
                        countWords(text.getText());
                    }
                }
            }
        }
    }

    protected int getSize(URL srcURL){
        System.err.println("URL: " + srcURL);
        int size = 0;
        try {
            URLConnection connection = srcURL.openConnection();
            size = connection.getContentLength();
            if ( size < 0 ) {
                size = 0;
                byte[] dummy = new byte[1000];
                InputStream in = connection.getInputStream();
                int skipped;
                while((skipped = (int) in.read(dummy)) > 0) {
                    size += skipped;
                }
                in.close();
            }
        } catch(Throwable t){
            System.err.println("Couldn't open URL " + srcURL);
            t.printStackTrace();
        }
        return size;
    }
    protected void countWords(String text){
        // This is an approximate measure because
        // some tags don't cause word breaks.
        char c;
        int len = text.length();
        boolean nonSpaceFound = false;
        for(int i = 0; i < len; i++ ){
            c = text.charAt(i);
            if ( Character.isWhitespace(c)){
                wordCount++;
            }
            else {
                if ( ! nonSpaceFound ) {
                    nonSpaceFound = true;
                    wordCount++;
                }
            }
        }
    }

    public int textBytes;
    public int wordCount;
    public int imageCount;
    public int imageBytes;
}

/** An awt dialog for interacting with the user. This is like
 * the java.awt.Dialog class, except that it doesn't require a
 * parent Frame.
 */
class DocInfoDialog extends Frame {
    public DocInfoDialog(String title) {
        super(title);
        add("Center", text = new TextArea());
     }
    /** Handle window close event.
    */
    public boolean handleEvent(Event event) {
        if (event.id == Event.WINDOW_DESTROY) {
            hide();
            signalExit();
            return true;
        } else {
            return super.handleEvent(event);
        }
    }
    public void setText(String text){
        this.text.setText(text);
    }
    synchronized public void waitForExit() {
        while ( ! bExited ) {
            try {
                wait();
            } catch ( InterruptedException e){
            }
        }
    }
    synchronized public void signalExit() {
        bExited = true;
        notifyAll();
    }
    private Button quit;
    private TextArea text;
    private boolean bExited = false;
}
