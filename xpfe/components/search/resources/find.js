
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
	var x = datasourceNode.selectedIndex;
	if (x < 0)		return(false);
	var datasource = datasourceNode.options[x].value;
	if (!datasource)	return(false);
	gDatasourceName = datasourceNode.options[x].text;
	dump("Datasource: " + gDatasourceName + "\n");

	// get match
	var matchNode = document.getElementById("match");
	if (!matchNode)		return(false);
	x = matchNode.selectedIndex;
	if (x < 0)		return(false);
	var match = matchNode.options[x].value;
	if (!match)		return(false);
	gMatchName = matchNode.options[x].text;
	dump("Match: " + gMatchName + "\n");

	// get method
	var methodNode = document.getElementById("method");
	if (!methodNode)	return(false);
	x = methodNode.selectedIndex;
	if (x < 0)		return(false);
	var method = methodNode.options[x].value;
	if (!method)		return(false);
	gMethodName = methodNode.options[x].text;
	dump("Method: " + method + "\n");

	// get user text to find
	var textNode = document.getElementById("findtext");
	if (!textNode)	return(false);
	var text = textNode.value;
	if (!text)	return(false);
	gTextName = text;
	dump("Find text: " + text + "\n");

	// construct find URL
	var url = "find:datasource=" + datasource;
	url += "&match=" + match;
	url += "&method=" + method;
	url += "&text=" + text;
	dump("Find URL: " + url + "\n");

	// load find URL into results pane
	var resultsTree = parent.frames[1].document.getElementById("findresultstree");
	if (!resultsTree)	return(false);
        resultsTree.setAttribute("ref", url);

	// enable "Save Search" button
	var searchButton = document.getElementById("SaveSearch");
	if (searchButton)
	{
		searchButton.removeAttribute("disabled", "false");
	}

	dump("doFind done.\n");
	return(true);
}



function saveSearch()
{
	var resultsTree = parent.frames[1].document.getElementById("findresultstree");
	if (!resultsTree)	return(false);
	var searchURL = resultsTree.getAttribute("ref");
	if ((!searchURL) || (searchURL == ""))		return(false);

	dump("Bookmark search URL: " + searchURL + "\n");
	var searchTitle = "Search: " + gMatchName + " " + gMethodName + " '" + gTextName + "' in " + gDatasourceName;
	dump("Title: " + searchTitle + "\n\n");

	var bmks = Components.classes["component://netscape/browser/bookmarks-service"].getService();
	if (bmks)	bmks = bmks.QueryInterface(Components.interfaces.nsIBookmarksService);
	if (bmks)	bmks.AddBookmark(searchURL, searchTitle);

	return(true);
}
