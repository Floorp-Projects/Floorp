
var	gSearchStr = "";
var	gTabName = "";

function loadPage(thePage, searchStr)
{
	var	content="", results="";
	var	tabName="";

	gSearchStr = "";

	if (thePage == "find")
	{
		tabName="findTab";
		content="chrome://communicator/content/search/find.xul";
		results="chrome://communicator/content/search/findresults.xul";
	}
	else if (thePage == "internet")
	{
		tabName="internetTab";
		content="chrome://communicator/content/search/internet.xul";
		results="chrome://communicator/content/search/internetresults.xul";

		if ((searchStr) && (searchStr != null))
		{
			gSearchStr = searchStr;
		}
	}
	else if (thePage == "mail")
	{
		tabName="mailnewsTab";
		content="about:blank";
		results="about:blank";
	}
	else if (thePage == "addressbook")
	{
		tabName="addressbookTab";
		content="about:blank";
		results="about:blank";
	}

	if (tabName == gTabName)	return(true);
	gTabName = tabName;

	if ((content != "") && (results != ""))
	{
		var	contentFrame = document.getElementById("content");
		if (contentFrame)
		{
			contentFrame.setAttribute("src", content);
		}
		var	resultsFrame = document.getElementById("results");
		if (resultsFrame)
		{
			resultsFrame.setAttribute("src", results);
		}
		var	theTab = document.getElementById(tabName);
		if (theTab)
		{
			theTab.setAttribute("selected", "true");
		}
	}
	return(true);
}



function getSearchText()
{
	return(gSearchStr);
}



function doUnload()
{
    // Get the current window position/size.
    var x = window.screenX;
    var y = window.screenY;
    var h = window.outerHeight;
    var w = window.outerWidth;

    // Store these into the window attributes (for persistence).
    var win = document.getElementById( "search-window" );
    win.setAttribute( "x", x );
    win.setAttribute( "y", y );
    win.setAttribute( "height", h );
    win.setAttribute( "width", w );
}
