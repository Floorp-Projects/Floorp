
function doClick(node)
{
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
				var bannerProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#Banner", true);
				var htmlProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#HTML", true);

				var banner = internetSearchStore.GetTarget(src, bannerProperty, true);
				if (banner)	banner = banner.QueryInterface(Components.interfaces.nsIRDFLiteral);
				if (banner)	banner = banner.Value;

				var target = internetSearchStore.GetTarget(src, htmlProperty, true);
				if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
				if (target)	target = target.Value;
				if (target)
				{
					var text = "<BASE TARGET='_NEW'>";
					if (banner)
					{
						// add a </A> and a <BR> just in case
						text += banner + "</A><BR>";
					}
					text += target;
					
					var doc = window.frames[0].document;
					doc.open("text/html", "replace");
					doc.writeln(text);
					doc.close();
				}
			}
		}
	}
	catch(ex)
	{
	}
	return(true);
}
