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
    this.userChoseApp = false;
    this.chosenApp    = null;

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
    nsIMIMEInfo			 : Components.interfaces.nsIMIMEInfo,
    nsIFilePicker        : Components.interfaces.nsIFilePicker,

    // Fill dialog from app launcher attributes.
    initDialog : function () {
        // "Always ask me" is always set (or else we wouldn't have got here!).
        document.getElementById( "alwaysAskMe" ).checked = true;
        document.getElementById( "alwaysAskMe" ).setAttribute("disabled", "true");
  
        // Pre-select the choice the user made last time.
        this.chosenApp = this.appLauncher.MIMEInfo.preferredApplicationHandler;
        var applicationDescription = this.appLauncher.MIMEInfo.applicationDescription;
        if (applicationDescription != "")
           document.getElementById( "appName" ).value = applicationDescription;
        else if (this.chosenApp)
        {
          // If a user-chosen application, show its path.
          document.getElementById( "appName" ).value = this.chosenApp.unicodePath;
        }

        if ( applicationDescription  && this.appLauncher.MIMEInfo.preferredAction != this.nsIMIMEInfo.saveToDisk ) {
            document.getElementById( "runApp" ).checked = true;         
        } else {
            // Save to disk.
            document.getElementById( "saveToDisk" ).checked = true;
            // Disable choose app button.
            document.getElementById( "chooseApp" ).setAttribute( "disabled", "true" );
        }

        // Put content type into dialog text.
        var html = document.getElementById( "intro" );
        if ( html && html.childNodes && html.childNodes.length ) {
            // Get raw text.
            var text = html.childNodes[ 0 ].nodeValue;
            // Substitute content type for "#1".
            text = text.replace( /#1/, this.appLauncher.MIMEInfo.MIMEType );

            // Replace #2 with product name.
            var brandBundle = srGetStrBundle("chrome://global/locale/brand.properties");
            if ( brandBundle ) {
                var product = brandBundle.GetStringFromName( "brandShortName" );
                text = text.replace( /#2/, product );
            }

            // Replace text in document.
            html.childNodes[ 0 ].nodeValue = text;
        }

        // Set up dialog button callbacks.
        var object = this;
        doSetOKCancel( function () { return object.onOK(); },
                       function () { return object.onCancel(); } );
        moveToAlertPosition();
    },

    // If the user presses OK, we do as requested...
    onOK : function () {
        // Get boolean switch from checkbox.
        var dontAskNextTime = !document.getElementById( "alwaysAskMe" ).checked;

        // this.appLauncher.MIMEInfo.alwaysAskBeforeHandling = document.getElementById( "alwaysAskMe" ).checked;
    
        if ( document.getElementById( "runApp" ).checked ) {
            // Update preferred action if the user chose an app.
            if ( this.userChoseApp ) {
                this.appLauncher.MIMEInfo.preferredAction = this.nsIHelperAppLauncher.useHelperApp;
            }
            this.appLauncher.launchWithApplication( this.chosenApp, dontAskNextTime );
        } else {
            this.appLauncher.MIMEInfo.preferredAction = this.nsIHelperAppLauncher.saveToDisk;
            try {
              this.appLauncher.saveToDisk( null, dontAskNextTime );
            } catch (exception) { 
            }
        }
    
        window.close();
    },

    // If the user presses cancel, tell the app launcher and close the dialog...
    onCancel : function () {
        // Cancel app launcher.
        try {
            this.appLauncher.Cancel();
        } catch( exception ) {
        }
    
        // Close up dialog by returning true.
        return true;
    },

    // Enable pick app button if the user chooses that option.
    toggleChoice : function () {
        // See what option is checked.
        if ( document.getElementById( "runApp" ).checked ) {
            // We can enable the pick app button.
            document.getElementById( "chooseApp" ).removeAttribute( "disabled" );
        } else {
            // We can disable the pick app button.
            document.getElementById( "chooseApp" ).setAttribute( "disabled", "true" );
        }
    },

    // Choose a new/different app...
    chooseApp : function () {
        var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance( this.nsIFilePicker );
        fp.init( window,
                 this.getString( "chooseAppFilePickerTitle" ),
                 this.nsIFilePicker.modeOpen );
        // XXX - We want to say nsIFilePicker.filterExecutable or something
        fp.appendFilters( this.nsIFilePicker.filterAll );
    
        if ( fp.show() == this.nsIFilePicker.returnOK && fp.file ) {
            // Remember the file they chose to run.
            this.userChoseApp = true;
            this.chosenApp    = fp.file;
            // Update dialog.
            document.getElementById( "appName" ).value = this.chosenApp.unicodePath;
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
    }
}
