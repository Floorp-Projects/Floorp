/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; -*-
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
 *
 * Contributor(s):
 *  Bill Law    <law@netscape.com>
 */

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
    parent.winHooks.settings = [ "isHandlingHTML",
                                 "isHandlingJPEG",
                                 "isHandlingGIF",
                                 "isHandlingPNG",
                                 "isHandlingXML",
                                 "isHandlingXUL",
                                 "isHandlingHTTP",
                                 "isHandlingHTTPS",
                                 "isHandlingFTP",
                                 "isHandlingCHROME" ];
    
    parent.winHooks.winhooks = null;
    parent.winHooks.prefs = null;
}

// This function is called when the user presses Ok to close the prefs window.
function onOK() {
    try {
        // Get updates from dialog if we're displayed.
        if ( "GetFields" in window ) {
            GetFields();
        }
        // Update prefs.
        parent.winHooks.winhooks.settings = parent.winHooks.prefs;
    }
    catch(e) {
        dump( e + "\n" );
    }
}

// This function is called when our pref panel is loaded.
function Startup() {
    // Get globals.
    var settings = parent.winHooks.settings;
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
                // Register so we get called when pref window Ok is pressed.
                parent.hPrefWindow.registerOKCallbackFunc( onOK );
            }
        }
        catch(e) {
            dump( e + "\n" );
        }
    }         
    // Transfer object settings to the dialog checkboxes.
    for( var index in settings  ) {
        var setting = settings[ index ];
        var checkbox = document.getElementById( setting );
        if ( checkbox && prefs[ setting ] ) {
            checkbox.setAttribute( "checked", "true" );
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
