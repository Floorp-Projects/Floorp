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
import java.awt.Color;

/** Sample Plugin that implements Netscape-style small caps.
 * Shows how to do low-level parsing of html.
 */

public class SmallCaps extends Plugin {
    /** Test the plugin. Not required for normal operation of the plugin.
     * You can use this to run the plugin from the command line:
     * java -classpath <your-class-path> <your-plugin-name> args...
     * where args... are passed on to the Test class.
     * You can remove this code before shipping your plugin.
     */
    static public void main(String[] args) {
        Test.perform(args, new SmallCaps());
    }
    /** Get the human readable name of the plugin. Defaults to the name of the plugin class.
     * @return the human readable name of the plugin.
     */
    public String getName()
    {
        return "Small Caps";
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
        return "Capitalizes selected text like the Netscape Home Page.";
    }

    /** Perform the action of the plugin. This plugin implements Netscape-style small caps.
     *
     * @param document the current document.
     */
    public boolean perform(Document document) throws IOException{
        // Get the output stream to hold the new document text.
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
            }
            else if ( inSelection ) {
                if ( token instanceof Text ) {
                    Text text = (Text) token;
                    smallCaps(text.getText(), out);
                    continue; // Don't need to output the original token
                }
            }
            out.print(token);
        }
        out.close();
        return true;
    }

    /** Convert a string to small caps.
    */
    private void smallCaps(String string, PrintWriter out){
        int len = string.length();
        int state = 1;
        Tag bigTag = new Tag("font");
        bigTag.addAttribute("SIZE", "+2");
        Tag fontEnd = new Tag("font", false);
        for(int i = 0; i < len; i++ ){
            char c = string.charAt(i);
            c = Character.toUpperCase(c);
            switch ( state ) {
            case 0: out.print(c);
                 if ( c == ' ' ) {
                     state = 1;
                 }
                 break;
            case 1: if ( c != ' ' ) {
                     out.print(bigTag);
                     out.print(c);
                     out.print(fontEnd);
                     state = 0;
                }
                else {
                    out.print(c);
                }
                break;
            }
        }
    }
}
