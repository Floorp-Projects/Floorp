function NavigatorWindow()
{
	// FIX ME - Really need to find the front most navigator window
	//          and bring it all the way to the front
	
	// FIX ME - This code needs to be updated to use window.open()
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
	// FIX ME - Really need to find the front most messenger window
	//          and bring it all the way to the front
	
	window.open("chrome://messenger/content/", "messenger", "chrome");
}


function AddressBook() 
{
	var wind = window.open("chrome://addressbook/content/addressbook.xul",
						   "addressbook", "chrome");
	return wind;
}
