var prefwindow = null;

function StartUp(windowName)
{
	dump("\nDoing " + windowName + " startup...\n");
	dump("Looking up prefwindow object...\n");
	if (prefwindow == null)
	{
		dump("Creating prefwindow object...\n");
    	prefwindow = Components.classes['component://netscape/prefwindow'].createInstance(Components.interfaces.nsIPrefWindow);
	}
	else
	{
		dump("prefwindow has already been created! Hurrah!\n");
	}
	if (prefwindow != null && windowName != "Top" && windowName != "Bottom")
	{
		prefwindow.PanelLoaded(window);
	}
}

function DoSave()
{
	prefwindow.SavePrefs();
}

function DoCancel()
{
	prefwindow.CancelPrefs();
}

