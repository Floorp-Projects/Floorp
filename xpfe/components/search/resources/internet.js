
var	gText = "";
var	gSites = "";

function doSearch()
{
	gText = "";
	gSites = "";

	// get user text to find
	var textNode = document.getElementById("searchtext");
	if (!textNode)	return(false);
	var text = textNode.value;
	if (!text)	return(false);
	dump("Search text: " + text + "\n");

	// get selected search engines
	var treeNode = document.getElementById("NC:SearchEngineRoot");
	if (!treeNode)	return(false);
	var treeChildrenNode = null;
	var numChildren = treeNode.childNodes.length;
	for (var x = 0; x<numChildren; x++)
	{
		if (treeNode.childNodes[x].tagName == "treechildren")
		{
			treeChildrenNode = treeNode.childNodes[x];
			break;
		}
	}
	if (treeChildrenNode == null)	return(false);

	gText = text;

	var searchURL="";
	var foundEngine = false;

	var numEngines = treeChildrenNode.childNodes.length;
	dump("Found treebody, it has " + numEngines + " kids\n");
	for (var x = 0; x<numEngines; x++)
	{
		var treeItem = treeChildrenNode.childNodes[x];
		if (!treeItem)	continue;
		// XXX when its fully implemented, instead use
		//     var engines = document.getElementsByTagName("checkbox");
		if (treeItem.childNodes[0].childNodes[0].childNodes[0].getAttribute("value") == "1")
		{
			var engineURI = treeItem.getAttribute("id");
			if (!engineURI)	continue;
			dump ("# " + x + ":  " + engineURI + "\n");

			var searchEngineName = treeItem.childNodes[0].childNodes[1].childNodes[0].getAttribute("value");
			if (searchEngineName != "")
			{
				if (gSites != "")
				{
					gSites += ", ";
				}
				gSites += searchEngineName;
			}

			if (searchURL == "")
			{
				searchURL = "internetsearch:";
			}
			else
			{
				searchURL += "&";
			}
			searchURL += "engine=" + engineURI;
			foundEngine = true;
		}
	}
	if (foundEngine == false)	return(false);

	searchURL += "&text=" + escape(text);
	dump("Internet Search URL: " + searchURL + "\n");

	// set text in results pane
	var summaryNode = parent.frames[1].document.getElementById("internetresultssummary");
	if (summaryNode)
	{
		var summaryText = "Results: term contains '";
		summaryText += text + "'";
		summaryNode.setAttribute("value", summaryText);
	}

	// load find URL into results pane
	var resultsTree = parent.frames[1].document.getElementById("internetresultstree");
	if (!resultsTree)	return(false);
        resultsTree.setAttribute("ref", searchURL);

	// enable "Save Search" button
	var searchButton = document.getElementById("SaveSearch");
	if (searchButton)
	{
		searchButton.removeAttribute("disabled", "false");
	}

	dump("doSearch done.\n");

	return(true);
}

function doUncheckAll()
{
	// get selected search engines
	var treeNode = document.getElementById("NC:SearchEngineRoot");
	if (!treeNode)	return(false);
	var treeChildrenNode = null;
	var numChildren = treeNode.childNodes.length;
	for (var x = 0; x<numChildren; x++)
	{
		if (treeNode.childNodes[x].tagName == "treechildren")
		{
			treeChildrenNode = treeNode.childNodes[x];
			break;
		}
	}
	if (treeChildrenNode == null)	return(false);

	var numEngines = treeChildrenNode.childNodes.length;
	dump("Found treebody, it has " + numEngines + " kids\n");
	for (var x = 0; x<numEngines; x++)
	{
		var treeItem = treeChildrenNode.childNodes[x];
		if (!treeItem)	continue;
		// XXX when its fully implemented, instead use
		//     var engines = document.getElementsByTagName("checkbox");
		if (treeItem.childNodes[0].childNodes[0].childNodes[0].getAttribute("value") == "1")
		{
			treeItem.childNodes[0].childNodes[0].childNodes[0].setAttribute("value", "0");
		}
	}

	dump("doUncheckAll() done.\n");

	return(true);
}



function saveSearch()
{
	var resultsTree = parent.frames[1].document.getElementById("internetresultstree");
	if (!resultsTree)	return(false);
	var searchURL = resultsTree.getAttribute("ref");
	if ((!searchURL) || (searchURL == ""))		return(false);

	dump("Bookmark search URL: " + searchURL + "\n");

	var bmks = Components.classes["component://netscape/browser/bookmarks-service"].getService();
	if (bmks)	bmks = bmks.QueryInterface(Components.interfaces.nsIBookmarksService);

	var searchTitle = "Search: '" + gText + "' using " + gSites;
	if (bmks)	bmks.AddBookmark(searchURL, searchTitle);

	return(true);
}
