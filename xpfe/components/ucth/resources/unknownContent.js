/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 */

var data;
var dialog;

function initData() {
    // Create data object and initialize.
    data = new Object;
    data.location      = window.arguments[0];
    data.contentType   = window.arguments[1];
}

function initDialog() {
    // Create dialog object and initialize.
    dialog = new Object;
    dialog.contentType = document.getElementById("dialog.contentType");
dump("dialog.contentType="+dialog.contentType+"\n");
    dialog.more        = document.getElementById("dialog.more");
    dialog.pick        = document.getElementById("dialog.pick");
    dialog.save        = document.getElementById("dialog.save");
    dialog.cancel      = document.getElementById("dialog.cancel");
}

function loadDialog() {
    // Set initial dialog field contents.
    dialog.contentType.childNodes[0].nodeValue = " " + data.contentType;
}

function onLoad() {
    // Init data.
    initData();

    // Init dialog.
    initDialog();

    // Fill dialog.
    loadDialog();
}

function onUnload() {
    // Nothing for now.
}

function more() {
    // Have parent browser window go to appropriate web page.
    var moreInfo = "http://cgi.netscape.com/cgi-bin/plug-in_finder.cgi?";
    moreInfo += data.contentType;
    window.opener.content.location = moreInfo;
}

function pick() {
    alert( "PickApp not implemented yet!" );
}

function save() {
    // Use stream xfer component to prompt for destination and save.
    var xfer = Components.classes[ "component://netscape/appshell/component/xfer" ].getService();
    xfer = xfer.QueryInterface( Components.interfaces.nsIStreamTransfer );
    try {
        // When Necko lands, we need to receive the real nsIChannel and
        // do SelectFileAndTransferLocation!

        // Use this for now...
        xfer.SelectFileAndTransferLocationSpec( data.location, window.opener );
    } catch( exception ) {
        // Failed (or cancelled), give them another chance.
        dump( "SelectFileAndTransferLocationSpec failed, rv=" + exception + "\n" );
        return;
    }
    // Save underway, close this dialog.
    window.close();
}

function cancel() {
    // Close the window.
    window.close();
}
