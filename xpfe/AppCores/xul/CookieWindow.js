var cookieCore;

function StartUp(windowName)
{
	dump("\nDoing " + windowName + " startup...\n");
	cookieCore = XPAppCoresManager.Find("CookieCore");  
	dump("Looking up CookieCore...\n");
	if (cookieCore == null)
	{
		dump("Creating CookieCore...\n");
		cookieCore = new CookieCore();
		if (cookieCore != null)
		{
			dump("CookieCore has been created.\n");
			cookieCore.Init("CookieCore");
		}
		else
		{
			dump("CookieCore was not created");
		}
	}
	else
	{
		dump("CookieCore has already been created! Hurrah!\n");
	}
	if (cookieCore != null && windowName != "Top" && windowName != "Bottom")
	{
		cookieCore.PanelLoaded(window);
	}
}

function DoGetCookieList()
{
	return cookieCore.GetCookieList();
}

function DoGetPermissionList()
{
	return cookieCore.GetPermissionList();
}

function DoSave(results)
{
	cookieCore.SaveCookie(results);
}

function DoCancel()
{
	cookieCore.CancelCookie();
}
