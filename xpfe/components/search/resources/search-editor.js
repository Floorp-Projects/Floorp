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



// global(s)
var pref = null;
try
{
	pref = Components.classes["component://netscape/preferences"].getService();
	if (pref)	pref = pref.QueryInterface( Components.interfaces.nsIPref );
}
catch(e)
{
	pref = null;
}



function debug(msg)
{
	// uncomment for noise
	// dump(msg+"\n");
}



function doLoad()
{
	// set up action buttons
	doSetOKCancel(Commit, Cancel);

	// adjust category popup
	var internetSearch = Components.classes["component://netscape/rdf/datasource?name=internetsearch"].getService();
	if (internetSearch)	internetSearch = internetSearch.QueryInterface(Components.interfaces.nsIInternetSearchService);
	if (internetSearch)
	{
		var catDS = internetSearch.GetCategoryDataSource();
		if (catDS)	catDS = catDS.QueryInterface(Components.interfaces.nsIRDFDataSource);
		if (catDS)
		{
			var categoryList = document.getElementById("categoryList");
			if (categoryList)
			{
				categoryList.database.AddDataSource(catDS);
				var ref = categoryList.getAttribute("ref");
				if (ref)	categoryList.setAttribute("ref", ref);
			}
			var engineList = document.getElementById("engineList");
			if (engineList)
			{
				engineList.database.AddDataSource(catDS);
			}
		}
	}

	// try and determine last category name used
	var lastCategoryName = "";
	try
	{
		var pref = Components.classes["component://netscape/preferences"].getService();
		if (pref)	pref = pref.QueryInterface( Components.interfaces.nsIPref );
		if (pref)	lastCategoryName = pref.CopyCharPref( "browser.search.last_search_category" );

		if (lastCategoryName != "")
		{
			// strip off the prefix if necessary
			var prefix="NC:SearchCategory?category=";
			if (lastCategoryName.indexOf(prefix) == 0)
			{
				lastCategoryName = lastCategoryName.substr(prefix.length);
			}
		}

	}
	catch( e )
	{
		debug("Exception in SearchPanelStartup\n");
		lastCategoryName = "";
	}
	debug("\nSearchPanelStartup: lastCategoryName = '" + lastCategoryName + "'\n");

	// select the appropriate category
	var categoryList = document.getElementById( "categoryList" );
	var categoryPopup = document.getElementById( "categoryPopup" );
	if( categoryList && categoryPopup )
	{
	 	var found = false;
		for( var i = 0; i < categoryPopup.childNodes.length; i++ )
		{
			if( ( lastCategoryName == "" && categoryPopup.childNodes[i].getAttribute("data") == "NC:SearchEngineRoot" ) ||
			    ( categoryPopup.childNodes[i].getAttribute("id") == lastCategoryName ) )
			{
				categoryList.selectedItem = categoryPopup.childNodes[i];
				found = true;
				break;
			}
		}
		if (found == false)
		{
			categoryList.selectedItem = categoryPopup.childNodes[0];
		}
		chooseCategory(categoryList.selectedItem);
	}

}



function Cancel()
{
	// Ignore any changes.
	window.close();
}



function Commit()
{
}



function doUnload()
{
    // Get the current window position/size.
    var x = window.screenX;
    var y = window.screenY;
    var h = window.outerHeight;
    var w = window.outerWidth;

    // Store these into the window attributes (for persistence).
    var win = document.getElementById( "search-editor-window" );
    win.setAttribute( "x", x );
    win.setAttribute( "y", y );
    win.setAttribute( "height", h );
    win.setAttribute( "width", w );
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

	var isupports = Components.classes["component://netscape/rdf/xul-sort-service"].getService();
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



function chooseCategory( aNode )
{
	var category = "NC:SearchCategory?category=" + aNode.getAttribute("id");
	debug("chooseCategory: '" + category + "'\n");

	var treeNode = document.getElementById("engineList");
	if (treeNode)
	{
		dump("\nSet search engine list to category='" + category + "'\n\n");
		treeNode.setAttribute( "ref", category );
	}
	return(true);
}



function AddEngine()
{
	return(true);
}



function RemoveEngine()
{
	return(true);
}



function MoveUp()
{
	return(true);
}



function MoveDown()
{
	return(true);
}



function NewCategory()
{
	return(true);
}



function RenameCategory()
{
	return(true);
}



function RemoveCategory()
{
	return(true);
}
