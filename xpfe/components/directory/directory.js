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
    dump(msg);
}



// get handle to the BrowserAppCore in the content area.
var appCore = window._content.appCore;



function Init()
{
    debug("directory.js: Init()\n");

    // Initialize the tree's base URL to whatever the HTTPIndex is rooted at
    var baseURI = HTTPIndex.BaseURL;

    // fix bug # 37102: if its a FTP directory
    // ensure it ends with a trailing slash
    if (baseURI && (baseURI.indexOf("ftp://") == 0) &&
    	(baseURI.substr(baseURI.length - 1) != "/"))
    {
    	debug("append traiing slash to FTP directory URL\n");
    	baseURI += "/";
    }
    debug("base URL = " + baseURI + "\n");

    // Note: Add the HTTPIndex datasource into the tree
    var tree = document.getElementById('tree');

    // Note: set encoding BEFORE setting "ref" (important!)
    var RDF = Components.classes["component://netscape/rdf/rdf-service"].getService();
    if (RDF)    RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
    if (RDF)
    {
        var httpDS = RDF.GetDataSource("rdf:httpindex");
        if (httpDS) httpDS = httpDS.QueryInterface(Components.interfaces.nsIHTTPIndex);
        if (httpDS)
        {
            httpDS.encoding = "ISO-8859-1";

            // Use a default character set.
            if (window._content.defaultCharacterset)
            {
                httpDS.encoding = window._content.defaultCharacterset;
            }
        }
    }

    // set window title
    var theWindow = window._content.parentWindow;
    if (theWindow)
    {
        theWindow.title = baseURI;
    }

    // root the tree (do this last)
    tree.setAttribute("ref", baseURI);
}



function OnClick(event, node)
{
    if( event.type == "click" &&
        ( event.button != 1 || event.detail != 2 || node.nodeName != "treeitem") )
      return(false);
    if( event.type == "keypress" && event.which != 13 )
      return(false);

    var tree = document.getElementById("tree");
    if( tree.selectedItems.length == 1 ) 
      {
        var selectedItem = tree.selectedItems[0];
        var theID = selectedItem.getAttribute("id");

        //if( selectedItem.getAttribute( "type" ) == "FILE" )
		if(appCore)
		{
		    // support session history (if appCore is available)
            appCore.loadUrl(theID);
		}
		else
		{
		    // fallback case (if appCore isn't available)
            window._content.location.href = theID;
		}

        // set window title
        var theWindow = window._content.parentWindow;
        if (theWindow)
        {
            theWindow.title = theID;
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
