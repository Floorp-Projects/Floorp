/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package netscape.plugin.composer.frameEdit;

import netscape.plugin.composer.*;
import netscape.plugin.composer.io.*;
import java.util.*;
import java.io.*;

import netscape.application.*;
import netscape.util.*;

/** Sample Composer Plugin that edits frames.
 */

public class FrameEdit extends Plugin {
    static public void main(String[] args){
        netscape.test.plugin.composer.Test.perform(args, new FrameEdit());
        System.exit(0); // Stop AWT threads
    }
    /** Get the human readable name of the plugin. Defaults to the name of the plugin class.
     * @return the human readable name of the plugin.
     */
    public String getName()
    {
        return getString("name");
    }

    /** Get the human readable category of the plugin. Defaults to the name of the plugin class.
     * @return the human readable category of the plugin.
     */
    public String getCategory()
    {
        return getString("category");
    }

    /** Get the human readable hint for the plugin. This is a one-sentence description of
     * what the plugin does. Defaults to the name of the plugin class.
     * @return the human readable hint for the plugin.
     */
    public String getHint()
    {
        return getString("hint");
    }

    public boolean perform(Document document){
        FrameEditApp app = new FrameEditApp(document);
        app.run();
        return app.result();
    }

    public static ResourceBundle bundle() {
        if ( gbundle == null ) {
            try {
                gbundle = ResourceBundle.getBundle("netscape.plugin.composer.frameEdit.FrameEditorResourceBundle");
            } catch ( Throwable t){
                System.err.println("Could not get localized resources:");
                t.printStackTrace();
                System.err.println("Using English Language Default.");
            }
            gbundle = new FrameEditorResourceBundle();
        }
        return gbundle;
    }

    public static String getString(String key){
        try {
            return bundle().getString(key);
        } catch ( Throwable t){
            System.err.println("Could not get resource " + key + ":");
            t.printStackTrace();
            return key + "(!)";
        }
    }

    public static String fontName(){
        if ( gFontName == null ){
            gFontName = getString("font name");
        }
        return gFontName;
    }
    private static ResourceBundle gbundle;
    private static String gFontName;
}

