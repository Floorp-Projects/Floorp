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
