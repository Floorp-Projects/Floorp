
function doClick(node)
{
	htmlText = "";

	var theID = node.getAttribute("id");
	if (!theID)	return(false);
	dump("You single-clicked on '" + theID + "'.\n");

	try
	{
		var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
		if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
		if (rdf)
		{
			var internetSearchStore = rdf.GetDataSource("rdf:internetsearch");
			if (internetSearchStore)
			{
				var src = rdf.GetResource(theID, true);
				var htmlProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#HTML", true);
				var target = internetSearchStore.GetTarget(src, htmlProperty, true);
				if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
				if (target)	target = target.Value;
				if (target)
				{
					htmlText = target;

					var htmlArea = window.frames["content-frame"];
					if (htmlArea)
					{
						htmlArea.document.open("text/html", "replace");

						htmlArea.document.writeln("<HTML><BODY><TABLE><TR><TD>");
						htmlArea.document.writeln(target);
						htmlArea.document.writeln("</TD></TR></TABLE></BODY></HTML>");

						htmlArea.document.close();
					}
					else dump("no html area.\n");
				}
			}
		}
	}
	catch(ex)
	{
	}
	return(true);
}
