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
import java.net.*;
import netscape.plugin.composer.*;
import netscape.plugin.composer.io.*;

/** Sample Plugin that adds a "Netscape Now" button to the document.
 * Shows how to add images to the document.
 */

public class AddButton extends Plugin {
    /** Test the plugin. Not required for normal operation of the plugin.
     * You can use this to run the plugin from the command line:
     * java -classpath <your-class-path> <your-plugin-name> args...
     * where args... are passed on to the Test class.
     * You can remove this code before shipping your plugin.
     */
    static public void main(String[] args) {
        Test.perform(args, new AddButton());
    }
    /** Get the human readable name of the plugin. Defaults to the name of the plugin class.
     * @return the human readable name of the plugin.
     */
    public String getName()
    {
        return "Netscape Button";
    }

    /** Get the human readable category of the plugin. Defaults to the name of the plugin class.
     * @return the human readable category of the plugin.
     */
    public String getCategory()
    {
        return "Insert";
    }

    /** Get the human readable hint for the plugin. This is a one-sentence description of
     * what the plugin does. Defaults to the name of the plugin class.
     * @return the human readable hint for the plugin.
     */
    public String getHint()
    {
        return "Inserts a 'Netscape Now' image that is linked to the Netscape home page.";
    }

    /** Perform the action of the plugin. This plugin adds a "Netscape Now" button
     * with a link to the Netscape home page.
     *
     * @param document the current document.
     */
    public boolean perform(Document document) throws IOException{
        // Create a print stream for the new document text.
        PrintWriter out = new PrintWriter(document.getOutput());
        // Create a lexical stream to tokenize the old document text.
        LexicalStream in = new LexicalStream(new SelectedHTMLReader(document.getInput(), out));
        // Keep track of whether or not we are in the selected text.
        // At the beginning of the document we're outside the selection.
        // After we encounter the start-selection comment, we're inside
        // the selection.
        // After we encounter the end-selection comment, we're outside
        // the selection again.
        boolean inSelection = false;
        Comment selectionStart = null;
        for(;;){
            // Get the next token of the document.
            Token token = in.next();
            if ( token == null ) break; //  Null means we've finished the document.
            else if (token instanceof Comment ) {
                Comment comment = (Comment) token;
                if ( comment.isSelectionStart() ){
                    selectionStart = comment;
                    inSelection = true;
                    continue; // Don't print the selection start yet.
                }
                else if (comment.isSelectionEnd() ){
                    inSelection = false;
                    out.print(selectionStart);
                    insertButton(document, out);
                }
                else { // don't output this generic comment.
                    continue;
                }
            }
            out.print(token);
        }
        out.close();
        return true;
    }

    void insertButton(Document document, PrintWriter out){
        String fileName = "netnow3.gif";
        String imgURLString = "http://home.netscape.com/comprod/products/navigator/version_3.0/images/"
            + fileName;

        writeImageToDisk(document, imgURLString, fileName);

        try {
            // Write the HTML for the button
            Tag anchor = new Tag("A");
            anchor.addAttribute("HREF", "http://www.netscape.com");
            Tag anchorEnd = new Tag("A", false);
            Tag img = new Tag("IMG");
            URL imgURL = new URL(document.getWorkDirectoryURL(), fileName);
            img.addAttribute("SRC", imgURL.toString());
            out.print(anchor);
            out.print(img);
            out.print(anchorEnd);
        } catch (Throwable t){
            t.printStackTrace();
        }
    }

    void writeImageToDisk(Document document, String imgURLString, String fileName){
        // We could just reference the image on the netscape site,
        // but the point of this example is to show how to write
        // a file to the disk.

        // In order to do this, we need to have the ability access files.
        netscape.security.PrivilegeManager.enablePrivilege("UniversalFileAccess");
        // And the ability to connect to an arbitrary URL.
        netscape.security.PrivilegeManager.enablePrivilege("UniversalConnect");

        // Copy the image to the work directory.
        File outFile = new File(document.getWorkDirectory(), fileName);
        if ( ! outFile.exists() ){
            try {
                InputStream in = new URL(imgURLString).openStream();
                FileOutputStream outStream = new FileOutputStream(outFile);
                int chunkSize = 1024;
                byte[] bytes = new byte[chunkSize];
                int readSize;
                while ( (readSize = in.read(bytes)) >= 0 ){
                    outStream.write(bytes, 0, readSize);
                }
                outStream.close();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }
}
