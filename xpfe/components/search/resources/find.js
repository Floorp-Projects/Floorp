
function doFind()
{
	// get RDF datasource to query
	var datasourceNode = document.getElementById("datasource");
	if (!datasourceNode)	return(false);
	var x = datasourceNode.selectedIndex;
	if (x < 0)		return(false);
	var datasource = datasourceNode.options[x].value;
	if (!datasource)	return(false);
	dump("Datasource: " + datasource + "\n");

	// get match
	var matchNode = document.getElementById("match");
	if (!matchNode)		return(false);
	x = matchNode.selectedIndex;
	if (x < 0)		return(false);
	var match = matchNode.options[x].value;
	if (!match)		return(false);
	dump("Match: " + match + "\n");

	// get method
	var methodNode = document.getElementById("method");
	if (!methodNode)	return(false);
	x = methodNode.selectedIndex;
	if (x < 0)		return(false);
	var method = methodNode.options[x].value;
	if (!method)		return(false);
	dump("Method: " + method + "\n");

	// get user text to find
	var textNode = document.getElementById("findtext");
	if (!textNode)	return(false);
	var text = textNode.value;
	if (!text)	return(false);
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
	dump("doFind done.\n");

	return(true);
}
