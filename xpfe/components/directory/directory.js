/* -*- Mode: Java; tab-width: 4; c-basic-offset: 4; -*-
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
 */

// By the time this runs, The 'HTTPIndex' variable will have been
// magically set on the global object by the native code.

function Init()
{
    dump("directory.js: Init()\n");

    // Add the HTTPIndex datasource into the tree
    var tree = document.getElementById('tree');
    tree.database.AddDataSource(HTTPIndex.DataSource);

    // Initialize the tree's base URL to whatever the HTTPIndex is rooted at
    dump("base URL = " + HTTPIndex.BaseURL + "\n");
    tree.setAttribute('ref', HTTPIndex.BaseURL);
}


function OnDblClick(event)
{
    var item = event.target.parentNode.parentNode;
    var url = item.getAttribute('id');

    var type = item.getAttribute('type');
    if (type == 'DIRECTORY') {
        // open inline
        dump('reading directory ' + url + '\n');
        ReadDirectory(url);
    }
    else {
        dump('navigating to ' + url + '\n');
        window.content.location.href = url;
    }
}


function ToggleTwisty(event)
{
    var item = event.target.parentNode.parentNode.parentNode;
    if (item.getAttribute('open') == 'true') {
        item.removeAttribute('open');
    }
    else {
        var url = item.getAttribute('id');
        ReadDirectory(url);
        item.setAttribute('open', 'true');
    }

    // XXX need to cancel the event here!!!
}


var Read = new Array();

function ReadDirectory(url)
{
    if (Read[url]) {
        dump("directory already read\n");
        return;
    }

    var ios = Components.classes['component://netscape/network/net-service'].getService();
    ios = ios.QueryInterface(Components.interfaces.nsIIOService);
    dump("ios = " + ios + "\n");

    var uri = ios.NewURI(url, null);
    dump("uri = " + uri + "\n");

    var channel = ios.NewChannelFromURI('load', uri, null);
    dump("channel = " + channel + "\n");

    var listener = HTTPIndex.CreateListener();
    dump("listener = " + listener + "\n");

    channel.AsyncRead(0, -1, null, listener);

    Read[url] = true;
}



// We need this hack because we've completely circumvented the onload() logic.
function Boot()
{
    if (document.getElementById('tree')) {
        Init();
    }
    else {
        setTimeout("Boot()", 500);
    }
}

setTimeout("Boot()", 0);


