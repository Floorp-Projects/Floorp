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
 * Original Author(s):
 *    Robert John Churchill   <rjc@netscape.com>
 *
 * Contributor(s): 
 *    Ben Goodger             <ben@netscape.com>
 *    Rob Ginda               <rginda@netscape.com>
 *    Steve Lamm              <slamm@netscape.com>
 */

const WMEDIATOR_PROGID  = "component://netscape/rdf/datasource?name=window-mediator";
const ISEARCH_PROGID    = "component://netscape/rdf/datasource?name=internetsearch";
const RDFSERVICE_PROGID = "component://netscape/rdf/rdf-service";
const BMARKS_PROGID     = "component://netscape/browser/bookmarks-service";

const nsIBookmarksService      = Components.interfaces.nsIBookmarksService;
const nsIWindowMediator        = Components.interfaces.nsIWindowMediator;
const nsIRDFService            = Components.interfaces.nsIRDFService;
const nsIRDFLiteral            = Components.interfaces.nsIRDFLiteral;
const nsIRDFDataSource         = Components.interfaces.nsIRDFDataSource;
const nsIRDFRemoteDataSource   = Components.interfaces.nsIRDFRemoteDataSource;
const nsIInternetSearchService = Components.interfaces.nsIInternetSearchService;
const nsIPref                  = Components.interfaces.nsIPref;

const STRINGBUNDLE_URL =
    "chrome://communicator/locale/search/search-panel.properties";

var	rootNode = null;
var	textArc = null;
var	modeArc = null;
var	RDF_observer = new Object;
var	pref = null;
var bundle = null;


function debug(msg)
{
	// uncomment for debugging information
	// dump(msg+"\n");
}



// get the click count pref
try
{
	pref = Components.classes["component://netscape/preferences"];
	if (pref)	pref = pref.getService(nsIPref);
}
catch(e)
{
	debug("Exception " + e + " trying to get prefs.\n");
}



RDF_observer =
{
	onAssert   : function(ds, src, prop, target)
		{
			if ((src == rootNode) && (prop == textArc))
			{
				rememberSearchText(target);
			}
			else if ((src == rootNode) && (prop == modeArc))
			{
				updateSearchMode();
			}
		},
	onUnassert : function(ds, src, prop, target)
		{
		},
	onChange   : function(ds, src, prop, old_target, new_target)
		{
			if ((src == rootNode) && (prop == textArc))
			{
				rememberSearchText(new_target);
			}
			else if ((src == rootNode) && (prop == modeArc))
			{
				updateSearchMode();
			}
		},
	onMove     : function(ds, old_src, new_src, prop, target)
		{
		},
	beginUpdateBatch : function(ds)
		{
		},
	endUpdateBatch   : function(ds)
		{
		}
}



function rememberSearchText(target)
{
	if (target)	target = target.QueryInterface(nsIRDFLiteral);
	if (target)	target = target.Value;
	if (target && (target != ""))
	{
		var textNode = document.getElementById("sidebar-search-text");
		if (!textNode)	return(false);

		// convert pluses (+) back to spaces
		target = target.replace(/+/i, " ");

		textNode.value = unescape(target);

        doEnabling();
	}
	// show the results tab
	switchTab(0);
}



function updateSearchMode()
{
	var searchMode = 0;
	try
	{
		if (pref)	searchMode = pref.GetIntPref("browser.search.mode");

		var categoryBox = document.getElementById("categoryBox");
		if (categoryBox)
		{
			if (searchMode == 0)
			{
				categoryBox.setAttribute("collapsed", "true");
				switchTab(0);
			}
			else
			{
				categoryBox.removeAttribute("collapsed");
				switchTab(1);
			}
		}
	}
	catch(ex)
	{
	}
	return(searchMode);
}



// Initialize the Search panel: 
// 1) init the category list
// 2) load the search engines associated with this category
// 3) initialise the checked state of said engines. 
function SearchPanelStartup()
{
	bundle = srGetStrBundle( STRINGBUNDLE_URL );

	var tree = document.getElementById("Tree");
	if (tree)
	{
        var rdf = Components.classes[RDFSERVICE_PROGID].getService(nsIRDFService);
		if (rdf)
		{
			rootNode = rdf.GetResource("NC:LastSearchRoot", true);
			textArc = rdf.GetResource("http://home.netscape.com/NC-rdf#LastText", true);
			modeArc = rdf.GetResource("http://home.netscape.com/NC-rdf#SearchMode", true);
			tree.database.AddObserver(RDF_observer);
		}
	}

    var internetSearch = Components.classes[ISEARCH_PROGID].getService(nsIInternetSearchService);
	if (internetSearch)
	{
		var catDS = internetSearch.GetCategoryDataSource();
		if (catDS)	catDS = catDS.QueryInterface(nsIRDFDataSource);
		if (catDS)
		{
			var categoryList = document.getElementById("categoryList");
			if (categoryList)
			{
				categoryList.database.AddDataSource(catDS);
				var ref = categoryList.getAttribute("ref");
				if (ref)	categoryList.setAttribute("ref", ref);
			}
			var engineTree = document.getElementById("searchengines");
			if (engineTree)
			{
				engineTree.database.AddDataSource(catDS);
				var ref = engineTree.getAttribute("ref");
				if (ref)	engineTree.setAttribute("ref", ref);
			}
		}
	}

	// try and determine last category name used
	var lastCategoryName = "";
	try
	{
		if (pref)	lastCategoryName = pref.CopyCharPref( "browser.search.last_search_category" );

		if (lastCategoryName != "")
		{
			// strip off the prefix if necessary
			var prefix="NC:SearchCategory?category=";
			if (lastCategoryName.indexOf(prefix) == 0)
			{
				lastCategoryName = lastCategoryName.substr(prefix.length);
			}
		}

	}
	catch( e )
	{
		debug("Exception in SearchPanelStartup\n");
		lastCategoryName = "";
	}
	debug("\nSearchPanelStartup: lastCategoryName = '" + lastCategoryName + "'\n");

	// select the appropriate category
	var categoryList = document.getElementById( "categoryList" );
	var categoryPopup = document.getElementById( "categoryPopup" );
	if( categoryList && categoryPopup )
	{
	 	var found = false;
		for( var i = 0; i < categoryPopup.childNodes.length; i++ )
		{
			if( ( lastCategoryName == "" &&
			      categoryPopup.childNodes[i].getAttribute("data") == "NC:SearchEngineRoot" ) ||
			    ( categoryPopup.childNodes[i].getAttribute("id") == lastCategoryName ) )
			{
				categoryList.selectedItem = categoryPopup.childNodes[i];
				found = true;
				break;
			}
		}
		if (found == false)
		{
			categoryList.selectedItem = categoryPopup.childNodes[0];
		}
    
		if (( lastCategoryName == "" ) || (lastCategoryName == null))
		{
			lastCategoryName = "NC:SearchEngineRoot";
		}
		if (lastCategoryName != "NC:SearchEngineRoot")
		{
			lastCategoryName = "NC:SearchCategory?category=" + lastCategoryName;
		}

		var treeNode = document.getElementById("searchengines");
		treeNode.setAttribute( "ref", lastCategoryName );
	}

	loadEngines( lastCategoryName );

	var searchMode = updateSearchMode();
	// if we have search results, show them, otherwise show engines
	if ((haveSearchResults() == true) || (searchMode == 0))
	{
		switchTab(0);
	}
	else
	{
		switchTab(1);
	}
}



function haveSearchResults()
{
	var resultsTree = document.getElementById("Tree");
	if( !resultsTree)	return(false);
	var ds = resultsTree.database;
	if (!ds)		return(false);

    var rdf = Components.classes[RDFSERVICE_PROGID].getService(nsIRDFService);
	if (rdf)
	{
		var source = rdf.GetResource( "NC:LastSearchRoot", true);
		var childProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#LastText", true);
		var target = ds.GetTarget(source, childProperty, true);
		if (target)	target = target.QueryInterface(nsIRDFLiteral);
		if (target)	target = target.Value;
		if (target && target != "")
		{
			var textNode = document.getElementById("sidebar-search-text");
			if (!textNode)	return(false);

			// convert pluses (+) back to spaces
			target = target.replace(/+/i, " ");

			textNode.value = unescape(target);
			return(true);
		}
	}
	return(false);
}



function getNumEngines()
{
 	var treeChildrenNode = document.getElementById("engineKids");
	var numChildren = treeChildrenNode.childNodes.length;
    debug("getNumEngines:  numChildren = " + numChildren + "\n");
	return(numChildren);
}



function chooseCategory( aNode )
{
  var category = !aNode.id ? "NC:SearchEngineRoot" : 
                  "NC:SearchCategory?category=" + aNode.getAttribute("id");

  if (pref)	
    pref.SetUnicharPref("browser.search.last_search_category", category);

  var treeNode = document.getElementById("searchengines");
  if (treeNode)
  {
    debug("chooseCategory:  ref='" + category + "'\n");
    treeNode.setAttribute("ref", category);
  }
  loadEngines(category);
  return(true);
}



// check an engine representation in the engine list
function doCheck(aNode)
{
	saveEngines();
	return(false);
}



function saveEngines()
{
	var categoryList = document.getElementById("categoryList");
	var category = categoryList.selectedItem.getAttribute("id");
	if( category )
	{
		category = "NC:SearchCategory?category=" + category;
	}
	else
	{
		category = "NC:SearchEngineRoot";
	}

    debug("Category: " + category + "\n");

    var rdf = Components.classes[RDFSERVICE_PROGID].getService(nsIRDFService);
	if (rdf)
	{
		var localStore = rdf.GetDataSource("rdf:local-store");
		if( !localStore )	return(false);

   	var engineBox = document.getElementById("engineKids");
    if( !engineBox )
      return false;
    var checkedProperty = rdf.GetResource( "http://home.netscape.com/NC-rdf#checked", true );
    var categorySRC = rdf.GetResource( category, true );

    debug("# of engines: " + engineBox.childNodes.length + "\n");

    for (var x = 0; x < engineBox.childNodes.length; x++)
    {
      var treeitemNode = engineBox.childNodes[x];
      var engineURI = treeitemNode.getAttribute("id");
      var engineSRC = rdf.GetResource(engineURI, true);

      var checkboxNode = treeitemNode.firstChild.firstChild.firstChild;
      if( !checkboxNode )
      	continue;

      if( checkboxNode.checked == true || checkboxNode.checked == "true")
      {
        debug("  Check engine #" + x + "    " + engineURI + "\n");
        localStore.Assert( categorySRC, checkedProperty, engineSRC, true );
      }
      else
      {
        debug("UnCheck engine #" + x + "    " + engineURI + "\n");
        localStore.Unassert( categorySRC, checkedProperty, engineSRC, true );
      }
    }

	// save changes; flush out the localstore
	try
	{
        var flushableStore = localStore.QueryInterface(nsIRDFRemoteDataSource);
		if (flushableStore)	flushableStore.Flush();
        debug("Flushed changes\n\n");
	}
	catch(ex)
	{
	}
  }
}



// initialise the appropriate engine list, and the checked state of the engines
function loadEngines( aCategory )
{
    var rdf = Components.classes[RDFSERVICE_PROGID].getService(nsIRDFService);
    if (rdf)
    {
        var localStore = rdf.GetDataSource("rdf:local-store");
        if (localStore)
        {
            var engineBox = document.getElementById("engineKids");
            if( engineBox )
            {
                var numEngines = engineBox.childNodes.length;
                var checkedProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#checked", true);
                var categorySRC = rdf.GetResource( aCategory, true );
                for (var x = 0; x<numEngines; x++)
                {
                    var treeitemNode = engineBox.childNodes[x];
                    var engineURI = treeitemNode.getAttribute("id");
                    var engineSRC = rdf.GetResource( engineURI, true );

                    var checkboxNode = treeitemNode.firstChild.firstChild.firstChild;
                    if (!checkboxNode) continue;

                    var hasAssertion = localStore.HasAssertion( categorySRC, checkedProperty, engineSRC, true );
                    if (hasAssertion)
                    {
                        checkboxNode.checked = true;
                    }
                }
            }
        }
    }
}



function SearchPanelShutdown()
{
	var tree = document.getElementById("Tree");
	if (tree)
	{
		tree.database.RemoveObserver(RDF_observer);
	}
}



function doStop()
{
	var stopButtonNode = document.getElementById("stopbutton");
    if (stopButtonNode)
    {
		stopButtonNode.setAttribute("style", "display: none;");
    }

	var searchButtonNode = document.getElementById("searchbutton");
	if(searchButtonNode)
	{
		searchButtonNode.setAttribute("style", "display: inherit;");
	}

	// should stop button press also stop the load of the page in the browser? I think so. 
	var progressNode = parent.document.getElementById("statusbar-icon");
    if (progressNode)
    {
		progressNode.setAttribute("mode", "normal");
    }

	// stop any network connections
    var internetSearchService = Components.classes[ISEARCH_PROGID].getService(nsIInternetSearchService);
    var internetSearch = null;
    if (internetSearchService)
    {
        internetSearchService.Stop();
        internetSearch = internetSearchService.QueryInterface(nsIRDFDataSource);
    }

	// get various services
    var rdf = Components.classes[RDFSERVICE_PROGID].getService(nsIRDFService);

	var sortSetFlag = false;

	// show appropriate column(s)
	if ((rdf) && (internetSearch))
	{
    var navWindow = getNavigatorWindow();
		var resultsTree = navWindow._content.document.getElementById("internetresultstree");
		if( !resultsTree )
			return(false);
		var searchURL = resultsTree.getAttribute("ref");
		if( !searchURL )	
			return(false);

		var searchResource       = rdf.GetResource(searchURL, true);
		var priceProperty        = rdf.GetResource("http://home.netscape.com/NC-rdf#Price", true);
		var availabilityProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#Availability", true);
		var relevanceProperty    = rdf.GetResource("http://home.netscape.com/NC-rdf#Relevance", true);
		var dateProperty         = rdf.GetResource("http://home.netscape.com/NC-rdf#Date", true);
		var trueProperty         = rdf.GetLiteral("true");

		var hasPriceFlag         = internetSearch.HasAssertion(searchResource, priceProperty, trueProperty, true);
		var hasAvailabilityFlag  = internetSearch.HasAssertion(searchResource, availabilityProperty, trueProperty, true);
		var hasRelevanceFlag     = internetSearch.HasAssertion(searchResource, relevanceProperty, trueProperty, true);
		var hasDateFlag          = internetSearch.HasAssertion(searchResource, dateProperty, trueProperty, true);

    var navWindow = getNavigatorWindow();
		if(hasPriceFlag == true)
		{
			var colNode = navWindow._content.document.getElementById("PriceColumn");
			if (colNode)
			{
				colNode.removeAttribute("style", "width: 0; visibility: collapse;");
				if (sortSetFlag == false)
				{
					top._content.setInitialSort(colNode, "ascending");
					sortSetFlag = true;
				}
			}
		}
		if (hasAvailabilityFlag == true)
		{
			colNode = navWindow._content.document.getElementById("AvailabilityColumn");
			if (colNode)
				colNode.removeAttribute("style", "width: 0; visibility: collapse;");
		}
		if (hasDateFlag == true)
		{
			colNode = navWindow._content.document.getElementById("DateColumn");
			if (colNode)
				colNode.removeAttribute("style", "width: 0; visibility: collapse;");
		}
		if (hasRelevanceFlag == true)
		{
			colNode = navWindow._content.document.getElementById("RelevanceColumn");
			if (colNode)
			{
				colNode.removeAttribute("style", "width: 0; visibility: collapse;");
				if (sortSetFlag == false)
				{
					navWindow._content.setInitialSort(colNode, "descending");
					sortSetFlag = true;
				}
			}
		}
	}

	if (sortSetFlag == false)
	{
		colNode = navWindow._content.document.getElementById("PageRankColumn");
		if (colNode)
			navWindow._content.setInitialSort(colNode, "ascending");
	}
	switchTab(0);
}



function doSearch()
{
  var searchButton = document.getElementById("searchbutton");      
  if ( searchButton.getAttribute("disabled") )
	{	   
	   var sidebarSearchText = document.getElementById("sidebar-search-text");
	   sidebarSearchText.focus();
	   return;
	}

	//get click count pref for later
	//and set tree attribute to cause proper results appearance (like links) to happen 
	//when user set pref to single click
	var searchMode = 0;
    var mClickCount = 1;
    var prefvalue = false;

    try
    {
        if( pref )
        {
            searchMode = pref.GetIntPref("browser.search.mode");
            prefvalue = pref.GetBoolPref( "browser.search.use_double_clicks" );
            mClickCount = prefvalue ? 2 : 1;
        } 
    }
    catch(e)
    {
        searchMode = 0;
        mClickCount = 1;
        prefvalue = false;
    }

    var tree = document.getElementById("Tree");
    if (mClickCount == 1)
    {
        tree.setAttribute("singleclick","true");
    }
    else
    {
        tree.removeAttribute("singleclick");
    }
  
	// hide various columns
    var navWindow = getNavigatorWindow();
    dump("*** navWindow = " + navWindow + "\n");
    if( navWindow._content.isMozillaSearchWindow )
    {
        colNode = navWindow._content.document.getElementById("RelevanceColumn");
        if (colNode)	colNode.setAttribute("style", "width: 0; visibility: collapse;");
        colNode = navWindow._content.document.getElementById("PriceColumn");
        if (colNode)	colNode.setAttribute("style", "width: 0; visibility: collapse;");
        colNode = navWindow._content.document.getElementById("AvailabilityColumn");
        if (colNode)	colNode.setAttribute("style", "width: 0; visibility: collapse;");
    }

	// get user text to find
	var textNode = document.getElementById("sidebar-search-text");
	if(!textNode)   return(false);
	if ( !textNode.value )
	{
    alert(bundle.GetStringFromName("enterstringandlocation") );
		return(false);
	}

	var searchURL   = "";
	var foundEngine = false;
  var engineURIs  = [];

  if (searchMode > 0)
  {
      // in advanced search mode, get selected search engines
      // (for the current search category)
  	var engineBox = document.getElementById("engineKids");
  	if (!engineBox)	return(false);

  	for (var x = 0; x<engineBox.childNodes.length; x++)
  	{
  	  var treeitemNode = engineBox.childNodes[x];
      if (!treeitemNode) continue;
  
      var checkboxNode = treeitemNode.firstChild.firstChild.firstChild;
      if (!checkboxNode) continue;
      
      if ( checkboxNode.checked == true || checkboxNode.checked == "true") 
      {
        var engineURI = treeitemNode.getAttribute("id");
        if (!engineURI)	continue;
        engineURIs[engineURIs.length] = engineURI;
        foundEngine = true;
      }
    }
    if (!foundEngine)
    {
      if( getNumEngines() == 1 ) {
        // only one engine in this category, check it
        var treeitemNode = engineBox.firstChild;
        engineURIs[engineURIs.length] = treeitemNode.getAttribute("id");
      }
      else {
        for( var i = 0; i < engineBox.childNodes.length; i++ )
        {
          var treeitemNode = engineBox.childNodes[i];
          var theID = treeitemNode.getAttribute("id");
          if( theID.indexOf("NetscapeSearch.src") != -1 )
          {
            engineURIs[engineURIs.length] = theID;
            foundEngine = true;
            break;
          }
        }
        if (foundEngine == false)
        {
            alert(bundle.GetStringFromName("enterstringandlocation") );
            return(false);
        }
      }
  	}
  }

	// hide search button
	var searchButtonNode = document.getElementById("searchbutton");
	if (searchButtonNode)
    searchButtonNode.setAttribute("style", "display: none;");
	
	// show stop button
	var stopButtonNode = document.getElementById("stopbutton");
	if (stopButtonNode)
    stopButtonNode.removeAttribute("style", "display: none;");

	var progressNode = top.document.getElementById("statusbar-icon");
	if (progressNode)
    progressNode.setAttribute( "mode", "undetermined" );
   
  // run the search
  OpenSearch(textNode.value, engineURIs );
  switchTab(0);
  return(true);
}



function checkSearchProgress()
{
	var	activeSearchFlag = false;
  var navWindow = getNavigatorWindow();
	var	resultsTree = navWindow._content.document.getElementById("internetresultstree");
	if(resultsTree)
	{	
    	var treeref = resultsTree.getAttribute("ref");
    	var ds = resultsTree.database;
    	if (ds && treeref)
    	{
    		try
    		{
                var rdf = Components.classes[RDFSERVICE_PROGID].getService(nsIRDFService);
    			if (rdf)
    			{
    				var source = rdf.GetResource(treeref, true);
    				var loadingProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#loading", true);
    				var target = ds.GetTarget(source, loadingProperty, true);
    				if (target)	target = target.QueryInterface(nsIRDFLiteral);
    				if (target)	target = target.Value;
    				if (target == "true")
    				{
    					activeSearchFlag = true;
    				}
    				else
    				{
    					activeSearchFlag = false;
    				}
    			}
    		}
    		catch(ex)
    		{
				activeSearchFlag = false;
    		}
    	}
    }

	if( activeSearchFlag )
	{
		setTimeout("checkSearchProgress()", 1000);
	}
	else
	{
		doStop();
	}
  
	return(activeSearchFlag);
}



function sidebarOpenURL(event, treeitem, root)
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
  
  if ((event.button != 1) || (event.detail != mClickCount))
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
		var theRootNode = document.getElementById(root);
		var ds = null;
		if (rootNode)
		{
			ds = theRootNode.database;
		}
        var rdf = Components.classes[RDFSERVICE_PROGID].getService(nsIRDFService);
		if (rdf)
		{
			if (ds)
			{
				var src = rdf.GetResource(id, true);
				var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
				var target = ds.GetTarget(src, prop, true);
				if (target)	target = target.QueryInterface(nsIRDFLiteral);
				if (target)	target = target.Value;
				if (target)	id = target;
			}
		}
	}
	catch(ex)
	{
	}

    loadURLInContent(id);
}



function OpenSearch( aSearchStr, engineURIs )
{
	var searchEngineURI = null;
	var autoOpenSearchPanel = false;
	var defaultSearchURL = null;

	try
	{
        if (pref)
        {
	  	    autoOpenSearchPanel = pref.GetBoolPref("browser.search.opensidebarsearchpanel");
	        searchEngineURI = pref.CopyCharPref("browser.search.defaultengine");
            debug("From prefs, searchEngineURI = '" + searchEngineURI + "'\n");
  		    defaultSearchURL = pref.getLocalizedUnicharPref("browser.search.defaulturl");
        }
	}
	catch(ex)
	{
	}

	if ( !defaultSearchURL )
	{
		defaultSearchURL = bundle.GetStringFromName("defaultSearchURL");
    }

    var searchDS = Components.classes[ISEARCH_PROGID].getService(nsIInternetSearchService);
    if( searchDS )
    {
        if(!aSearchStr)
        {
            return(false);
        }

        var	escapedSearchStr = escape( aSearchStr );
        searchDS.RememberLastSearchText( escapedSearchStr );

  		try
  		{
            if( !engineURIs || ( engineURIs && engineURIs.length <= 1 ) )
            {
                debug("searchEngineURI = '" + searchEngineURI + "'\n");
                // not called from sidebar or only one engine selected
                if (engineURIs && engineURIs.length == 1)
                {
                    debug("Got one engine: " + engineURIs[0] + "\n");
                    searchEngineURI = engineURIs[0];
                    gURL = "internetsearch:engine=" + searchEngineURI + "&text=" + escapedSearchStr;
                }
                else
                {
                    debug("No engines, default to '" + searchEngineURI + "'\n");
                }

		if ((!searchEngineURI) || (searchEngineURI == ""))
		{
			searchEngineURI = bundle.GetStringFromName("defaultSearchURL");
		}

                // look up the correct search URL format for the given engine
		try
		{
			var	searchURL = searchDS.GetInternetSearchURL( searchEngineURI, escapedSearchStr );
		}
		catch(ex)
		{
			searchURL = "";
		}

                if (searchURL)
                {
                    defaultSearchURL = searchURL;
                }
                else 
                {
                    defaultSearchURL = defaultSearchURL + escapedSearchStr;
                    gURL = "";
                }

                // load the results page of selected or default engine in the content area
                if (defaultSearchURL)
                {
                    loadURLInContent(defaultSearchURL);
                }
            }
            else
            {
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
                gURL = searchURL;
                loadURLInContent("chrome://communicator/content/search/internetresults.xul?" + searchURL);
            }
  		}
  		catch(ex)
  		{
  		}

        setTimeout("checkSearchProgress()", 1000);
    }
}



function switchTab( aPageIndex )
{
	var deck = document.getElementById( "advancedDeck" );
	deck.setAttribute( "index", aPageIndex );
  
  	// decide whether to show/hide/enable/disable save search query button
	if (aPageIndex != 0)	return(true);

	var saveQueryButton = document.getElementById("saveQueryButton");
	if (!saveQueryButton)	return(true);

	var resultsTree = document.getElementById("Tree");
	if( !resultsTree)	return(false);
	var ds = resultsTree.database;
	if (!ds)		return(false);

	var	haveSearchRef = false;

    var rdf = Components.classes[RDFSERVICE_PROGID].getService(nsIRDFService);
	if (rdf)
	{
		var source = rdf.GetResource( "NC:LastSearchRoot", true);
		var childProperty;
		var target;

		// look for last search URI
		childProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#ref", true);
		target = ds.GetTarget(source, childProperty, true);
		if (target)	target = target.QueryInterface(nsIRDFLiteral);
		if (target)	target = target.Value;
		if (target && target != "")
		{
			haveSearchRef = true;
		}
	}

	if (haveSearchRef == true)
	{
		saveQueryButton.removeAttribute("disabled", "true");
    }
	else
	{
		saveQueryButton.setAttribute("disabled", "true");
    }
    return(true);
}



function saveSearch()
{
	var resultsTree = document.getElementById("Tree");
	if( !resultsTree)	return(false);
	var ds = resultsTree.database;
	if (!ds)		return(false);

	var	lastSearchURI="";
	var	lastSearchText="";

    var rdf = Components.classes[RDFSERVICE_PROGID].getService(nsIRDFService);
	if (rdf)
	{
		var source = rdf.GetResource( "NC:LastSearchRoot", true);
		var childProperty;
		var target;

		// look for last search URI
		childProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#ref", true);
		target = ds.GetTarget(source, childProperty, true);
		if (target)	target = target.QueryInterface(nsIRDFLiteral);
		if (target)	target = target.Value;
		if (target && target != "")
		{
			lastSearchURI = target;
			debug("Bookmark search  URL: '" + lastSearchURI + "'\n");
		}

		// look for last search text
		childProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#LastText", true);
		target = ds.GetTarget(source, childProperty, true);
		if (target)	target = target.QueryInterface(nsIRDFLiteral);
		if (target)	target = target.Value;
		if (target && target != "")
		{
			// convert pluses (+) back to spaces
			target = target.replace(/+/i, " ");

			lastSearchText = unescape(target);
			debug("Bookmark search Name: '" + lastSearchText + "'\n");
		}
	}


	if ((lastSearchURI == null) || (lastSearchURI == ""))	return(false);

    // 	rjc says: if lastSearchText is empty/null, that's still OK, synthesize the text
	if ((lastSearchText == null) || (lastSearchText == ""))
	{
		lastSearchText = lastSearchURI;
		var siteOffset = lastSearchText.indexOf("://");
		if (siteOffset > 0)
		{
			siteOffset += 3;
			var endOffset = lastSearchText.indexOf("/", siteOffset);
			if (endOffset > 0)
			{
				lastSearchText = lastSearchText.substr(0, endOffset+1);
			}
		}
	}

    var bmks = Components.classes[BMARKS_PROGID].getService(nsIBookmarksService);

	var textNode = document.getElementById("sidebar-search-text");
	if( !textNode )		return(false);

	var searchTitle = "Search: '" + lastSearchText + "'";	// using " + gSites;
	if (bmks)	bmks.AddBookmark(lastSearchURI, searchTitle, bmks.BOOKMARK_SEARCH_TYPE, null);

	return(true);
}



function doCustomize()
{
	window.openDialog("chrome://communicator/content/search/search-editor.xul", "_blank", "centerscreen,chrome,resizable");
}



function loadURLInContent(url)
{
    var appCore = getNavigatorWindowAppCore();
    if (appCore)
      appCore.loadUrl(url);
}

// retrieves the most recent navigator window
function getNavigatorWindow()
{
  const WM_PROGID = "component://netscape/rdf/datasource?name=window-mediator";
  var wm;
  if (top.document) {
    var possibleNavigator = top.document.getElementById("main-window");
    if (possibleNavigator && 
        possibleNavigator.getAttribute("windowtype") == "navigator:browser")
      return top;
    else {
      wm = nsJSComponentManager.getService(WM_PROGID, "nsIWindowMediator");
      return wm ? wm.getMostRecentWindow("navigator:browser") : openNewNavigator();
    }
  }
  else {
    wm = nsJSComponentManager.getService(WM_PROGID, "nsIWindowMediator");
    return wm ? wm.getMostRecentWindow("navigator:browser") : openNewNavigator();
  }
}

// retrieves the most recent navigator window's crap core. 
function getNavigatorWindowAppCore()
{
  var navigatorWindow = getNavigatorWindow();
  return navigatorWindow.appCore;
}

function openNewNavigator()
{
  var newNavigator = open(search_getBrowserURL(), "_blank", "chrome,all,dialog=no");
  dump("*** newNavigator - " + newNavigator + "\n");
  return newNavigator;
}

function search_getBrowserURL()
{
    var url = "chrome://navigator/content/navigator.xul";

    if (pref) {
        var temp = pref.CopyCharPref("browser.chromeURL");
        if (temp) url = temp;
    }
    return url;
}



function doEnabling()
{
	var searchButton = document.getElementById("searchbutton");
	var sidebarSearchText = document.getElementById("sidebar-search-text");

    if ( sidebarSearchText.value == "" ) 
    {
        // No input, disable search button if enabled.
        if ( !searchButton.getAttribute("disabled") )
        searchButton.setAttribute("disabled","true");
    }
    else
    {
        if ( searchButton.getAttribute("disabled") == "true")
        {
            searchButton.removeAttribute("disabled");
        }
    }
}
