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

/*

   Script for the directory window

*/

// By the time this runs, The 'HTTPIndex' variable will have been
// magically set on the global object by the native code.

function debug(msg)
{
    // Uncomment to print out debug info.
    //dump(msg);
}

function Init()
{
    debug("directory.js: Init()\n");

    // Add the HTTPIndex datasource into the tree
    var tree = document.getElementById('tree');
    tree.database.AddDataSource(HTTPIndex.DataSource);

    // Initialize the tree's base URL to whatever the HTTPIndex is rooted at
    debug("base URL = " + HTTPIndex.BaseURL + "\n");
    tree.setAttribute('ref', HTTPIndex.BaseURL);
}


function OnClick(event)
{
    debug('OnClick()\n');

    // This'll be set to 'twisty' on the twisty icon, and 'filename'
    // if they're over the filename link.
    var targetclass = event.target.getAttribute('class');
    debug('targetclass = ' + targetclass + '\n');

    if (targetclass == 'twisty') {
        // The twisty is nested three below the treeitem:
        // <treeitem>
        //   <treerow>
        //     <treecell>
        //       <box> <!-- anonymous -->
        //         <titledbutton class="twisty"> <!-- anonymous -->
        var treeitem = event.target.parentNode.parentNode.parentNode.parentNode;
        ToggleOpenState(treeitem);
    }
    else {
        // The click'll have hit a cell, which is nested two below the
        // treeitem.
        var treeitem = event.target.parentNode.parentNode;

        // This'll be set to 'FILE' for files and 'DIRECTORY' for
        // directories.
        var type = treeitem.getAttribute('type');

        if (targetclass == 'filename' && type == 'FILE') {
            var url = treeitem.getAttribute('id');

            debug('navigating to ' + url + '\n');
            window.content.location.href = url;
        }
    }
}


function OnDblClick(event)
{
    debug('OnDblClick()\n');

    // This'll be set to 'filename' if they're over the filename link.
    var targetclass = event.target.getAttribute('class');

    // This'll be the treeitem that got the event
    var treeitem = event.target.parentNode.parentNode;

    // This'll be set to 'FILE' for files and 'DIRECTORY' for
    // directories.
    var type = treeitem.getAttribute('type');

    if (targetclass == 'filename' && type == 'DIRECTORY') {
        ToggleOpenState(treeitem);
    }
}


function ToggleOpenState(treeitem)
{
    debug('ToggleOpenState(' + treeitem.tagName + ')');

    if (treeitem.getAttribute('open') == 'true') {
        var url = treeitem.getAttribute('id');
        ReadDirectory(url);
    }
}

// This array contains the URL of each directory we've read, so that
// we don't load a directory into the datasource >1 time.
var Read = new Array();

function ReadDirectory(url)
{
    // Check to make sure we haven't read the directory yet...
    if (Read[url])
        return;

    debug('ReadDirectory(' + url + ')\n');

    var ios = Components.classes['component://netscape/network/net-service'].getService();
    ios = ios.QueryInterface(Components.interfaces.nsIIOService);

    var uri = ios.NewURI(url, null);

    // Create a channel...
    var channel = ios.NewChannelFromURI('load', uri, null);

    // ...so that we can pipe it into a new HTTPIndex listener to
    // parse the directory's contents.
    channel.AsyncRead(0, -1, null, HTTPIndex.CreateListener());
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


