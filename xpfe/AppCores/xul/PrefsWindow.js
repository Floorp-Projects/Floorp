var prefsCore;

function StartUp(windowName)
{
	dump("\nDoing " + windowName + " startup...\n");
	prefsCore = XPAppCoresManager.Find("PrefsCore");  
	dump("Looking up PrefsCore...\n");
	if (prefsCore == null)
	{
		dump("Creating PrefsCore...\n");
		prefsCore = new PrefsCore();
		if (prefsCore != null)
		{
			dump("PrefsCore has been created.\n");
			prefsCore.Init("PrefsCore");
		}
		else
		{
			dump("PrefsCore was not created");
		}
	}
	else
	{
		dump("PrefsCore has already been created! Hurrah!\n");
	}
	if (prefsCore != null && windowName != "Top" && windowName != "Bottom")
	{
		prefsCore.PanelLoaded(window);
	}
}

function DoSave()
{
	prefsCore.SavePrefs();
}

function DoCancel()
{
	prefsCore.CancelPrefs();
}
