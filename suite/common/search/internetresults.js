
var gEditorShell = null;

function doLoad()
{
	var editorShell = Components.classes["component://netscape/editor/editorshell"].createInstance();
	if (editorShell)	editorShell = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
	if (editorShell)
	{
		window.editorShell = editorShell;

		editorShell.Init();

//		editorShell.SetWebShellWindow(window);
//		dump("SetWebShellWindow\n");
//		editorShell.SetToolbarWindow(window)
//		dump("SetToolbarWindow\n");

		editorShell.SetEditorType("html");
		editorShell.SetContentWindow(window.content);

		// The editor gets instantiated by the editor shell when the URL has finished loading.
		editorShell.LoadUrl("about:blank");
		
		gEditorShell = editorShell;
	}
}

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
					if (gEditorShell == null)
					{
						doLoad();
					}
					if (gEditorShell)
					{
						var text = "";
						if (banner)
						{
							// add a </A> and a <BR> just in case
							text += banner + "</A><BR>";
						}
						text += target;

						gEditorShell.SelectAll();
						gEditorShell.DeleteSelection(2);
						gEditorShell.InsertSource(text);
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
