/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

var data;
var dialog;
var bundle;

function initData() 
{
    // Create data object and initialize.
    data = new Object;
    data.channel     = window.arguments[0];
    data.contentType = window.arguments[1];
    data.contentDisp = window.arguments[2];

    // Get location from channel.
    data.location = data.channel.URI.spec;
}

function initDialog() {
    // Create dialog object and initialize.
    dialog = new Object;
    dialog.contentType = document.getElementById("dialog.contentType");
    dialog.more        = document.getElementById("dialog.more");
    bundle = srGetStrBundle("chrome://global/locale/unknownContent.properties");

    var pickApp = document.getElementById("Button2");
    pickApp.setAttribute( "style", "display: inherit" );
    pickApp.setAttribute( "value", bundle.GetStringFromName("pick") );
    var saveButton = document.getElementById("ok");
    saveButton.setAttribute( "value", bundle.GetStringFromName("save") );
    doSetOKCancel( save, null, pick, null );
}

function loadDialog() 
{
  // Set initial dialog field contents.

  // Get raw content.
  var text = dialog.contentType.childNodes[0].nodeValue;

  // Replace #1 with actual content type.
  text = text.replace( /#1/, data.contentType );

  // Put it back into the dialog.
  dialog.contentType.childNodes[0].nodeValue = text;
}

function onUnload() 
{
  // do nothing
}

function onLoad() 
{
    // Init data.
    initData();

    // Init dialog.
    initDialog();

    // Fill dialog.
    loadDialog();
}


function more() 
{
    // Have parent browser window go to appropriate web page.
    var moreInfo = "http://cgi.netscape.com/cgi-bin/plug-in_finder.cgi?";
    moreInfo += data.contentType;
    window.opener._content.location = moreInfo;
}

function pick() 
{
    alert( "PickApp not implemented yet!" );
}

// Parse out suggested file name from content disposition.
function fileNameFromContentDisp( contentDisp ) {
    // content-disposition: has format
    //     disposition-type < ; filename=value >
    var result = "";
    try {
        result = contentDisp.toString().match( /.*;\s*filename\s*=\s*(\S*)/ )[ 1 ];
    } catch ( exception ) {
        // ignored
    }
    return result;

}

function save() 
{
    // Use stream xfer component to prompt for destination and save.
    var xfer = Components.classes[ "@mozilla.org/appshell/component/xfer;1" ].getService();
    xfer = xfer.QueryInterface( Components.interfaces.nsIStreamTransfer );
    var retry = false;
    try {
        var result = xfer.SelectFileAndTransferLocationSpec( data.location,
                                                             window,
                                                             data.contentType.toString(),
                                                             fileNameFromContentDisp( data.contentDisp ),
                                                             true,
                                                             null );
    } catch( exception ) {
        // Failed (or cancelled), give them another chance.
        retry = true;
    }
    // If save underway, close this dialog.
    if ( !retry ) {
        window.setTimeout( "window.close();", 10 );
    }
}

function cancel() 
{
    // Close this dialog.
    window.close();
}
