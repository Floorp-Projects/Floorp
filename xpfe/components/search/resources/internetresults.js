function searchResultsOpenURL(event)
{
  var node = event.target.parentNode.parentNode;
	if (node.localName != "treeitem" || node.getAttribute('container') == "true")
		return false;

	var url = node.id;
	var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
	if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
	if (rdf)
	{
		var ds = rdf.GetDataSource("rdf:internetsearch");
		if (ds)
		{
			var src = rdf.GetResource(url, true);
			var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
			var target = ds.GetTarget(src, prop, true);
			if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
			if (target)	target = target.Value;
			if (target)	url = target;
		}
	}

	// Ignore "NC:" urls.
	if (url.substring(0, 3) == "NC:")
		return false;

  if (top.appCore) 
    top.appCore.loadUrl(url); // arghghghgh
  else
    top._content.location.href = url;

	return true;
}



function onLoadInternetResults()
{
  // HACK HACK HACK HACK HACK HACK - chrome urls are being stored in sh as file urls!
  // access to xpconnect is denied! ack! erk! ork!
  try {
    var pref = Components.classes["@mozilla.org/preferences;1"].getService();
    if (pref) pref = pref.QueryInterface(Components.interfaces.nsIPref);
  } 
  catch(e) {
    // ok, we're toast. user clicked back, loaded as file url, no xpconnect. abort, 
    // reload.
    const searchResults = "chrome://communicator/content/search/internetresults.xul"
    if (top.appCore) top.appCore.loadUrl(searchResults);
    else top._content.location.href = searchResults;
    return; // ABORT. 
  }

  // clear any previous results on load
	var iSearch = Components.classes["@mozilla.org/rdf/datasource;1?name=internetsearch"].getService();
	if (iSearch) iSearch = iSearch.QueryInterface(Components.interfaces.nsIInternetSearchService);
	if (iSearch) iSearch.ClearResultSearchSites();

  // the search URI is passed in as a parameter, so get it and them root the results tree
  var searchURI = top._content.location.href;
  if (searchURI) {
    const lastSearchURIPref = "browser.search.lastMultipleSearchURI";
    var offset = searchURI.indexOf("?");
    if (offset > 0) {
      nsPreferences.setUnicharPref(lastSearchURIPref, searchURI); // evil
      searchURI = searchURI.substr(offset+1);
      loadResultsTree(searchURI);
    }
    else {  
      searchURI = nsPreferences.copyUnicharPref(lastSearchURIPref, "");
      offset = searchURI.indexOf("?");
      if (offset > 0) {
        nsPreferences.setUnicharPref(lastSearchURIPref, searchURI); // evil
        searchURI = searchURI.substr(offset+1);
        loadResultsTree(searchURI);
      }
    }
  }
  return true;
}

function loadResultsTree( aSearchURL )
{
  var resultsTree = document.getElementById( "internetresultstree" );
  if (!resultsTree) return false;
  resultsTree.setAttribute("ref", aSearchURL);
  return true;
}



function doEngineClick( event, aNode )
{
  // do toggling
  var engineTabBox = document.getElementById("engineTabs");
  var toggledEls = engineTabBox.getElementsByAttribute("toggled", "true");
  for (var i = 0; i < toggledEls.length; i++)
    toggledEls[i].removeAttribute("toggled");
  event.target.setAttribute("toggled", "true");

	var html = null;

	var resultsTree = document.getElementById("internetresultstree");
	var contentArea = document.getElementById("content");
	var splitter = document.getElementById("results-splitter");
	var engineURI = aNode.id;
	if (engineURI == "allEngines") {
		resultsTree.removeAttribute("hidden");
		splitter.removeAttribute("hidden");
	}
	else
	{
		resultsTree.setAttribute("hidden", "true");
		splitter.setAttribute("hidden", "true");
		try
		{
			var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
			if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
			if (rdf)
			{
				var internetSearchStore = rdf.GetDataSource("rdf:internetsearch");
				if (internetSearchStore)
				{
					var src = rdf.GetResource(engineURI, true);
					var htmlProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#HTML", true);
					html = internetSearchStore.GetTarget(src, htmlProperty, true);
					if ( html )	html = html.QueryInterface(Components.interfaces.nsIRDFLiteral);
					if ( html )	html = html.Value;
				}
			}
		}
		catch(ex)
		{
		}
	}

	if ( html )
	{
		var doc = frames[0].document;
		if (doc)
		{
    		doc.open("text/html", "replace");
	    	doc.writeln( html );
		    doc.close();
		}
	}
	else
		frames[0].document.location = "chrome://communicator/locale/search/default.htm";
}



function doResultClick(node)
{
	var theID = node.id;
	if (!theID)	return(false);

	try
	{
		var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
		if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
		if (rdf)
		{
			var internetSearchStore = rdf.GetDataSource("rdf:internetsearch");
			if (internetSearchStore)
			{
				var src = rdf.GetResource(theID, true);
				var urlProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
				var bannerProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#Banner", true);
				var htmlProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#HTML", true);

				var url = internetSearchStore.GetTarget(src, urlProperty, true);
				if (url)	url = url.QueryInterface(Components.interfaces.nsIRDFLiteral);
				if (url)	url = url.Value;
				if (url)
				{
					var statusNode = document.getElementById("status-button");
					if (statusNode)
					{
						statusNode.setAttribute("value", url);
					}
				}

				var banner = internetSearchStore.GetTarget(src, bannerProperty, true);
				if (banner)	banner = banner.QueryInterface(Components.interfaces.nsIRDFLiteral);
				if (banner)	banner = banner.Value;

				var target = internetSearchStore.GetTarget(src, htmlProperty, true);
				if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
				if (target)	target = target.Value;
				if (target)
				{
					var text = "<HTML><HEAD><TITLE>Search</TITLE><BASE TARGET='_top'></HEAD><BODY><FONT POINT-SIZE='9'>";

					if (banner)
						text += banner + "</A><BR>"; // add a </A> and a <BR> just in case
					text += target;
					text += "</FONT></BODY></HTML>"
					
					var doc = frames[0].document;
					doc.open("text/html", "replace");
					doc.writeln(text);
					doc.close();
				}
			}
		}
	}
	catch(ex)
	{
	}
	return(true);
}

function treeSelect(event)
{
  if (!event.target.selectedItems || 
      event.target.selectedItems && event.target.selectedItems.length != 1) 
    return false;
  doResultClick(event.target.selectedItems[0]);
}

function treeClick(event)
{
  if (event.detail == 2 && event.which == 1) 
    searchResultsOpenURL(event);
  else
    treeSelect(event);
}
