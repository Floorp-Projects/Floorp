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

// lifted from bookmarks.js
var htmlInput = null;
var saveNode = null;
var newValue = "";
var timerID = null;
var gEditNode = null;
function OpenURL(event, node)
{
    // clear any single-click/edit timeouts
    if (timerID != null)
    {
        gEditNode = null;
        clearTimeout(timerID);
        timerID = null;
    }

    if (node.getAttribute('container') == "true")
    {
        return(false);
    }

    var url = node.getAttribute('id');

    // Ignore "NC:" urls.
    if (url.substring(0, 3) == "NC:")
    {
        return(false);
    }

	try
	{
		// add support for IE favorites under Win32, and NetPositive URLs under BeOS
		if (url.indexOf("file://") == 0)
		{
			var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
			if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
			if (rdf)
			{
				var fileSys = rdf.GetDataSource("rdf:files");
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
		}
	}
	catch(ex)
	{
	}

    window.open(url,'history');

    return(true);
}
