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

import netscape.plugin.composer.*;
import netscape.plugin.composer.io.*;
import java.util.*;
import java.io.*;

import netscape.application.*;
import netscape.util.*;

/** Sample Plugin that uses the Internet Foundation Classes.
 * It shows how to use the IFC from within a Composer plug-in.
 * <p>
 * As of Netscape Navigator 4.0b2,
 * the file load/save menu items don't work because Navigator 4.0b2 is
 * missing the AWT File Dialog classes.
 */

public class IFCTest extends Plugin {
    static public void main(String[] args){
        Test.perform(args, new IFCTest());
    }
    /** Get the human readable name of the plugin. Defaults to the name of the plugin class.
     * @return the human readable name of the plugin.
     */
    public String getName()
    {
        return bundle().getString("name");
    }

    /** Get the human readable category of the plugin. Defaults to the name of the plugin class.
     * @return the human readable category of the plugin.
     */
    public String getCategory()
    {
        return bundle().getString("category");
    }

    /** Get the human readable hint for the plugin. This is a one-sentence description of
     * what the plugin does. Defaults to the name of the plugin class.
     * @return the human readable hint for the plugin.
     */
    public String getHint()
    {
        return bundle().getString("hint");
    }

    public boolean perform(Document document) throws IOException {
        IFCTestApp app = new IFCTestApp(document);
        app.run();
        boolean result = app.result();
        if ( result ) {
            document.setText(app.getText());
        }
        return result;
    }

    static public ResourceBundle bundle() {
        if ( bundle_ == null ){
            bundle_ = ResourceBundle.getBundle(
                "netscape.test.plugin.composer.IFCTestBundle");
        }
        return bundle_;
    }
    private static ResourceBundle bundle_;
}

/** The IFC application that implements the plug-in.
 */

class IFCTestApp extends Application implements Target,
    WindowOwner
{
    public IFCTestApp(Document document){
        this.document = document;
    }
    public boolean result() {
        return this.bresult;
    }
    /** This method gets called to initialize an application. We'll take
      * this opportunity to set up the View hierarchy.
      */
    public void init() {
        try {
        super.init();
            init2();
        }
        catch(Throwable t){
            t.printStackTrace();
        }
    }

    private void init2(){
        mainWindow = new ExternalWindow();
        mainWindow.setOwner(this);
        mainWindow.setTitle(IFCTest.bundle().getString("title"));
        setMainRootView(mainWindow.rootView());
        mainWindow.sizeTo(620, 450);
        mainWindow.setResizable(false);

        RootView v = mainWindow.rootView();
        v.setColor(Color.lightGray);

        // How about some text?

        textView_ = new TextView();
        textView_.setBounds(0,0,600-5,1000);
        copyTextFromDocument();

        ScrollGroup scrollGroup = new ScrollGroup(0,0,600,385);
        scrollGroup.scrollView().setContentView(textView_);
        scrollGroup.setHorizScrollBarDisplay(ScrollGroup.NEVER_DISPLAY);
        scrollGroup.setVertScrollBarDisplay(ScrollGroup.AS_NEEDED_DISPLAY);
        scrollGroup.setBackgroundColor(Color.white);
        v.addSubview(scrollGroup);

        int buttonY = 400;

        Button ok = Button.createPushButton(25, buttonY, 150, 20);
        ok.setTitle(IFCTest.bundle().getString("ok"));
        ok.setCommand("ok");
        ok.setTarget(this);
        v.addSubview(ok);

        Button apply = Button.createPushButton(185, buttonY, 150, 20);
        apply.setTitle(IFCTest.bundle().getString("apply"));
        apply.setCommand("apply");
        apply.setTarget(this);
        v.addSubview(apply);

        Button cancel = Button.createPushButton(345, buttonY, 150, 20);
        cancel.setTitle(IFCTest.bundle().getString("cancel"));
        cancel.setCommand("cancel");
        cancel.setTarget(this);
        v.addSubview(cancel);

        mainWindow.show();
    }

    public void performCommand(String command, Object arg) {
        try {
            if (command.equals("ok")){
                bresult = true;
                stopRunning();
            }
            else if (command.equals("apply")){
                try {
                    document.setText(getText());
                } catch(IOException e){
                    System.err.println("Error writing document:");
                    e.printStackTrace();
                }
            }
            else if (command.equals("cancel")){
                bresult = false;
                stopRunning();
            }
            else {
                System.err.println("Unknown command " + command);
            }
        }
        catch (Throwable t){
            System.err.println("Caught exception while performing command:");
            t.printStackTrace();
        }
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
        textView_.setString(text);
    }
    /** Copies text out of the dialog box.
    */
    public String getText(){
        return textView_.string();
    }


    // Boiler-plate code to allow us to run as an application

    public RootView rootView(){
        if ( mainWindow != null ){
            return mainWindow.rootView();
        }
        return super.mainRootView();
    }

    public boolean windowWillShow(Window aWindow){return true;}
    public boolean windowWillHide(Window aWindow){return true;}
    public void windowWillSizeBy(Window aWindow, Size aSize){}
    public void windowDidBecomeMain(Window aWindow){}
    public void windowDidShow(Window aWindow){}
    public void windowDidResignMain(Window aWindow){}
    public void windowDidHide(Window aWindow){
        if ( aWindow == mainWindow ){
            stopRunning();
            }
        }

    private TextView textView_;
    private ExternalWindow mainWindow;
    private boolean bresult;
    private Document document;
}

