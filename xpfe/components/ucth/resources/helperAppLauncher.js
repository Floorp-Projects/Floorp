/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

function nsHelperAppLauncherDialog() {
    // Initialize data properties.
    this.chosenApp  = null;
    this.chosenFile = null;

    try {
        // App launcher is passed as dialog argument.
        this.appLauncher = window.arguments[0];

        // Initialize the dialog contents.
        this.initDialog();
    } catch( e ) {
        // On error, close dialog.
        dump( "nsHelperAppLauncherDialog error: " + e + "\n" );
        window.close();
    }
}

nsHelperAppLauncherDialog.prototype= {
    // Statics.
    nsIHelperAppLauncher : Components.interfaces.nsIHelperAppLauncher,
    nsIFilePicker        : Components.interfaces.nsIFilePicker,

    // Fill dialog from app launcher attributes.
    initDialog : function () {
        document.getElementById( "alwaysAskMe" ).checked = true;
        if ( this.appLauncher.MIMEInfo.preferredApplicationHandler ) {
            // Run this app unless requested otherwise.
            document.getElementById( "runApp" ).checked = true;

            this.chosenApp = this.appLauncher.MIMEInfo.preferredApplicationHandler;

            document.getElementById( "appName" ).value = this.chosenApp.unicodePath;
        } else {
            // Save to disk.
            document.getElementById( "saveToDisk" ).checked = true;
        }

        // Set up dialog button callbacks.
        var object = this;
        doSetOKCancel( function () { return object.onOK(); },
                       function () { return object.onCancel(); } );
    },

    // If the user presses OK, we do as requested...
    onOK : function () {
        var dontAskNextTime = !document.getElementById( "alwaysAskMe" ).checked;
    
        if ( document.getElementById( "runApp" ).checked ) {
            this.appLauncher.launchWithApplication( this.chosenApp, dontAskNextTime );
        } else {
            this.appLauncher.saveToDisk( this.chosenFile, dontAskNextTime );
        }
    
        window.close();
    },

    // If the user presses cancel, tell the app launcher and close the dialog...
    onCancel : function () {
        // Cancel app launcher.
        this.appLauncher.Cancel();
    
        // Close up dialog by returning true.
        return true;
    },

    // Choose a new/different app...
    chooseApp : function () {
        var fp = Components.classes["component://mozilla/filepicker"].createInstance( this.nsIFilePicker );
        fp.init( window,
                 this.getString( "chooseAppFilePickerTitle" ),
                 this.nsIFilePicker.modeOpen );
        // XXX - We want to say nsIFilePicker.filterExecutable or something
        fp.appendFilters( this.nsIFilePicker.filterAll );
    
        if ( fp.show() == this.nsIFilePicker.returnOK && fp.file ) {
            this.chosenApp = fp.file;
            document.getElementById( "appName" ).value = this.chosenApp.unicodePath;
        }
    },

    // Choose a file to save to...
    chooseFile : function () {
        var fp = Components.classes["component://mozilla/filepicker"].createInstance( this.nsIFilePicker );
        fp.init( window,
                 this.getString( "chooseFileFilePickerTitle" ),
                 this.nsIFilePicker.modeSave );
        // XXX - Can we set this to filter on extension of file to be saved?
        fp.appendFilters( this.nsIFilePicker.filterAll );
    
        if ( fp.show() == this.nsIFilePicker.returnOK && fp.file ) {
            this.chosenFile = fp.file;
            document.getElementById( "fileName" ).value = this.chosenFile.unicodePath;
        }
    },

    // Get string from bundle.
    getString : function ( id ) {
        // String of last resort is the id.
        var result = id;
    
        // Get string bundle (if not done previously).
        if ( !this.strings ) {
            this.strings = srGetStrBundle("chrome://global/locale/helperAppLauncher.properties");
        }
    
        if ( this.strings ) {
            // Get string from bundle.
            result = this.strings.GetStringFromName( id );
        }
    
        return result;
    },
}
