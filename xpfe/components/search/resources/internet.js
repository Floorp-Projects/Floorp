
var	gText = "";
var	gSites = "";



function loadPage()
{
	// check and see if we need to do an automatic search
	if (window.parent)
	{
		var searchText = window.parent.getSearchText();
		if (searchText)
		{
			var textNode = document.getElementById("searchtext");
			if (textNode)
			{
				textNode.setAttribute("value", searchText);
				doSearch();
			}
		}
	}
}

function doCheckAll(activeFlag)
{
	// get selected search engines
	var treeNode = document.getElementById("searchengines");
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
	dump("doCheckAll():  " + numEngines + " engines.\n");

	for (var x = 0; x<numEngines; x++)
	{
		var treeItem = treeChildrenNode.childNodes[x];
		if (!treeItem)
		{
			dump("doCheckAll(): huh? treeItem is null.\n");
			continue;
		}

		var checkedFlag = false;

		if (treeItem.childNodes[0].childNodes[0].childNodes[0].checked == true)
		{
			checkedFlag = true;
		}
		else if (treeItem.childNodes[0].childNodes[0].childNodes[0].getAttribute("checked") == "1")
		{
			checkedFlag = true;
		}

		if (checkedFlag != activeFlag)
		{
			treeItem.childNodes[0].childNodes[0].childNodes[0].checked = activeFlag;
			if (activeFlag)
			{
				treeItem.childNodes[0].childNodes[0].childNodes[0].setAttribute("checked", "1");
			}
			else
			{
				treeItem.childNodes[0].childNodes[0].childNodes[0].removeAttribute("checked");
//				treeItem.childNodes[0].childNodes[0].childNodes[0].setAttribute("checked", "0");
			}
		}
	}

	dump("doCheckAll() done.\n");

	return(true);
}



function saveSearch()
{
	var resultsTree = parent.frames[1].document.getElementById("internetresultstree");
	if (!resultsTree)	return(false);
	var searchURL = resultsTree.getAttribute("ref");
	if ((!searchURL) || (searchURL == ""))		return(false);

	dump("Bookmark search URL: " + searchURL + "\n");

	var bmks = Components.classes["@mozilla.org/browser/bookmarks-service;1"].getService();
	if (bmks)	bmks = bmks.QueryInterface(Components.interfaces.nsIBookmarksService);

	var searchTitle = "Search: '" + gText + "' using " + gSites;
	if (bmks)	bmks.AddBookmark(searchURL, searchTitle, bmks.BOOKMARK_SEARCH_TYPE, null);

	return(true);
}
