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
