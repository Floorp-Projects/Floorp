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
const settings = [ "isHandlingHTML",
                   "isHandlingJPEG",
                   "isHandlingGIF",
                   "isHandlingPNG",
                   "isHandlingXML",
                   "isHandlingXUL",
                   "isHandlingHTTP",
                   "isHandlingHTTPS",
                   "isHandlingFTP",
                   "isHandlingCHROME" ];

var winhooks = null;
var prefs = null;

// This function is called when the user presses Ok to close the prefs window.
function onOK() {
    // Transfer data from dialog to prefs object.
    for( var index in settings ) {
        var setting = settings[ index ];
        var checkbox = document.getElementById( setting );
        if ( checkbox ) {
            prefs[ setting ] = checkbox.checked;
        }
    }
    try {
        // Update prefs.
        winhooks.settings = prefs;
    }
    catch(e) {
        alert( "Oh oh!" );
    }
}

// This function is called when our pref panel is loaded.
function Startup() {
    if ( !winhooks ) {
        // Get component service.
        try {
            winhooks = Components.classes[ "@mozilla.org/winhooks;1" ].getService( Components.interfaces.nsIWindowsHooks );
            if ( winhooks ) {
                // Try to get preferences.
                prefs = winhooks.settings;
                // Now transfer those to the dialog checkboxes.
                for( var index in settings  ) {
                    var setting = settings[ index ];
                    var checkbox = document.getElementById( setting );
                    if ( checkbox && prefs[ setting ] ) {
                        checkbox.setAttribute( "checked", "true" );
                    }
                }
            }
        }
        catch(e) {
            dump( e + "\n" );
            alert( "Rats!" );
        }
    }         
    // If all is well, hook into "Ok".
    if ( winhooks && prefs && parent && parent.hPrefWindow ) {
        parent.hPrefWindow.registerOKCallbackFunc( onOK );
    }
}

// Launch specified dialog.
function showDialog( index ) {
    var url = "chrome://pref/content/pref-winhooks-" + index + ".xul";
    window.openDialog( url, "", "chrome", prefs );
}
