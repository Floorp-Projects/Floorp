
function loadPage(thePage)
{
	var	content="", results="";

	if (thePage == "find")
	{
		content="find.xul";
		results="findresults.xul";
	}
	else if (thePage == "internet")
	{
		content="internet.xul";
		results="internetresults.xul";
	}
	else if (thePage == "mail")
	{
		content="about:blank";
		results="about:blank";
	}
	else if (thePage == "addressbook")
	{
		content="about:blank";
		results="about:blank";
	}

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
	}
	return(true);
}
