/* -*- Mode: Java; tab-width: 4; c-basic-offset: 4; -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*

   Script for the directory window

*/

// By the time this runs, The 'HTTPIndex' variable will have been
// magically set on the global object by the native code.

function debug(msg)
{
    // Uncomment to print out debug info.
    // dump(msg);
}

function Init()
{
    debug("directory.js: Init()\n");

    // Add the HTTPIndex datasource into the tree
    var tree = document.getElementById('tree');

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

    if (targetclass != 'twisty') {
        // The click'll have hit a cell, which is nested two below the
        // treeitem.
        var treeitem = event.target.parentNode.parentNode;

        // This'll be set to 'FILE' for files and 'DIRECTORY' for
        // directories.
        var type = treeitem.getAttribute('type');

        if (targetclass == 'filename') {
            var url = treeitem.getAttribute('id');

            debug('navigating to ' + url + '\n');
            window.content.location.href = url;
        }
    }
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


function doSort(sortColName)
{
	var node = document.getElementById(sortColName);
	if (!node) return(false);

	// determine column resource to sort on
	var sortResource = node.getAttribute('resource');

	// switch between ascending & descending sort (no natural order support)
	var sortDirection="ascending";
	var isSortActive = node.getAttribute('sortActive');
	if (isSortActive == "true")
	{
		var currentDirection = node.getAttribute('sortDirection');
		if (currentDirection == "ascending")
		{
			sortDirection = "descending";
		}
	}

	try
	{
		var isupports = Components.classes["component://netscape/rdf/xul-sort-service"].getService();
		if (!isupports)    return(false);
		var xulSortService = isupports.QueryInterface(Components.interfaces.nsIXULSortService);
		if (!xulSortService)    return(false);
		xulSortService.Sort(node, sortResource, sortDirection);
	}
	catch(ex)
	{
	}
	return(false);
}
