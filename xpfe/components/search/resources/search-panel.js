/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

var	rootNode = null;
var	textArc = null;
var	RDF_observer = new Object;
var   settingsButton = null;
var   settingsButtonText = null;
var   bunremoveAttributedle = null;
var   pref = null;

// get the click count pref
try {
  pref = Components.classes["component://netscape/preferences"].getService();
  if( pref )
    pref = pref.QueryInterface( Components.interfaces.nsIPref );
}
catch(e) {
}

RDF_observer =
{
	OnAssert   : function(src, prop, target)
		{
			if ((src == rootNode) && (prop == textArc))
			{
				rememberSearchText(target);
			}
		},
	OnUnassert : function(src, prop, target)
		{
		},
	OnChange   : function(src, prop, old_target, new_target)
		{
			if ((src == rootNode) && (prop == textArc))
			{
				rememberSearchText(new_target);
			}
		},
	OnMove     : function(old_src, new_src, prop, target)
		{
		}
}

function rememberSearchText(targetNode)
{
	if (targetNode)	targetNode = targetNode.QueryInterface(Components.interfaces.nsIRDFLiteral);
	if (targetNode)	targetNode = targetNode.Value;
	if (targetNode && (targetNode != ""))
	{
		var textNode = document.getElementById("sidebar-search-text");
		if (!textNode)	return(false);
		textNode.value = unescape(targetNode);
	}
}

// Initialize the Search panel: 
// 1) init the category list
// 2) load the search engines associated with this category
// 3) initialise the checked state of said engines. 
function SearchPanelStartup()
{
	settingsButton = document.getElementById("btn.Advanced");
	settingsButtonText = settingsButton.getAttribute("value");

  bundle = srGetStrBundle( "chrome://search/locale/search-panel.properties" );
   
	var tree = document.getElementById("Tree");
	if (tree)
	{
		var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
		if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
		if (rdf)
		{
			rootNode = rdf.GetResource("NC:LastSearchRoot", true);
			textArc = rdf.GetResource("http://home.netscape.com/NC-rdf#LastText", true);
			tree.database.AddObserver(RDF_observer);
		}
	}
	var categoryList = document.getElementById("categoryList");
	if (categoryList)
	{
		var internetSearch = Components.classes["component://netscape/rdf/datasource?name=internetsearch"].getService();
		if (internetSearch)	internetSearch = internetSearch.QueryInterface(Components.interfaces.nsIInternetSearchService);
		if (internetSearch)
		{
			var catDS = internetSearch.GetCategoryDataSource();
			if (catDS)	catDS = catDS.QueryInterface(Components.interfaces.nsIRDFDataSource);
			if (catDS)
			{
				categoryList.database.AddDataSource(catDS);
				var ref = categoryList.getAttribute("ref");
				if (ref)	categoryList.setAttribute("ref", ref);
			}
		}
		

	}
  
  try {
    var pref = Components.classes["component://netscape/preferences"].getService();
    if( pref )
      pref = pref.QueryInterface( Components.interfaces.nsIPref );
    var lastCategoryName = pref.CopyCharPref( "browser.search.last_search_category" );
  }
  catch( e ) {
    var lastCategoryName = "";
  }
  dump("*** lastCategoryName = " + lastCategoryName + "\n");
  var categoryList = document.getElementById( "categoryList" );
  if( categoryList ) {
  	 //set to default value 'the web'
	 //hack: hardcoded postion in here replace with a function that finds the entry 'the web'
	 
    for( var i = 0; i < categoryList.options.length; i++ )
    {
      if( ( lastCategoryName == "" && categoryList.options[i].value == "NC:SearchEngineRoot" ) ||
          ( categoryList.options[i].getAttribute("id") == lastCategoryName ) ) {
        categoryList.selectedIndex = i;
      }
    }
    
    // set the category name on the advanced panel
    var categoryText = categoryList.options[ categoryList.selectedIndex ].text;
    var textElement = document.getElementById( "categoryNameText" );
    textElement.setAttribute( "value", categoryText );
    //set the category name on the settings button
    settingsButton.value = settingsButtonText + categoryText + "...";
    

    if( lastCategoryName == "" )
      lastCategoryName = "NC:SearchEngineRoot";
    else
      lastCategoryName = "NC:SearchCategory?category=" + lastCategoryName;
  	var treeNode = document.getElementById("searchengines");
  	treeNode.setAttribute( "ref", lastCategoryName );
  }

  loadEngines( lastCategoryName );
}

function getNumEngines()
{
 	var treeNode = document.getElementById("searchengines");
  var numChildren = treeNode.childNodes.length;
	for (var x = 0; x<numChildren; x++)
 	{
  	if (treeNode.childNodes[x].tagName == "treechildren")
	  {
			var treeChildrenNode = treeNode.childNodes[x];
 			break;
  	}
	}
  if( !treeChildrenNode )
    return -1;
  return treeChildrenNode.childNodes.length;
}

function chooseCategory( aSelectElement )
{
  // set the category name on the advanced panel
  var categoryText = aSelectElement.options[ aSelectElement.selectedIndex ].text;
  var textElement = document.getElementById( "categoryNameText" );
  textElement.setAttribute( "value", categoryText );
  //set the category name on the settings button
    settingsButton.setAttribute("value",settingsButtonText + categoryText + "...");
    //alert("set button to " + settingsButtonText + categoryText);//debug remove

	var category = aSelectElement.options[ aSelectElement.selectedIndex ].getAttribute("id");
  var pref = Components.classes["component://netscape/preferences"].getService();
  if( pref )
    pref = pref.QueryInterface( Components.interfaces.nsIPref );
  if( !category )
    category = "";
  pref.SetCharPref( "browser.search.last_search_category", category );
	if ( category )	
    category = "NC:SearchCategory?category=" + category;
	else		
    category = "NC:SearchEngineRoot";
	var treeNode = document.getElementById("searchengines");
	if (!treeNode)	
    return false;
	treeNode.setAttribute( "ref", category );
  loadEngines( category );
	return true;
}

function saveEngines( aSelectElement )
{
  var category = aSelectElement.options[ aSelectElement.selectedIndex ].getAttribute("id");
  if( category )
    category = "NC:SearchCategory?category=" + category;
  else
    category = "NC:SearchEngineRoot";
	var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
	if (rdf)
	{
		var localStore = rdf.GetDataSource("rdf:local-store");
    if( !localStore )
      return false;
   	var treeNode = document.getElementById("searchengines");
    var numChildren = treeNode.childNodes.length;
  	for (var x = 0; x<numChildren; x++)
   	{
    	if (treeNode.childNodes[x].tagName == "treechildren")
  	  {
  			var treeChildrenNode = treeNode.childNodes[x];
   			break;
    	}
 		}
    if( !treeChildrenNode )
      return false;
    var checkedProperty = rdf.GetResource( "http://home.netscape.com/NC-rdf#checked", true );
    var categorySRC = rdf.GetResource( category, true );
    var checkedFlag = false;
    for (var x = 0; x < treeChildrenNode.childNodes.length; x++)
    {
      var treeItem = treeChildrenNode.childNodes[x];
      if( !treeItem )
      	continue;
      checkedFlag = false;
      var checkbox = treeItem.firstChild.firstChild.firstChild;
      if( checkbox.checked || checkbox.getAttribute("checked") == "1" ) 
        checkedFlag = true;
      var engineURI = treeItem.getAttribute("id");
      var engineSRC = rdf.GetResource(engineURI, true);
      if( checkedFlag )
        localStore.Assert( categorySRC, checkedProperty, engineSRC, true );
      else
        localStore.Unassert( categorySRC, checkedProperty, engineSRC, true );
    }
    flushableStore = localStore.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    if (flushableStore)
      flushableStore.Flush();
  }
}

// initialise the appropriate engine list, and the checked state of the engines
function loadEngines( aCategory )
{
  var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
  if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
  if (rdf)
  {
  	var localStore = rdf.GetDataSource("rdf:local-store");
  	if (localStore)
  	{
  		var treeNode = document.getElementById("searchengines");
  		var numChildren = treeNode.childNodes.length;
  		for (var x = 0; x<numChildren; x++)
  		{
  			if (treeNode.childNodes[x].tagName == "treechildren")
  			{
  				var treeChildrenNode = treeNode.childNodes[x];
  				break;
  			}
  		}
  		if( treeChildrenNode )
  		{
  			var numEngines = treeChildrenNode.childNodes.length;
  			var checkedProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#checked", true);
        var categorySRC = rdf.GetResource( aCategory, true );
  			for (var x = 0; x<numEngines; x++)
  			{
  				var treeItem = treeChildrenNode.childNodes[x];
  				if (!treeItem)	continue;
  				var engineURI = treeItem.getAttribute("id");
  				var engineSRC = rdf.GetResource( engineURI, true );
  				var hasAssertion = localStore.HasAssertion( categorySRC, checkedProperty, engineSRC, true );
          var checkbox = treeItem.firstChild.firstChild.firstChild;
          dump("*** hasAssertion = " + hasAssertion + "\n");
  				if ( hasAssertion )
  				{
  					checkbox.checked = true;
  					checkbox.setAttribute("checked", "1");
  				}
  			}
  		}
  	}
  }
}


function SearchPanelShutdown()
{

}

function doStop()
{
	var isupports = Components.classes["component://netscape/rdf/datasource?name=internetsearch"].getService();
	if (!isupports) 
    return false;
	var internetSearchService = isupports.QueryInterface(Components.interfaces.nsIInternetSearchService);
	if (internetSearchService)
		internetSearchService.Stop();

  // should stop button press also stop the load of the page in the browser? I think so. 
	var progressNode = parent.document.getElementById("statusbar-icon");
	var stopButtonNode = document.getElementById("stopbutton");
	var searchButtonNode = document.getElementById("searchbutton");
  if( !progressNode || !stopButtonNode || !searchButtonNode ) 
    return; 
  progressNode.setAttribute("mode", "normal");
  stopButtonNode.setAttribute("style", "display: none;");
	searchButtonNode.setAttribute("style", "display: inherit;");

	// get various services
	var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
	if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);

	var internetSearch = Components.classes["component://netscape/rdf/datasource?name=internetsearch"].getService();
	if (internetSearch)	internetSearch = internetSearch.QueryInterface(Components.interfaces.nsIRDFDataSource);

	var sortSetFlag = false;

	// show appropriate column(s)
	if ((rdf) && (internetSearch))
	{
		var resultsTree = top.content.document.getElementById("internetresultstree");
		if( !resultsTree )
      return false;
		var searchURL = resultsTree.getAttribute("ref");
		if( !searchURL )	
      return false;

		var searchResource       = rdf.GetResource(searchURL, true);
		var priceProperty        = rdf.GetResource("http://home.netscape.com/NC-rdf#Price", true);
		var availabilityProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#Availability", true);
		var relevanceProperty    = rdf.GetResource("http://home.netscape.com/NC-rdf#Relevance", true);
		var trueProperty         = rdf.GetLiteral("true");

		var hasPriceFlag         = internetSearch.HasAssertion(searchResource, priceProperty, trueProperty, true);
		var hasAvailabilityFlag  = internetSearch.HasAssertion(searchResource, availabilityProperty, trueProperty, true);
		var hasRelevanceFlag     = internetSearch.HasAssertion(searchResource, relevanceProperty, trueProperty, true);

		if(hasPriceFlag == true)
		{
			var colNode = top.content.document.getElementById("PriceColumn");
			if (colNode)
			{
				colNode.removeAttribute("style", "width: 0; visibility: collapse;");
				if (sortSetFlag == false)
				{
					top.content.setInitialSort(colNode, "ascending");
					sortSetFlag = true;
				}
			}
		}
		if (hasAvailabilityFlag == true)
		{
			colNode = top.content.document.getElementById("AvailabilityColumn");
			if (colNode)
				colNode.removeAttribute("style", "width: 0; visibility: collapse;");
		}
		if (hasRelevanceFlag == true)
		{
			colNode = top.content.document.getElementById("RelevanceColumn");
			if (colNode)
			{
				colNode.removeAttribute("style", "width: 0; visibility: collapse;");
				if (sortSetFlag == false)
				{
					top.content.setInitialSort(colNode, "descending");
					sortSetFlag = true;
				}
			}
		}
	}

	if (sortSetFlag == false)
	{
		colNode = top.content.document.getElementById("NameColumn");
		if (colNode)
			top.content.setInitialSort(colNode, "ascending");
	}
}

function doSearch()
{
	//get click count pref for later
	//and set tree attribute to cause proper results appearance (like links) to happen 
	//when user set pref to single click
  try {
    if( pref ) {
      var prefvalue = pref.GetBoolPref( "browser.search.use_double_clicks" );
      var mClickCount = prefvalue ? 2 : 1;
    } 
    else
      mClickCount = 1;
  }
  catch(e) {
    mClickCount = 1;
  }
  var tree = document.getElementById("Tree");
  if (mClickCount == 1)
    tree.setAttribute("singleclick","true");
  else
    tree.removeAttribute("singleclick");

	//end insert for single click appearance
	
	// hide search button
	var searchButtonNode = document.getElementById("searchbutton");
	var stopButtonNode = document.getElementById("stopbutton");
	var progressNode = top.document.getElementById("statusbar-icon");
	if( !stopButtonNode || !searchButtonNode  )
    return;
  
	// hide various columns
  if( parent.content.isMozillaSearchWindow ) {
  	colNode = parent.content.document.getElementById("RelevanceColumn");
  	if (colNode)	colNode.setAttribute("style", "width: 0; visibility: collapse;");
	  colNode = parent.content.document.getElementById("PriceColumn");
  	if (colNode)	colNode.setAttribute("style", "width: 0; visibility: collapse;");
	  colNode = parent.content.document.getElementById("AvailabilityColumn");
  	if (colNode)	colNode.setAttribute("style", "width: 0; visibility: collapse;");
  }

	// get user text to find
	var textNode = document.getElementById("sidebar-search-text");
	if( !textNode )	
    return false;
	if ( !textNode.value )
	{
    alert( bundle.GetStringFromName("enterstringandlocation") );
		return false;
	}
	// get selected search engines
	var treeNode = document.getElementById("searchengines");
	if (!treeNode)	return(false);
	var numChildren = treeNode.childNodes.length;
	for (var x = 0; x<numChildren; x++)
	{
		if (treeNode.childNodes[x].tagName == "treechildren")
		{
			var treeChildrenNode = treeNode.childNodes[x];
			break;
		}
	}
	if ( !treeChildrenNode )	
    return false;

	var searchURL   = "";
	var foundEngine = false;
  var engineURIs  = [];
	var numEngines  = treeChildrenNode.childNodes.length;

	for (var x = 0; x<numEngines; x++)
	{
		var treeItem = treeChildrenNode.childNodes[x];
		if (!treeItem)
			continue;
		var checkedFlag = false;
    var checkbox = treeItem.firstChild.firstChild.firstChild;
		if ( checkbox.checked == true || checkbox.getAttribute("checked") == "1")
			checkedFlag = true;
		if ( checkedFlag )
		{
			var engineURI = treeItem.getAttribute("id");
			if (!engineURI)	continue;
      engineURIs[engineURIs.length] = engineURI;
			foundEngine = true;
		}
	}
	if (foundEngine == false)
	{
    if( getNumEngines() == 1 ) {
      // only one engine in this category, check it
      var treeItem = treeChildrenNode.firstChild;
      engineURIs[engineURIs.length] = treeItem.getAttribute( "id" );
    }
    else {
      dump("*** multiple search engines present, selecting the netscape search engine\n");
      for( var i = 0; i < treeChildrenNode.childNodes.length; i++ )
      {
        var currItem = treeChildrenNode.childNodes[i];
        dump("*** the current URI is = " + currItem.getAttribute("id") + "\n");
        if( currItem.getAttribute("id").indexOf("NetscapeSearch.src") != -1 ) {
          engineURIs[engineURIs.length] = currItem.getAttribute("id");
          break;
        }
      }
    }
	}

  progressNode.setAttribute( "mode", "undetermined" );
  searchButtonNode.setAttribute("style", "display: none;");
  stopButtonNode.removeAttribute("style", "display: none;");
    
  // run the search
	OpenSearch("internet", false, textNode.value, engineURIs );
	return true;
}

// check an engine representation in the engine list
function doCheck( aNode )
{
	if( aNode.checked )
		aNode.setAttribute( "checked", "1" );
	else
		aNode.removeAttribute( "checked" );
  saveEngines( document.getElementById("categoryList") )
	return false;
}

function checkSearchProgress( aSearchURL )
{
	var	activeSearchFlag = false;
	var resultsTree = top.content.document.getElementById("internetresultstree");
  var enginesBox = top.content.document.getElementById("engineTabs");
	if( !resultsTree || !enginesBox )	
    return false;
	var treeref = resultsTree.getAttribute("ref");
  if( !treeref ) {
    resultsTree.setAttribute( "ref", aSearchURL );
    treeref = aSearchURL;
  }

	var ds = resultsTree.database;
	if ( treeref && ds )
	{
		var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
		if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
		if (rdf)
		{
			var source = rdf.GetResource( treeref, true);
			var loadingProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#loading", true);
			var target = ds.GetTarget(source, loadingProperty, true);
			if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
			if (target)	target = target.Value;
			if (target == "true")
				activeSearchFlag = true;
		}
	}
	if( activeSearchFlag )
		setTimeout("checkSearchProgress()", 1000);
	else {
		//window.frames["sidebar-content"].doStop();
    doStop();
  }
  
  return true;
}

function FOO_doSearch()
{
	var textNode = document.getElementById("sidebar-search-text");
	if (!textNode)	return(false);
	var text = textNode.value;
	top.OpenSearch("internet", false, text);
	return(true);
}

function openURL(event, treeitem, root)
{
  try {
    if( pref ) {
      var prefvalue = pref.GetBoolPref( "browser.search.use_double_clicks" );
      mClickCount = prefvalue ? 2 : 1;
    } 
    else
      mClickCount = 1;
  }
  catch(e) {
    mClickCount = 1;
  } 
  
  if ((event.button != 1) || (event.clickCount != mClickCount))
		return(false);

	if (treeitem.getAttribute("container") == "true")
		return(false);

	if (treeitem.getAttribute("type") == "http://home.netscape.com/NC-rdf#BookmarkSeparator")
		return(false);

	var id = treeitem.getAttribute('id');
	if (!id)
		return(false);

	// rjc: add support for anonymous resources; if the node has
	// a "#URL" property, use it, otherwise default to using the id
	try
	{
		var rootNode = document.getElementById(root);
		var ds = null;
		if (rootNode)
		{
			ds = rootNode.database;
		}
		var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
		if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
		if (rdf)
		{
			if (ds)
			{
				var src = rdf.GetResource(id, true);
				var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
				var target = ds.GetTarget(src, prop, true);
				if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
				if (target)	target = target.Value;
				if (target)	id = target;
			}
		}
	}
	catch(ex)
	{
	}

	window.content.location = id;
}

// this should go somewhere else. 
function OpenSearch( tabName, forceDialogFlag, aSearchStr, engineURIs )
{
	var searchMode = 0;
	var searchEngineURI = null;
	var autoOpenSearchPanel = false;
	var defaultSearchURL = null;

	try
	{
    if( pref ) {
  		searchMode = pref.GetIntPref("browser.search.powermode");
	  	autoOpenSearchPanel = pref.GetBoolPref("browser.search.opensidebarsearchpanel");
      if( !engineURIs )
  		  searchEngineURI = pref.CopyCharPref("browser.search.defaultengine");
  		defaultSearchURL = pref.CopyCharPref("browser.search.defaulturl");
    }
	}
	catch(ex)
	{
	}
	
	if ( !defaultSearchURL )
		defaultSearchURL = "http://info.netscape.com/fwd/sidb1dnet/http://search.netscape.com/cgi-bin/search?search=";
//		defaultSearchURL = "http://search.netscape.com/cgi-bin/search?search=";

	if( searchMode == 1 || forceDialogFlag )
	{
		// Use a single search dialog
    var windowManager = getService( "component://netscape/rdf/datasource?name=window-mediator", "nsIWindowMediator" );
		var searchWindow = windowManager.getMostRecentWindow("search:window");
		if (searchWindow)
		{
			searchWindow.focus();
			if( searchWindow.loadPage )	
        searchWindow.loadPage( tabName, searchStr );
		}
		else
			top.openDialog("chrome://search/content/search.xul", "SearchWindow", "dialog=no,close,chrome,resizable", tabName, searchStr);
	}
	else
	{
    var searchDS = Components.classes["component://netscape/rdf/datasource?name=internetsearch"].getService();
    if( searchDS )
      searchDS = searchDS.QueryInterface( Components.interfaces.nsIInternetSearchService );
  	if( searchDS )
		{
  	  if( !aSearchStr ) 
        return false;
    	var	escapedSearchStr = escape( aSearchStr );
			searchDS.RememberLastSearchText( escapedSearchStr );
  		try
  		{
        if( !engineURIs || ( engineURIs && engineURIs.length <= 1 ) ) {
          // not called from sidebar or only one engine selected
          if( engineURIs )
            searchEngineURI = engineURIs[0];
          // look up the correct search URL format for the given engine
          var	searchURL = searchDS.GetInternetSearchURL( searchEngineURI, escapedSearchStr );
          if( searchURL )
            defaultSearchURL = searchURL;
          else 
            defaultSearchURL = defaultSearchURL + escapedSearchStr;
          // load the results page of selected or default engine in the content area
          if( defaultSearchURL )
            top.content.location.href = defaultSearchURL;
        }
        else {
          // multiple providers
          searchURL = "";
          for( var i = 0; i < engineURIs.length; i++ ) 
          {
            if( searchURL == "" )
              searchURL = "internetsearch:";
            else
              searchURL += "&";
            searchURL += "engine=" + engineURIs[i];
          }
          searchURL += ( "&text=" + escapedSearchStr );
          top.content.location.href = "chrome://search/content/internetresults.xul";
        	setTimeout("checkSearchProgress('" + searchURL + "')", 1000);
        }
  		}
  		catch(ex)
  		{
  		}
    }
	}
}

function switchTab( aPageIndex )
{
  var deck = document.getElementById( "advancedDeck" );
  deck.setAttribute( "index", aPageIndex );
}
