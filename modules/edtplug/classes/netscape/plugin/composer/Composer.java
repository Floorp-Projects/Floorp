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

package netscape.plugin.composer;

import java.util.*;

/** The Java representation of the Composer
 */
class Composer {
    public static final int PLUGIN_FAIL = 0;
    public static final int PLUGIN_CANCEL = 1;
    public static final int PLUGIN_OK = 2;
    public static final int PLUGIN_NEWTEXT = 3;
    public static final int PLUGIN_EDITURL = 4; /* Must match case 4 in native_netscape_plugin_composer_Composer_mtCallback */
    public Composer(int composerID, int callbackFunc, int mozenv){
        this.composerID = composerID;
        this.callbackFunc = callbackFunc;
        this.mozenv = mozenv;
    }

    /** Call from any thread.
    */
    public void pluginFinished(int status, Object arg){
        new ComposerCallback(this, status, arg).enqueue();
    }

    /** Call from any thread.
    */
    public void newText(String arg){
        new ComposerCallback(this, PLUGIN_NEWTEXT, arg).enqueue();
    }

    public static void editDocument(String url){
        Composer dummy = new Composer(0, 0, 0);
        new ComposerCallback(dummy, PLUGIN_EDITURL, url).enqueue();
    }

    /** Only for ComposerCallback to call back in Mozilla thread.
     */
    public void callback(int action, Object arg){
        mtCallback(composerID, callbackFunc, action, arg);
    }
    private native void mtCallback(int context, int callbackFunc, int action, Object arg);
    public final int getMozenv() { return mozenv; }
    private int composerID;
    private int mozenv;
    private int callbackFunc;
}

class ComposerCallback extends MozillaCallback {
    public ComposerCallback(Composer composer, int action, Object argument){
        super(composer.getMozenv());
        this.composer = composer;
        this.action = action;
        this.argument = argument;
    }
    protected void perform() {
        composer.callback(action, argument);
    }
    private Composer composer;
    private int action;
    private Object argument;
}
