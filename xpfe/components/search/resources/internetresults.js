
function doClick(node)
{
	htmlText = "";

	var theID = node.getAttribute("id");
	if (!theID)	return(false);

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

					var htmlArea = window.content;
					if (htmlArea)
					{
						htmlArea.open();

//						htmlArea.write("<HTML><BODY><TABLE><TR><TD>");
//						htmlArea.write(target);
//						htmlArea.write("</TD></TR></TABLE></BODY></HTML>\n");

						htmlArea.close();
						
						dump("HTML\n----------\n" + target + "\n----------\n");
					}
				}
			}
		}
	}
	catch(ex)
	{
	}
	return(true);
}
