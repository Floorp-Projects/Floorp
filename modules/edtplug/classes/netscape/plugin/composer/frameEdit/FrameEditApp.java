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

package netscape.plugin.composer.frameEdit;

import netscape.plugin.composer.*;
import netscape.plugin.composer.io.*;
import java.util.*;
import java.io.*;
import java.net.URL;

import netscape.application.*;
import netscape.util.*;
import netscape.constructor.*;

/** The IFC application that implements the FrameEdit Composer Plug-in.
 */

class FrameEditApp extends Application implements Target,
    WindowOwner
{
    public FrameEditApp(Document document){
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

    private void init2() throws IOException {
        setupModel();
        Plan plan = getPlan("frameDialogs");
        mainWindow = plan.externalWindowWithContents();
        mainWindow.setOwner(this);
        setMainRootView(mainWindow.rootView());

        // Set up the event target

        FramePropertyTarget propertyTarget = new FramePropertyTarget(model, plan);
        TargetChain.applicationChain().addTarget(propertyTarget, true);
        model.addObserver(propertyTarget);
        // Document window

        InternalWindow docWindow = (InternalWindow) plan.componentNamed("Frameset");
        docWindow.contentView().setLayoutManager(new LayoutSameSizeAsParent());
        ContainerView standin = (ContainerView) plan.componentNamed("fsi");
        int w = standin.width();
        int h = standin.height();
        standin.removeFromSuperview();
        FrameEditView frameEditView = new FrameEditView(model);
        docWindow.addSubview(frameEditView);
        frameEditView.sizeTo(w,h);
        docWindow.layoutView(0,0);

        Menu menu = new Menu(true);
        MenuItem fileMenuItem = menu.addItemWithSubmenu(
            getString("file"));
        // fileMenuItem.submenu().addItem(getString("apply"), "apply", this);
        fileMenuItem.submenu().addItem(getString("cancel"), "cancel", this);
        fileMenuItem.submenu().addItem("-", "-", this);
        fileMenuItem.submenu().addItem(getString("save"), "save", this);
        MenuItem editMenuItem = menu.addItemWithSubmenu(getString("edit"));
        editMenuItem.submenu().addItem(getString("undo"), "Undo", this);
        editMenuItem.submenu().addItem("-", "-", this);
        editMenuItem.submenu().addItem(getString("delete"), "delete", this);
        editMenuItem.submenu().addItem("-", "-", this);
        editMenuItem.submenu().addItem(getString("split |"), "split |", this);
        editMenuItem.submenu().addItem(getString("split -"), "split -", this);

        MenuItem navigate = menu.addItemWithSubmenu(getString("navigate"));
        navigate.submenu().addItem(getString("navRoot"), "navRoot", this);
        navigate.submenu().addItem(getString("navParent"), "navParent", this);
        navigate.submenu().addItem(getString("navPrev"), "navPrevious", this);
        navigate.submenu().addItem(getString("navNext"), "navNext", this);
        navigate.submenu().addItem(getString("navChild"), "navChild", this);

        MenuItem help = menu.addItemWithSubmenu(getString("helpMenu"));
        help.submenu().addItem(getString("help"), "help", this);

        mainWindow.setMenu(menu);

        mainWindow.show();

    }

    private String getString(String key){
        return FrameEdit.getString(key);
    }

    /* Search for a plan file, using the JDK 1.1 local search path.
     * The exact local is searched for first, and then
     * less specific locals are searched.
     * For a given local, ascii plan files have precedence over
     * binary plan files.
     */

    private Plan getPlan(String planName) throws IOException {
        netscape.util.Enumeration suffixes = getLocalSearchSuffixes();
        InputStream inputStream = null;
        int planType = Plan.ASCII_TYPE;
        searchForPlan:
        while(suffixes.hasMoreElements()){
            String suffix = (String) suffixes.nextElement();
            for(int i = 0; i < 2; i++){
                planType = (i == 0 ) ? Plan.ASCII_TYPE : Plan.BINARY_TYPE;
                String extension = planType == Plan.BINARY_TYPE ?
                    Plan.BINARY_FILE_EXTENSION :
                    Plan.ASCII_FILE_EXTENSION;
                String fullName = planName + suffix + "." + extension;
                // System.err.println("Looking for plan resource " + fullName);

                inputStream = getClass().getResourceAsStream(fullName);
                if ( inputStream != null ) break searchForPlan;
            }
        }
        if ( inputStream != null ) {
            try {
                Plan plan = new Plan(inputStream, planType);
                plan.unarchiveObjects();
                return plan;
            } catch ( IOException e){
                System.err.println("Had trouble reading " + inputStream);
                e.printStackTrace();
            }
        }
        throw new IOException("Could not find plan " + planName);
    }

    /* Utility method to get a resource stream by name.
     */
    private InputStream getResourceStream(String streamName, String extension) throws IOException {
        netscape.util.Enumeration suffixes = getLocalSearchSuffixes();
        InputStream inputStream = null;
        while(suffixes.hasMoreElements()){
            String suffix = (String) suffixes.nextElement();
            String fullName = streamName + suffix + extension;
                // System.err.println("Looking for resource " + fullName);
                inputStream = getClass().getResourceAsStream(fullName);
                if ( inputStream != null ){
                    return inputStream;
            }
        }
        throw new IOException("Could not find resource " + streamName + extension);
    }

    /* Utility method to get a resource URL by name.
     */
    private URL getResource(String streamName, String extension) throws IOException {
        netscape.util.Enumeration suffixes = getLocalSearchSuffixes();
        URL url = null;
        while(suffixes.hasMoreElements()){
            String suffix = (String) suffixes.nextElement();
            String fullName = streamName + suffix + extension;
                // System.err.println("Looking for resource " + fullName);
                url = getClass().getResource(fullName);
                if ( url != null ){
                    return url;
            }
        }
        throw new IOException("Could not find resource " + streamName + extension);
    }

    private netscape.util.Enumeration getLocalSearchSuffixes(){
        netscape.util.Vector suffixes = new netscape.util.Vector();
        Locale locale = Locale.getDefault();
        // Search order
        // "_" + language1 + "_" + country1 + "_" + variant1
        // "_" + language1 + "_" + country1
        // "_" + language1
        // ""
        String language = locale.getLanguage().toUpperCase();
        String country = locale.getCountry();
        String variant = locale.getVariant();
        if ( variant != null && variant != "") {
            suffixes.addElement("_" + language + "_" + country + "_" + variant);
        }
        suffixes.addElement("_" + language + "_" + country);
        suffixes.addElement("_" + language);
        suffixes.addElement("");

        return suffixes.elements();
    }

    private void setupModel(){
        model = new FrameModel();
        try {
            Reader in = document.getInput();
            model.read(in);
            in.close();
        } catch (IOException e){
            e.printStackTrace();
        }
    }

    private void writeModel(){
        try {
            Writer out = document.getOutput();
            model.write(out);
            out.close();
        } catch (IOException e){
            e.printStackTrace();
        }
    }

    public void performCommand(String command, Object arg) {
        try {
            if (command.equals("About")){
                return;
            }
            else if (command.equals("apply")){
                writeModel();
            }
            else if (command.equals("cancel")){
                bresult = false;
                stopRunning();
            }
            else if (command.equals("save")){
                writeModel();
                bresult = true;
                stopRunning();
            }
            else if (command.equals("delete")){
                model.selection().delete();
            }
            else if (command.equals("split |")){
                split(true);
            }
            else if (command.equals("split -")){
                split(false);
            }
            else if (command.equals("Undo")){
                undo();
            }
            else if (command.equals("navRoot")){
                model.selection().selectRoot();
            }
            else if (command.equals("navParent")){
                model.selection().selectParent();
            }
            else if (command.equals("navPrevious")){
                model.selection().selectPreviousSibling();
            }
            else if (command.equals("navNext")){
                model.selection().selectNextSibling();
            }
            else if (command.equals("navChild")){
                model.selection().selectFirstChild();
            }
            else if (command.equals("help")){
                help();
            }
            else {
                System.err.println("Unknown command " + command);
            }
        }
        catch (Throwable t){
            System.err.println("Caught exception while performing command "
                + command +":");
            t.printStackTrace();
        }
    }

    public void undo(){
        model.undo();
    }

    public void split(boolean horizontal){
        model.selection().split(horizontal, 0.5);
    }

    private void help(){
        if ( help_ == null ) {
            help_ = new ExternalWindow();
            help_.setOwner(this);
            help_.setTitle(getString("help title"));
            help_.sizeTo(600, 400);
            help_.moveTo(20, 20);

            RootView v = help_.rootView();

            ScrollGroup group = new ScrollGroup(5, 5, v.width() - 10 , v.height() - 10);
            group.setHorizResizeInstruction(View.WIDTH_CAN_CHANGE);
            group.setVertResizeInstruction(View.HEIGHT_CAN_CHANGE);
            group.setHorizScrollBarDisplay(ScrollGroup.NEVER_DISPLAY);
            group.setVertScrollBarDisplay(ScrollGroup.AS_NEEDED_DISPLAY);
            group.setBackgroundColor(Color.white);

            TextView text = new TextView(0, 0, group.width() - 5, 100); // height doesn't matter
            text.setHorizResizeInstruction(View.WIDTH_CAN_CHANGE);
            text.setVertResizeInstruction(View.HEIGHT_CAN_CHANGE);
            text.setEditable(false);
            text.setSelectable(false);
            try {
                URL url = getResource("help", ".html");
                System.err.println("Help file URL: " + url.toString());
                text.importHTMLFromURLString(url.toString());
            } catch ( IOException e) {
                // e.printStackTrace();
                // Communicator 4.0PR3 can't find resources in JAR files by URL.
                // This may be a problem with JDK 1.1 and JAR files.
                // This alternative syntax works, but URLs in the
                // html won't be resolved, so pictures won't show up.
                try {
                    InputStream helpStream = getResourceStream("help", ".html");
                    text.importHTMLInRange(helpStream, new Range(0,0), null);
                } catch ( IOException e2) {
                    e2.printStackTrace();
                } catch ( HTMLParsingException e2) {
                    e2.printStackTrace();
                }
            }
            group.scrollView().setContentView(text);

            group.scrollView().setBuffered(true);
            v.addSubview(group);
            v.setColor(Color.lightGray);
        }
        help_.show();
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
            writeModel();
            stopRunning();
            }
        }
    private ExternalWindow mainWindow;
    private ExternalWindow help_;
    private boolean bresult = true;
    private Document document;
    private FrameModel model;
}

