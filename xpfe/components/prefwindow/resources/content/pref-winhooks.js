/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; -*-
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bill Law       <law@netscape.com>
 *   Aaron Kaluszka <ask@swva.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// Turn this on to get debug output.
const debug = 1;
function debugDump( text ) {
    if ( debug ) {
        dump( text + "\n" );
    }
}
function dumpObject( obj, name ) {
    for ( prop in obj ) {
        debugDump( name + "." + prop + "=" + obj[prop] );
    }
}

// Top-level windows integration preferences.
if ( !( "winHooks" in parent ) ) {
    parent.winHooks = new Object;
    
    parent.winHooks.settings = null;
    parent.winHooks.winhooks = null;
    parent.winHooks.prefs = null;
}

// This function is called when the user presses Ok to close the prefs window.
function onOK() {
    try {
        // Update prefs.
        parent.winHooks.winhooks.settings = parent.winHooks.prefs;
    }
    catch(e) {
        dump( e + "\n" );
    }
}

var gPrefService = null;
var gPrefBranch = null;

// This function is called when our pref panel is loaded.
function Startup() {

    const prefbase = "system.windows.lock_ui.";

    // initialise preference component.
    // While the data is coming from the system registry, we use a set
    // of parallel preferences to indicate if the ui should be locked.
    if (!gPrefService) {
        gPrefService = Components.classes["@mozilla.org/preferences-service;1"];
        gPrefService = gPrefService.getService();
        gPrefService = gPrefService.QueryInterface(Components.interfaces.nsIPrefService);
        gPrefBranch = gPrefService.getBranch(prefbase);
    }
 

    // Get globals.
    var settings = "settings" in parent.winHooks ? parent.winHooks.settings : null;
    var winhooks = parent.winHooks.winhooks;
    var prefs    = parent.winHooks.prefs;
    if ( !winhooks ) {
        // Get component service.
        try {
            winhooks = Components.classes[ "@mozilla.org/winhooks;1" ].getService( Components.interfaces.nsIWindowsHooks );
            if ( winhooks ) {
                // Try to get preferences.
                prefs = winhooks.settings;
                // Set globals.
                parent.winHooks.winhooks = winhooks;
                parent.winHooks.prefs    = prefs;
            }
        }
        catch(e) {
            dump( e + "\n" );
        }
    }         
    if ( !settings ) {
        // Set state specific to this panel (not shared with the "default browser"
        // button state from the Navigator panel).
        settings = parent.winHooks.settings = [ "isHandlingHTML",
                                                "isHandlingJPEG",
                                                "isHandlingGIF",
                                                "isHandlingPNG",
                                                "isHandlingMNG",
                                                "isHandlingXBM",
                                                "isHandlingBMP",
                                                "isHandlingICO",
                                                "isHandlingXML",
                                                "isHandlingXHTML",
                                                "isHandlingXUL",
                                                "isHandlingHTTP",
                                                "isHandlingHTTPS",
                                                "isHandlingFTP",
                                                "isHandlingCHROME",
                                                "isHandlingGOPHER",
                                                "showDialog" ];
        // Register so we get called when pref window Ok is pressed.
        parent.hPrefWindow.registerOKCallbackFunc( onOK );
    }
    // Transfer object settings to the dialog checkboxes.
    for( var index in settings  ) {
        var setting = settings[ index ];
        var checkbox = document.getElementById( setting );
        if ( checkbox && prefs[ setting ] ) {
            checkbox.setAttribute( "checked", "true" );
        }

        // disable the xul element if the appropriate pref is locked.
        if (gPrefBranch && gPrefBranch.prefIsLocked(setting)) {
            checkbox.disabled = true;
        }
    }
}

function GetFields() {
    // Get globals.
    var settings = parent.winHooks.settings;
    var winhooks = parent.winHooks.winhooks;
    var prefs    = parent.winHooks.prefs;
    // Transfer data from dialog to prefs object.
    for( var index in settings ) {
        var setting = settings[ index ];
        var checkbox = document.getElementById( setting );
        if ( checkbox ) {
            prefs[ setting ] = checkbox.checked;
        }
    }
}
