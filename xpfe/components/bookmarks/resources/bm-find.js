/* -*- Mode: Java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *  Original Author(s):
 *    Robert John Churchill    <rjc@netscape.com>
 *
 * Contributor(s): 
 */


function debug(msg)
{
	// uncomment for noise
	// dump(msg+"\n");
}



function doLoad()
{
	// disable "Save Search" button (initially)
	var searchButton = document.getElementById("SaveSearch");
	if (searchButton)
	{
		searchButton.setAttribute("disabled", "true");
	}

	// set initial focus
	var findtext = document.getElementById("findtext");
	if (findtext)
	{
		findtext.focus();
	}
}



function doUnload()
{
    // Get the current window position/size.
    var x = window.screenX;
    var y = window.screenY;
    var h = window.outerHeight;
    var w = window.outerWidth;

    // Store these into the window attributes (for persistence).
    var win = document.getElementById( "bookmark-find-window" );
    win.setAttribute( "x", x );
    win.setAttribute( "y", y );
    win.setAttribute( "height", h );
    win.setAttribute( "width", w );
}




var	gDatasourceName = "";
var	gMatchName = "";
var	gMethodName = "";
var	gTextName = "";

function doFind()
{
	gDatasourceName = "";
	gMatchName = "";
	gMethodName = "";
	gTextName = "";

	// get RDF datasource to query
	var datasourceNode = document.getElementById("datasource");
	if (!datasourceNode)	return(false);
	var datasource = datasourceNode.selectedItem.getAttribute("data");
	gDatasourceName = datasourceNode.selectedItem.getAttribute("value");
	debug("Datasource: " + gDatasourceName + "\n");

	// get match
	var matchNode = document.getElementById("match");
	if (!matchNode)		return(false);
	var match = matchNode.selectedItem.getAttribute("data");
	gMatchName = matchNode.selectedItem.getAttribute("value");
	debug("Match: " + match + "\n");

	// get method
	var methodNode = document.getElementById("method");
	if (!methodNode)	return(false);
	var method = methodNode.selectedItem.getAttribute("data");
	gMethodName = methodNode.selectedItem.getAttribute("value");
	debug("Method: " + method + "\n");

	// get user text to find
	var textNode = document.getElementById("findtext");
	if (!textNode)	return(false);
	gTextName = textNode.value;
	if (!gTextName || gTextName=="")	return(false);
	debug("Find text: " + gTextName + "\n");

	// construct find URL
	var url = "find:datasource=" + datasource;
	url += "&match=" + match;
	url += "&method=" + method;
	url += "&text=" + escape(gTextName);
	debug("Find URL: " + url + "\n");

	// load find URL into results pane
	var resultsTree = document.getElementById("findresultstree");
	if (!resultsTree)	return(false);
        resultsTree.setAttribute("ref", "");
        resultsTree.setAttribute("ref", url);

	// enable "Save Search" button
	var searchButton = document.getElementById("SaveSearch");
	if (searchButton)
	{
		searchButton.removeAttribute("disabled", "true");
	}

	debug("doFind done.\n");
	return(true);
}



function saveFind()
{
	var resultsTree = document.getElementById("findresultstree");
	if (!resultsTree)	return(false);
	var searchURL = resultsTree.getAttribute("ref");
	if ((!searchURL) || (searchURL == ""))		return(false);

	debug("Bookmark search URL: " + searchURL + "\n");
	var searchTitle = "Find: " + gMatchName + " " + gMethodName + " '" + gTextName + "' in " + gDatasourceName;
	debug("Title: " + searchTitle + "\n\n");

	var bmks = Components.classes["@mozilla.org/browser/bookmarks-service;1"].getService();
	if (bmks)	bmks = bmks.QueryInterface(Components.interfaces.nsIBookmarksService);
	if (bmks)	bmks.AddBookmark(searchURL, searchTitle, bmks.BOOKMARK_FIND_TYPE, null);

	return(true);
}



function getAbsoluteID(root, node)
{
	var url = node.getAttribute("ref");
	if ((url == null) || (url == ""))
	{
		url = node.getAttribute("id");
	}
	try
	{
		var rootNode = document.getElementById(root);
		var ds = null;
		if (rootNode)
		{
			ds = rootNode.database;
		}

		// add support for anonymous resources such as Internet Search results,
		// IE favorites under Win32, and NetPositive URLs under BeOS
		var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
		if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
		if (rdf && ds)
		{
			var src = rdf.GetResource(url, true);
			var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
			var target = ds.GetTarget(src, prop, true);
			if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
			if (target)	target = target.Value;
			if (target)	url = target;
		}
	}
	catch(ex)
	{
	}
	return(url);
}



function OpenURL(event, node, root)
{
	if ((event.button != 1) || (event.detail != 2) || (node.nodeName != "treeitem"))
		return(false);

	if (node.getAttribute("container") == "true")
		return(false);

	var url = getAbsoluteID(root, node);

	// Ignore "NC:" urls.
	if (url.substring(0, 3) == "NC:")
		return(false);

	// get right sized window
	window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url );
	return(true);
}



/* Note: doSort() does NOT support natural order sorting, unless naturalOrderResource is valid,
         in which case we sort ascending on naturalOrderResource
 */
function doSort(sortColName, naturalOrderResource)
{
	var node = document.getElementById(sortColName);
	// determine column resource to sort on
	var sortResource = node.getAttribute('resource');
	if (!sortResource) return(false);

	var sortDirection="ascending";
	var isSortActive = node.getAttribute('sortActive');
	if (isSortActive == "true")
	{
		sortDirection = "ascending";

		var currentDirection = node.getAttribute('sortDirection');
		if (currentDirection == "ascending")
		{
			if (sortResource != naturalOrderResource)
			{
				sortDirection = "descending";
			}
		}
		else if (currentDirection == "descending")
		{
			if (naturalOrderResource != null && naturalOrderResource != "")
			{
				sortResource = naturalOrderResource;
			}
		}
	}

	var isupports = Components.classes["@mozilla.org/rdf/xul-sort-service;1"].getService();
	if (!isupports)    return(false);
	var xulSortService = isupports.QueryInterface(Components.interfaces.nsIXULSortService);
	if (!xulSortService)    return(false);
	try
	{
		xulSortService.Sort(node, sortResource, sortDirection);
	}
	catch(ex)
	{
		debug("Exception calling xulSortService.Sort()");
	}
	return(true);
}
