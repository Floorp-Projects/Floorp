var signonCore;

function StartUp(windowName)
{
	dump("\nDoing " + windowName + " startup...\n");
	signonCore = XPAppCoresManager.Find("SignonCore");  
	dump("Looking up SignonCore...\n");
	if (signonCore == null)
	{
		dump("Creating SignonCore...\n");
		signonCore = new SignonCore();
		if (signonCore != null)
		{
			dump("SignonCore has been created.\n");
			signonCore.Init("SignonCore");
		}
		else
		{
			dump("SignonCore was not created");
		}
	}
	else
	{
		dump("SignonCore has already been created! Hurrah!\n");
	}
	if (signonCore != null && windowName != "Top" && windowName != "Bottom")
	{
		signonCore.PanelLoaded(window);
	}
}

function DoSave(results)
{
	signonCore.SaveSignon(results);
}

function DoCancel()
{
	signonCore.CancelSignon();
}
