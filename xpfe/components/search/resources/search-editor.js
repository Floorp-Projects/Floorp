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
var bundle = srGetStrBundle("chrome://search/locale/search-editor.properties");
var pref = null;
var RDF = null;
var RDFC = null;
var RDFCUtils = null;
var catDS = null;

try
{
	pref = Components.classes["component://netscape/preferences"].getService();
	if (pref)	pref = pref.QueryInterface( Components.interfaces.nsIPref );

	RDF = Components.classes["component://netscape/rdf/rdf-service"].getService();
	if (RDF)	RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

	RDFC = Components.classes["component://netscape/rdf/container"].getService();
	if (RDFC)	RDFC = RDFC.QueryInterface(Components.interfaces.nsIRDFContainer);

	RDFCUtils = Components.classes["component://netscape/rdf/container-utils"].getService();
	if (RDFCUtils)	RDFCUtils = RDFCUtils.QueryInterface(Components.interfaces.nsIRDFContainerUtils);
}
catch(e)
{
	pref = null;
	RDF = null;
	RDFC = null;
	RDFCUtils = null;
}



function debug(msg)
{
	// uncomment for noise
	// dump(msg+"\n");
}



function doLoad()
{
	// adjust category popup
	var internetSearch = Components.classes["component://netscape/rdf/datasource?name=internetsearch"].getService();
	if (internetSearch)	internetSearch = internetSearch.QueryInterface(Components.interfaces.nsIInternetSearchService);
	if (internetSearch)
	{
		catDS = internetSearch.GetCategoryDataSource();
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
		if (categoryPopup.childNodes.length > 0)
		{
			if (found == false)
			{
				categoryList.selectedItem = categoryPopup.childNodes[0];
			}
			chooseCategory(categoryList.selectedItem);
		}
	}

}



function Commit()
{
	// flush
	if (catDS)
	{
		var flushableDS = catDS.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
		if (flushableDS)
		{
			flushableDS.Flush();
		}
	}

	window.close();
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
		debug("\nSet search engine list to category='" + category + "'\n\n");
		treeNode.setAttribute( "ref", category );
	}
	return(true);
}



function AddEngine()
{
	var allenginesList = document.getElementById("allengines");
	if (!allenginesList)	return(false);
	var select_list = allenginesList.selectedItems;
	if (!select_list)	return(false)
	if (select_list.length < 1)	return(false);

	// make sure we have at least one category to add engine into
	var categoryPopup = document.getElementById( "categoryPopup" );
	if (!categoryPopup)	return(false);
	if (categoryPopup.childNodes.length < 1)
	{
		if (NewCategory() == false)	return(false);
	}

	var promptStr = bundle.GetStringFromName("AddEnginePrompt");
	if (!confirm(promptStr))	return(false);

	var engineList = document.getElementById("engineList");
	if (!engineList)	return(false);

	var ref = engineList.getAttribute("ref");
	if ((!ref) || (ref == ""))	return(false);

	var categoryRes = RDF.GetResource(ref);
	if (!categoryRes)	return(false);

	RDFCUtils.MakeSeq(catDS, categoryRes);

	RDFC.Init(catDS, categoryRes);

	for (var x = 0; x < select_list.length; x++)
	{
		var id = select_list[x].getAttribute("id");
		if ((!id) || (id == ""))	return(false);
		var idRes = RDF.GetResource(id);
		if (!idRes)	return(false);

		var nodeIndex = RDFC.IndexOf(idRes);
		if (nodeIndex < 1)
		{
			RDFC.AppendElement(idRes);
		}
	}

	return(true);
}



function RemoveEngine()
{
	var engineList = document.getElementById("engineList");
	if (!engineList)	return(false);
	var select_list = engineList.selectedItems;
	if (!select_list)	return(false)
	if (select_list.length < 1)	return(false);

	var promptStr = bundle.GetStringFromName("RemoveEnginePrompt");
	if (!confirm(promptStr))	return(false);

	var ref = engineList.getAttribute("ref");
	if ((!ref) || (ref == ""))	return(false);
	var categoryRes = RDF.GetResource(ref);
	if (!categoryRes)	return(false);

	RDFC.Init(catDS, categoryRes);

	// Note: walk backwards to make deletion easier
	for (var x = select_list.length - 1; x >= 0; x--)
	{
		var id = select_list[x].getAttribute("id");
		if ((!id) || (id == ""))	return(false);
		var idRes = RDF.GetResource(id);
		if (!idRes)	return(false);

		var nodeIndex = RDFC.IndexOf(idRes);
		if (nodeIndex > 0)
		{
			RDFC.RemoveElementAt(nodeIndex, true, idRes);
		}
	}
	
	return(true);
}



function MoveUp()
{
	return	MoveDelta(-1);
}



function MoveDown()
{
	return	MoveDelta(1);
}



function MoveDelta(delta)
{
	var engineList = document.getElementById("engineList");
	if (!engineList)	return(false);
	var select_list = engineList.selectedItems;
	if (!select_list)	return(false)
	if (select_list.length != 1)	return(false);

	var ref = engineList.getAttribute("ref");
	if ((!ref) || (ref == ""))	return(false);
	var categoryRes = RDF.GetResource(ref);
	if (!categoryRes)	return(false);

	var id = select_list[0].getAttribute("id");
	if ((!id) || (id == ""))	return(false);
	var idRes = RDF.GetResource(id);
	if (!idRes)	return(false);

	RDFC.Init(catDS, categoryRes);

	var nodeIndex = RDFC.IndexOf(idRes);
	if (nodeIndex < 1)	return(false);		// how did that happen?

	var numItems = RDFC.GetCount();

	var newPos = -1;
	if (((delta == -1) && (nodeIndex > 1)) ||
		((delta == 1) && (nodeIndex < numItems)))
	{
		newPos = nodeIndex + delta;
		RDFC.RemoveElementAt(nodeIndex, true, idRes);
	}
	if (newPos > 0)
	{
		RDFC.InsertElementAt(idRes, newPos, true);
	}

	selectItems(engineList, ref, id);

	return(true);
}



function NewCategory()
{
	var promptStr = bundle.GetStringFromName("NewCategoryPrompt");
	var name = prompt(promptStr, "");
	if ((!name) || (name == ""))	return(false);

	var newName = RDF.GetLiteral(name);
	if (!newName)	return(false);

	var categoryRes = RDF.GetResource("NC:SearchCategoryRoot");
	if (!categoryRes)	return(false);

	RDFC.Init(catDS, categoryRes);

	var randomID = Math.random();
	var categoryID = "NC:SearchCategory?category=urn:search:category:" + randomID.toString();
	var currentCatRes = RDF.GetResource(categoryID);
	if (!currentCatRes)	return(false);

	var titleRes = RDF.GetResource("http://home.netscape.com/NC-rdf#title");
	if (!titleRes)	return(false);

	// set the category's name
	catDS.Assert(currentCatRes, titleRes, newName, true);

	// and insert the category
	RDFC.AppendElement(currentCatRes);

	RDFCUtils.MakeSeq(catDS, currentCatRes);

	// try and select the new category
	var categoryList = document.getElementById( "categoryList" );
	var select_list = categoryList.getElementsByAttribute("id", categoryID);
	if (select_list && select_list.length > 0)
	{
		categoryList.selectedItem = select_list[0];
		chooseCategory(categoryList.selectedItem);
	}
	return(true);
}



function RenameCategory()
{
	// make sure we have at least one category
	var categoryPopup = document.getElementById( "categoryPopup" );
	if (!categoryPopup)	return(false);
	if (categoryPopup.childNodes.length < 1)	return(false);

	var categoryList = document.getElementById( "categoryList" );
	var currentName = categoryList.selectedItem.getAttribute("value");
	var promptStr = bundle.GetStringFromName("RenameCategoryPrompt");
	var name = prompt(promptStr, currentName);
	if ((!name) || (name == "") || (name == currentName))	return(false);

	var currentCatID = categoryList.selectedItem.getAttribute("id");
	var currentCatRes = RDF.GetResource(currentCatID);
	if (!currentCatRes)	return(false);

	var titleRes = RDF.GetResource("http://home.netscape.com/NC-rdf#title");
	if (!titleRes)	return(false);

	var newName = RDF.GetLiteral(name);
	if (!newName)	return(false);

	var oldName = catDS.GetTarget(currentCatRes, titleRes, true);
	if (oldName)	oldName = oldName.QueryInterface(Components.interfaces.nsIRDFLiteral);
	if (oldName)
	{
		catDS.Change(currentCatRes, titleRes, oldName, newName);
	}
	else
	{
		catDS.Assert(currentCatRes, titleRes, newName, true);
	}

	// force popup to update
	chooseCategory(categoryList.selectedItem);

	return(true);
}



function RemoveCategory()
{
	// make sure we have at least one category
	var categoryPopup = document.getElementById( "categoryPopup" );
	if (!categoryPopup)	return(false);
	if (categoryPopup.childNodes.length < 1)	return(false);

	var promptStr = bundle.GetStringFromName("RemoveCategoryPrompt");
	if (!confirm(promptStr))	return(false);

	var categoryRes = RDF.GetResource("NC:SearchCategoryRoot");
	if (!categoryRes)	return(false);

	var categoryList = document.getElementById( "categoryList" );
	if (!categoryList)	return(false);

	var idNum = categoryList.selectedIndex;

	var id = categoryList.selectedItem.getAttribute("id");
	if ((!id) || (id == ""))	return(false);

	var idRes = RDF.GetResource(id);
	if (!idRes)	return(false);

	RDFC.Init(catDS, categoryRes);

	var nodeIndex = RDFC.IndexOf(idRes);
	if (nodeIndex < 1)	return(false);		// how did that happen?

	RDFC.RemoveElementAt(nodeIndex, true, idRes);

	// try and select another category
	if (idNum > 0)	--idNum;
	else		idNum = 0;

	if (categoryPopup.childNodes.length > 0)
	{
		categoryList.selectedItem = categoryPopup.childNodes[idNum];
		chooseCategory(categoryList.selectedItem);
	}
	else
	{
		// last category was deleted, so empty out engine list
		var treeNode = document.getElementById("engineList");
		if (treeNode)
		{
			treeNode.setAttribute( "ref", "" );
		}
	}
	return(true);
}



function selectItems(treeRoot, containerID, targetID)
{
	var select_list = treeRoot.getElementsByAttribute("id", targetID);
	for (var x=0; x<select_list.length; x++)
	{
		var node = select_list[x];
		if (!node)	continue;

		var parent = node.parentNode.parentNode;
		if (!parent)	continue;

		var id = parent.getAttribute("ref");
		if (!id || id=="")
		{
			id = parent.getAttribute("id");
		}
		if (!id || id=="")	continue;

		if (id == containerID)
		{
			node.setAttribute("selected", "true");
			break;
		}
	}
}
