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
import netscape.plugin.composer.*;
import netscape.plugin.composer.io.*;

/** Sample Plugin that strips tags from the selected text in an html document.
 * Shows how to do low-level parsing of html.
 */

public class TagStrip extends Plugin {
    /** Test the plugin. Not required for normal operation of the plugin.
     * You can use this to run the plugin from the command line:
     * java -classpath <your-class-path> <your-plugin-name> args...
     * where args... are passed on to the Test class.
     * You can remove this code before shipping your plugin.
     */
    static public void main(String[] args) {
        Test.perform(args, new TagStrip());
    }
    /** Get the human readable name of the plugin. Defaults to the name of the plugin class.
     * @return the human readable name of the plugin.
     */
    public String getName()
    {
        return "Tag Stripper";
    }

    /** Get the human readable category of the plugin. Defaults to the name of the plugin class.
     * @return the human readable category of the plugin.
     */
    public String getCategory()
    {
        return "Character Tools";
    }

    /** Get the human readable hint for the plugin. This is a one-sentence description of
     * what the plugin does. Defaults to the name of the plugin class.
     * @return the human readable hint for the plugin.
     */
    public String getHint()
    {
        return "Removes all style and container information from the selected text.";
    }

    /** Perform the action of the plugin. This plugin strips the
     * tags from the selected text.
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
        String paragraphTag = "P";
        for(;;){
            // Get the next token of the document.
            Token token = in.next();
            if ( token == null ) break; //  Null means we've finished the document.
            else if (token instanceof Comment ) {
                Comment comment = (Comment) token;
                if ( comment.isSelectionStart() ){
                    inSelection = true;
                }
                else if (comment.isSelectionEnd() ){
                    inSelection = false;
                }
                else { // don't output this generic comment.
                    continue;
                }
            }
            else if ( inSelection && token instanceof Tag) {
                Tag tag = (Tag) token;
                if ( ! paragraphTag.equals(tag.getName()) ) {
                    // Don't output this tag.
                    continue;
                }
            }
            out.print(token);
        }
        out.close();
        return true;
    }
}
