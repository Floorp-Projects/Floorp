
function OpenURL(event,node)
{
	var url = node.getAttribute('id');

	if (node.getAttribute('container') == "true")
	{
		return(false);
	}

	var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
	if (rdf)
	{
		var fileSys = rdf.GetDataSource("rdf:internetsearch");
		if (fileSys)
		{
			var src = rdf.GetResource(url, true);
			var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
			var target = fileSys.GetTarget(src, prop, true);
			if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
			if (target)	target = target.Value;
			if (target)	url = target;
			
		}
	}

	dump("OpenURL: double-clicked on '" + url + "'\n");

	// Ignore "NC:" urls.
	if (url.substring(0, 3) == "NC:")
	{
		return(false);
	}

	dump("Opening URL: " + url + "\n");
	window.open(url, "_blank");

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

	var isupports = Components.classes["component://netscape/rdf/xul-sort-service"].getService();
	if (!isupports)    return(false);
	var xulSortService = isupports.QueryInterface(Components.interfaces.nsIXULSortService);
	if (!xulSortService)    return(false);
	xulSortService.Sort(node, sortResource, sortDirection);

	return(true);
}
