// -*- Mode: Java -*-

function doSort(sortColName)
{
        var node = document.getElementById(sortColName);
	// determine column resource to sort on
	var sortResource = node.getAttribute('resource');
	if (!node)	return(false);

        var sortDirection="ascending";
        var isSortActive = node.getAttribute('sortActive');
        if (isSortActive == "true")
        {
                var currentDirection = node.getAttribute('sortDirection');
                if (currentDirection == "ascending")
                        sortDirection = "descending";
                else if (currentDirection == "descending")
                        sortDirection = "natural";
                else    sortDirection = "ascending";
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
        return(false);
}
