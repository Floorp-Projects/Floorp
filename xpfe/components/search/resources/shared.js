
function OpenURL(event,node)
{
	if (node.getAttribute('container') == "true")
	{
		return(false);
	}
	url = node.getAttribute('id');

	// Ignore "NC:" urls.
	if (url.substring(0, 3) == "NC:")
	{
		return(false);
	}

	var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
	if (!toolkitCore)
	{
		toolkitCore = new ToolkitCore();
		if (toolkitCore)
		{
			toolkitCore.Init("ToolkitCore");
		}
	}
	if (toolkitCore)
	{
		toolkitCore.ShowWindow(url,window);
	}

	dump("OpenURL(" + url + ")\n");
	return true;
}

/* Note: doSort() does NOT support natural order sorting! */

function doSort(sortColName)
{
	var node = document.getElementById(sortColName);
	// determine column resource to sort on
	var sortResource = node.getAttribute('resource');
	if (!sortResource) return(false);

	var sortDirection="ascending";
	var isSortActive = node.getAttribute('sortActive');
	if (isSortActive == "true")
	{
		var currentDirection = node.getAttribute('sortDirection');
		if (currentDirection == "ascending")
			sortDirection = "descending";
		else	sortDirection = "ascending";
	}

	// get RDF Core service
	var rdfCore = XPAppCoresManager.Find("RDFCore");
	if (!rdfCore)
	{
		rdfCore = new RDFCore();
		if (!rdfCore)
		{
			return(false);
		}
		rdfCore.Init("RDFCore");
	}
	// sort!!!
	rdfCore.doSort(node, sortResource, sortDirection);
	return(true);
}
