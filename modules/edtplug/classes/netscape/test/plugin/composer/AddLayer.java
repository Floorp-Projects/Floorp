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
import netscape.plugin.composer.*;
import netscape.plugin.composer.io.*;

/** Sample Plugin that edits layers.
 */

public class AddLayer extends Plugin {
    /** Test the plugin. Not required for normal operation of the plugin.
     * You can use this to run the plugin from the command line:
     * java -classpath <your-class-path> <your-plugin-name> args...
     * where args... are passed on to the Test class.
     * You can remove this code before shipping your plugin.
     */
    static public void main(String[] args) {
        Test.perform(args, new AddLayer());
    }
    /** Get the human readable name of the plugin. Defaults to the name of the plugin class.
     * @return the human readable name of the plugin.
     */
    public String getName()
    {
        return "Create Layer...";
    }

    /** Get the human readable category of the plugin. Defaults to the name of the plugin class.
     * @return the human readable category of the plugin.
     */
    public String getCategory()
    {
        return "Layers";
    }

    /** Get the human readable hint for the plugin. This is a one-sentence description of
     * what the plugin does. Defaults to the name of the plugin class.
     * @return the human readable hint for the plugin.
     */
    public String getHint()
    {
        return "Turns the selection into a layer.";
    }

    /** Perform the action of the plugin.
     *
     * @param document the current document.
     */
    public boolean perform(Document document) throws IOException{
        // Interact with the user to get the layer tag/

        Tag layerTag = getLayerTagFromUser(document);
        if ( layerTag == null ) return false; // User cancled.
        Tag endTag = new Tag(layerTag.getName(), false);
        // Get the output stream to hold the new document text.
        PrintWriter out = new PrintWriter(document.getOutput());
        // Create a lexical stream to tokenize the old document text.
        LexicalStream in = new LexicalStream(new SelectedHTMLReader(document.getInput(), out));
        for(;;){
            // Get the next token of the document.
            Token token = in.next();
            if ( token == null ) break; //  Null means we've finished the document.
            else if ( token instanceof Comment ) {
                Comment comment = (Comment) token;
                if ( comment.isSelectionStart() ) {
                    out.print(layerTag);
                }
                else if ( comment.isSelectionEnd() ) {
                    out.print(comment);
                    out.print(endTag);
                    continue; // So comment isn't printed twice.
                }
            }
            out.print(token);
        }
        out.close();
        return true;
    }

    private Tag getLayerTagFromUser(Document document){
        AddLayerDialog dialog = new AddLayerDialog("Add Layer", document);
        dialog.reshape(50,50,300,300);
        dialog.show();
        dialog.requestFocus();

        /* Wait for the user to exit the dialog. */
        boolean result = dialog.waitForExit();
        dialog.dispose(); // Cleans up the native OS window associated with the dialog.
        if ( result ) {
            Tag layer = new Tag("LAYER");
            return layer;
        }
        else return null;
    }
}

/** An awt dialog for interacting with the user. This is like
 * the java.awt.Dialog class, except that it doesn't require a
 * parent Frame.
 */
class AddLayerDialog extends Frame {
    public AddLayerDialog(String title, Document document) {
        super(title);
        this.document = document;
        Panel buttons = new Panel();
        buttons.add("East", ok = new Button("OK"));
        buttons.add("West", cancel = new Button("Cancel"));
        // add("Center", text = new TextArea());
        add("South", buttons);
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
    public boolean action(Event evt, Object what){
        if ( evt.target == ok || evt.target == cancel) {
            success = evt.target == ok;
            hide();
            signalExit();
            return true;
        }
        return false;
    }
    synchronized public boolean waitForExit() {
        while ( ! bExited ) {
            try {
                wait();
            } catch ( InterruptedException e){
            }
        }
        return success;
    }
    synchronized public void signalExit() {
        bExited = true;
        notifyAll();
    }
    private Button ok;
    private Button cancel;
    private boolean bExited = false;
    private boolean success = false;
    private Document document;
}
