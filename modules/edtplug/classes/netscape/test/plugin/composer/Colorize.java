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

/** Sample Plugin that colorizes the selected text in an html document.
 * Shows how to do low-level parsing of html.
 */

public class Colorize extends Plugin {
    /** Test the plugin. Not required for normal operation of the plugin.
     * You can use this to run the plugin from the command line:
     * java -classpath <your-class-path> <your-plugin-name> args...
     * where args... are passed on to the Test class.
     * You can remove this code before shipping your plugin.
     */
    static public void main(String[] args) {
        Test.perform(args, new Colorize());
    }
    /** Get the human readable name of the plugin. Defaults to the name of the plugin class.
     * @return the human readable name of the plugin.
     */
    public String getName()
    {
        return "Colorize";
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
        return "Colorizes the selected text.";
    }

    /** Perform the action of the plugin. This plugin colorizes the selected text.
     *
     * @param document the current document.
     */
    public boolean perform(Document document) throws IOException{
        // Get the output stream to hold the new document text.
        PrintWriter out = new PrintWriter(document.getOutput());
        // Create a lexical stream to tokenize the old document text.
        LexicalStream in = new LexicalStream(new SelectedHTMLReader(document.getInput(), out));
        hue = 0;
        for(;;){
            // Get the next token of the document.
            Token token = in.next();
            if ( token == null ) break; //  Null means we've finished the document.
            else if ( token instanceof Text ) {
                Text text = (Text) token;
                colorize(text.getText(), out);
                continue; // Don't need to output the original token
            }
            else if ( token instanceof Tag ) {
                Tag tag = (Tag) token;
                if ( tag.getName().equals("FONT")
                    && tag.containsAttribute("COLOR") ){
                    // Strip out the color tag
                    tag.removeAttribute("COLOR");
                }
            }
            out.print(token);
        }
        out.close();
        return true;
    }

    /** Colorize the text in a string.
    */
    private void colorize(String string, PrintWriter out){
        int len = string.length();
        Tag colorTag = new Tag("font");
        Tag colorEnd = new Tag("font", false);
        for(int i = 0; i < len; i++ ){
            char c = string.charAt(i);
            int rgb = Color.HSBtoRGB(hue, 1.0f, 1.0f);
            colorTag.addAttribute("color", formatColor(rgb));
            out.print(colorTag);
        out.print(c);
        out.print(colorEnd);
            hue = (float) ((hue + .005f) % 1.0f);
        }
    }

    private String formatColor(int rgb){
        return "#"
            + Integer.toHexString((rgb >> 20) & 0xf)
            + Integer.toHexString((rgb >> 16) & 0xf)
            + Integer.toHexString((rgb >> 12) & 0xf)
            + Integer.toHexString((rgb >> 8) & 0xf)
            + Integer.toHexString((rgb >> 4) & 0xf)
            + Integer.toHexString(rgb & 0xf);
    }
    private float hue;
}
