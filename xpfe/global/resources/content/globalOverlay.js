function NavigatorWindow()
{
	var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
	if (!toolkitCore)
	{
		toolkitCore = new ToolkitCore();
		if (toolkitCore)
		{
			toolkitCore.Init("ToolkitCore");
		}
    }

    if (toolkitCore)
	{
      toolkitCore.ShowWindow("chrome://navigator/content/",
                             window);
    }
}


function MessengerWindow()
{
	window.open("chrome://messenger/content/", "messenger", "chrome");
}


function AddressBook() 
{
	var wind = window.open("chrome://addressbook/content/addressbook.xul",
						   "addressbook", "chrome");
	return wind;
}


