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
import netscape.plugin.composer.*;

/** Allow the user to view and edit the raw html of the document.
 * Shows how to call awt (and by extension any user interface
 * toolkit) from a plugin.
 */

public class EditRaw extends Plugin {
    /** Test the plugin. Not required for normal operation of the plugin.
     * You can use this to run the plugin from the command line:
     * java -classpath <your-class-path> <your-plugin-name> args...
     * where args... are passed on to the Test class.
     * You can remove this code before shipping your plugin.
     */
    static public void main(String[] args) {
        Test.perform(args, new EditRaw());
    }

    /** Get the human readable name of the plugin. Defaults to the name of the plugin class.
     * @return the human readable name of the plugin.
     */
    public String getName()
    {
        return "Edit HTML";
    }

    /** Get the human readable category of the plugin. Defaults to the name of the plugin class.
     * @return the human readable category of the plugin.
     */
    public String getCategory()
    {
        return "HTML Tools";
    }

    /** Get the human readable hint for the plugin. This is a one-sentence description of
     * what the plugin does. Defaults to the name of the plugin class.
     * @return the human readable hint for the plugin.
     */
    public String getHint()
    {
        return "Allows editing of the raw HTML of the document.";
    }

    /** Execute the command.
     * @param document the current document state.
     */
    public boolean perform(Document document) throws IOException{

        MyDialog dialog = new MyDialog("Edit Raw HTML", document);
        dialog.reshape(50,50,300,300);
        dialog.show(); // make the window visible.
        dialog.requestFocus(); // Make sure the window is on top and gets focus.
        boolean result = dialog.waitForExit(); //Wait for the user to exit the dialog.
        dialog.dispose(); // Cleans up the native OS window associated with the dialog.
        if ( result ) {
            document.setText(dialog.getText());
        }
        return result;
    }

}

/** An awt dialog for interacting with the user. This is like
 * the java.awt.Dialog class, except that it doesn't require a
 * parent Frame.
 */
class MyDialog extends Frame {
    public MyDialog(String title, Document document) {
        super(title);
        this.document = document;
        Panel buttons = new Panel();
        buttons.add("East", ok = new Button("OK"));
        buttons.add("Center", apply = new Button("Apply"));
        buttons.add("West", cancel = new Button("Cancel"));
        add("Center", text = new TextArea());
        add("South", buttons);

        copyTextFromDocument();
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
    /** Handle the actions of the dialog.
     */
    public boolean action(Event evt, Object what){
        if ( evt.target == ok || evt.target == cancel) {
            success = evt.target == ok;
            hide();
            signalExit();
            return true;
        }
        else if ( evt.target == apply ) {
            try {
                document.setText(getText());
            } catch(IOException e){
                System.err.println("Error writing document:");
                e.printStackTrace();
            }
            return true;
        }
        return false;
    }
    /** Copies the text from the document to
     * the dialog box.
     */
    protected void copyTextFromDocument() {
        try {
            setText(document.getText());
        } catch(IOException e){
            System.err.println("Error reading document:");
            e.printStackTrace();
        }
    }
    /** Puts text into the dialog box.
     */
    public void setText(String text){
        this.text.setText(text);
    }
    /** Copies text out of the dialog box.
    */
    public String getText(){
        return this.text.getText();
    }
    /** Called by the main plug-in thread. This method waits for
     * the dialog thread to call signalExit(), and then it returns.
     */
    synchronized public boolean waitForExit() {
        while ( ! bExited ) {
            try {
                wait();
            } catch ( InterruptedException e){
            }
        }
        return success;
    }
    /** Called from the dialog thread to signal to the plug-in thread
     * that the dialog is finished.
     */
    synchronized public void signalExit() {
        bExited = true;
        notifyAll();
    }
    private Button ok;
    private Button apply;
    private Button cancel;
    private TextArea text;
    private boolean bExited = false;
    private boolean success = false;
    private Document document;
}
