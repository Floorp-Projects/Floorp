
var gEditorShell = null;

function doLoad()
{
	dump("doLoad() entered.\n");

	var editorShell = Components.classes["component://netscape/editor/editorshell"].createInstance();
	if (editorShell)	editorShell = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
	if (editorShell)
	{
		window.editorShell = editorShell;

		editorShell.Init();
		dump("init\n");
//		editorShell.SetWebShellWindow(window);
//		dump("SetWebShellWindow\n");
//		editorShell.SetToolbarWindow(window)
//		dump("SetToolbarWindow\n");
		editorShell.SetEditorType("html");
		dump("SetEditorType\n");
		editorShell.SetContentWindow(window.content);

		// Get url for editor content and load it.
		// the editor gets instantiated by the editor shell when the URL has finished loading.
		editorShell.LoadUrl("about:blank");
		
		gEditorShell = editorShell;
		dump("doLoad() succeeded.\n");
	}
	else dump("doLoad(): problem loading component://netscape/editor/editorshell \n");
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
				var htmlProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#HTML", true);
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
						var text = target;

						gEditorShell.SelectAll();
						gEditorShell.DeleteSelection(2);
//						gEditorShell.SetTextProperty("font", "size", "-2");
						gEditorShell.InsertSource(text);
//						gEditorShell.SelectAll();
//						gEditorShell.SetTextProperty("font", "size", "-2");
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
