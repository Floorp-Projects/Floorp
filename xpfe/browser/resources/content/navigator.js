/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

var pref = null;
var bundle = srGetStrBundle("chrome://navigator/locale/navigator.properties");

// in case we fail to get the start page, load this
var startPageDefault = "about:blank";

// in case we fail to get the home page, load this
var homePageDefault = bundle.GetStringFromName( "homePageDefault" );

try {
	pref = Components.classes['component://netscape/preferences'];
	pref = pref.getService();
	pref = pref.QueryInterface(Components.interfaces.nsIPref);
}
catch (ex) {
	dump("failed to get prefs service!\n");
	pref = null;
}

  var appCore = null;
  var explicitURL = false;
  var textZoom = 1.0;

  // Stored Status, Link and Loading values
  var defaultStatus = bundle.GetStringFromName( "defaultStatus" );
  var jsStatus = null;
  var jsDefaultStatus = null;
  var overLink = null; 
  var startTime = 0;

  //cached elements/ fields
  var statusTextFld = null;
  var statusMeter = null;
  var throbberElement = null;
  var stopButton = null;
  var stopMenu = null;
  var locationFld = null;
  var backButton	= null;
  var forwardButton = null;

function savePage( url ) {
        // Default is to save current page.
        if ( !url ) {
            url = window.content.location.href;
        }
        // Use stream xfer component to prompt for destination and save.
        var xfer = Components
                     .classes[ "component://netscape/appshell/component/xfer" ]
                       .getService( Components.interfaces.nsIStreamTransfer );
        try {
            // When Necko lands, we need to receive the real nsIChannel and
            // do SelectFileAndTransferLocation!

            // Use this for now...
            xfer.SelectFileAndTransferLocationSpec( url, window );
        } catch( exception ) {
            // Failed (or cancelled), give them another chance.
            dump( "SelectFileAndTransferLocationSpec failed, rv=" + exception + "\n" );
        }
        return;
    }


function UpdateBookmarksLastVisitedDate(event)
{
	if ((window.content.location.href) && (window.content.location.href != ""))
	{
		try
		{
			// if the URL is bookmarked, update its "Last Visited" date
			var bmks = Components.classes["component://netscape/browser/bookmarks-service"].getService();
			if (bmks)	bmks = bmks.QueryInterface(Components.interfaces.nsIBookmarksService);
			if (bmks)	bmks.UpdateBookmarkLastVisitedDate(window.content.location.href);
		}
		catch(ex)
		{
			dump("failed to update bookmark last visited date.\n");
		}
	}
}



function UpdateInternetSearchResults(event)
{
	if ((window.content.location.href) && (window.content.location.href != ""))
	{
		var	searchInProgressFlag = false;

		try
		{
			var search = Components.classes["component://netscape/rdf/datasource?name=internetsearch"].getService();
			if (search)	search = search.QueryInterface(Components.interfaces.nsIInternetSearchService);
			if (search)	searchInProgressFlag = search.FindInternetSearchResults(window.content.location.href);
		}
		catch(ex)
		{
		}

		if (searchInProgressFlag == true)
		{
			RevealSearchPanel();
		}
	}
}



function createBrowserInstance()
{
    appCore = Components
                .classes[ "component://netscape/appshell/component/browser/instance" ]
                  .createInstance( Components.interfaces.nsIBrowserInstance );
    if ( !appCore ) {
        alert( "Error creating browser instance\n" );
    }
  }

function UpdateStatusField()
{
	var text = defaultStatus;

	if(jsStatus)
		text = jsStatus;
	else if(overLink)
		text = overLink;
	else if(jsDefaultStatus)
		text = jsDefaultStatus;

	if(!statusTextFld)
		statusTextFld = document.getElementById("statusText");

	statusTextFld.setAttribute("value", text);
}

function nsXULBrowserWindow()
{
}

nsXULBrowserWindow.prototype = 
{
	QueryInterface : function(iid)
		{
		if(iid.equals(Components.interfaces.nsIXULBrowserWindow))
			return this;
		throw Components.results.NS_NOINTERFACE;
		},
	setJSStatus : function(status)
		{
		if(status == "")
			jsStatus = null;
		else
			jsStatus = status;
		UpdateStatusField();
		},
	setJSDefaultStatus : function(status)
		{
		if(status == "")
			jsDefaultStatus = null;
		else
			jsDefaultStatus = status;
		UpdateStatusField();
		},
	setDefaultStatus : function(status)
		{
		if(status == "")
			defaultStatus = null;
		else
			defaultStatus = status;
		UpdateStatusField();
		},
	setOverLink : function(link)
		{
		if(link == "")
			overLink = null;
		else
			overLink = link;
		UpdateStatusField();
		},
  onProgress : function (channel, current, max)
  {
 		if(!statusMeter)
			statusMeter = document.getElementById("statusbar-icon");
    var percentage = 0;
    if (max > 0)
    {
      percentage = (current * 100) / max ;
      statusMeter.setAttribute("mode", "normal");
      statusMeter.value = percentage;
      statusMeter.progresstext = Math.round(percentage) + "%";
    }
    else
      statusMeter.setAttribute("mode","undetermined");
         
  },
	onStatusChange : function(channel, status)
		{
		if(!throbberElement)
			throbberElement = document.getElementById("navigator-throbber");
		if(!statusMeter)
			statusMeter = document.getElementById("statusbar-icon");
		if(!stopButton)
			stopButton = document.getElementById("stop-button");
		if(!stopMenu)
			stopMenu = document.getElementById("menuitem-stop");

		if(status & Components.interfaces.nsIWebProgress.flag_net_start)
			{
			// Remember when loading commenced.
    		startTime = (new Date()).getTime();
         // Turn progress meter on.
			statusMeter.setAttribute("mode","undetermined");
			throbberElement.setAttribute("busy", true);
			}
		else if(status & Components.interfaces.nsIWebProgress.flag_net_stop)
			{
			// Record page loading time.
			var elapsed = ( (new Date()).getTime() - startTime ) / 1000;
			var msg = bundle.GetStringFromName("nv_done") + " (" + elapsed + " secs)";
			dump( msg + "\n" );
			defaultStatus = msg;
			UpdateStatusField();
			window.XULBrowserWindow.setDefaultStatus(msg);
       // Turn progress meter off.
       statusMeter.setAttribute("mode","normal");
       statusMeter.value = 0;  // be sure to clear the progress bar
       statusMeter.progresstext = "";
			throbberElement.setAttribute("busy", false);
			}
		else if(status & Components.interfaces.nsIWebProgress.flag_net_dns)
			{
			alert("XXX: Not yet Implemented (navigator.js dns network step.");
			}
		else if(status & Components.interfaces.nsIWebProgress.flag_net_connecting)
			{
			alert("XXX: Not yet Implemented (navigator.js connecting network step.");
			}
		else if(status & Components.interfaces.nsIWebProgress.flag_net_transferring)
			{
			alert("XXX: Not yet Implemented (navigator.js transferring network step.");
			}
		else if(status & Components.interfaces.nsIWebProgress.flag_net_redirecting)
			{
			alert("XXX: Not yet Implemented (navigator.js redirecting network step.");
			}
		else if(status & Components.interfaces.nsIWebProgress.flag_net_negotiating)
			{
			alert("XXX: Not yet Implemented (navigator.js negotiating network step.");
			}

		if(status & Components.interfaces.nsIWebProgress.flag_win_start)
			{
			stopButton.setAttribute("disabled", false);
			stopMenu.setAttribute("disabled", false);
			}
		else if(status & Components.interfaces.nsIWebProgress.flag_win_stop)
			{
			stopButton.setAttribute("disabled", true);
			stopMenu.setAttribute("disabled", true);
			}
		},
	onLocationChange : function(location)
		{
		if(!locationFld)
			locationFld = document.getElementById("urlbar");

		// We should probably not do this if the value has changed since the user 
		// searched
		locationFld.setAttribute("value", location);

		UpdateBackForwardButtons();
		}
}

function UpdateBackForwardButtons()
{
	if(!backButton)
		backButton = document.getElementById("canGoBack");
	if(!forwardButton)								  
		forwardButton = document.getElementById("canGoForward");
		
	backButton.setAttribute("disabled", !appCore.canGoBack);	
	forwardButton.setAttribute("disabled", !appCore.canGoForward);
}

function Startup()
  {
    window.XULBrowserWindow = new nsXULBrowserWindow();
    //  TileWindow();
    // Make sure window fits on screen initially
    //FitToScreen();
	
    // Create the browser instance component.
    createBrowserInstance();
    if (appCore == null) {
        // Give up.
        window.close();
    }

    // Initialize browser instance..
    appCore.setWebShellWindow(window);
    
		// give urlbar focus so it'll be an active editor and d&d will work properly
		var url_bar = document.getElementById("urlbar");
		if ( url_bar )
      url_bar.focus();
    
    tryToSetContentWindow();
	
    // Add a capturing event listener to the content area
    // (rjc note: not the entire window, otherwise we'll get sidebar pane loads too!)
    //  so we'll be notified when onloads complete.
    var contentArea = document.getElementById("appcontent");
    if (contentArea)
    {
	    contentArea.addEventListener("load", UpdateBookmarksLastVisitedDate, true);
	    contentArea.addEventListener("load", UpdateInternetSearchResults, true);
    }

	 // Check for window.arguments[0].  If present, go to that url.
    if ( window.arguments && window.arguments[0] ) {
        // Load it using yet another psuedo-onload handler.
        onLoadViaOpenDialog();
    }
  }

function Shutdown()
{
	try
	{
		// If bookmarks are dirty, flush 'em to disk
		var bmks = Components.classes["component://netscape/browser/bookmarks-service"].getService();
		if (bmks)	bmks = bmks.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
		if (bmks)	bmks.Flush();
    	}
	catch (ex)
	{
	}
	try
	{
		// give history a change at flushing to disk also                           
    		var history = getService( "component://netscape/browser/global-history", "nsIRDFRemoteDataSource" );
    		if (history)    
      		history.Flush();   
	}
	catch (ex)
	{
	}
	
  // Close the app core.
  if ( appCore )
    appCore.close();
}

  function onLoadViaOpenDialog() {
    // See if load in progress (loading default page).
    if ( document.getElementById("navigator-throbber").getAttribute("busy") == "true" ) {
        dump( "Stopping load of default initial page\n" );
        appCore.stop();
    }
    dump( "Loading page specified via openDialog\n" );
    appCore.loadUrl( window.arguments[0] );
  }

  function tryToSetContentWindow() {
    var startpage = startPageDefault;
    if ( window.content ) {
        dump("Setting content window\n");
        appCore.setContentWindow( window.content );
        // Have browser app core load appropriate initial page.

/* sspitzer: I think this code is unnecessary, but I'll leave it until I prove it */
/* START OF UNNECESSARY CODE */
        if ( !explicitURL ) {
            try {
                var handler = Components.classes['component://netscape/commandlinehandler/general-startup-browser'];
                handler = handler.getService();
                handler = handler.QueryInterface(Components.interfaces.nsICmdLineHandler);
                if (handler) {
                    startpage = handler.defaultArgs;
                }
            }
            catch (ex) {
                dump("failed, reason: " + ex + "\n");
                startpage = startPageDefault;
            }

            //dump("startpage = " + startpage + "\n");
            var args = document.getElementById("args")
            if (args) args.setAttribute("value", startpage);
        }
/* END OF UNNECESSARY CODE */

        appCore.loadInitialPage();
    } else {
        // Try again.
        dump("Scheduling later attempt to set content window\n");
        window.setTimeout( "tryToSetContentWindow()", 100 );
    }
  }

  function Translate()
  {
	var service = "http://cgi.netscape.com/cgi-bin/translate.cgi?AlisUI=simple_frames/ns_home";

	// if we're already viewing a translated page, then just get the
	// last argument (which we expect to always be "AlisTargetURI")
	var targetURI = window.content.location.href;
	var targetURIIndex = targetURI.indexOf("AlisTargetURI=");
	if (targetURIIndex >= 0)
	{
		targetURI = targetURI.substring(targetURIIndex + 14);
	}
	service += "&AlisTargetURI=" + targetURI;

	window.content.location.href = service;
  }

  function RefreshUrlbar()
  {
   //Refresh the urlbar bar
    document.getElementById('urlbar').value = window.content.location.href;
  }

  function gotoHistoryIndex(event)
  {
     dump("In gotoHistoryIndex\n");
	 var index = event.target.getAttribute("index");
	 if (index) {
	    dump("gotoHistoryIndex:: Index = " + index);
        appCore.gotoHistoryIndex(index);
	    return true;
	 }
	 else {
	 /* NOT NEEDED NOW 
	   var id = event.target.getAttribute("id");
	   if (id == "menuitem-back")
	      BrowserBack();
	   else if (id == "menuitem-forward")
	      BrowserForward();	  
	 */
     }
  }

  function BrowserBack()
  {
     // Get a handle to the back-button
     var bb = document.getElementById("canGoBack");
     // If the button is disabled, don't bother calling in to Appcore
     if ( (bb.getAttribute("disabled")) == "true" ) 
        return;

      dump("Going Back\n");
      appCore.back();
		UpdateBackForwardButtons();
  }


  function BrowserForward()
  {
     // Get a handle to the back-button
     var fb = document.getElementById("canGoForward");
     // If the button is disabled, don't bother calling in to Appcore
     if ( (fb.getAttribute("disabled")) == "true" ) 
        return;

      dump("Going Forward\n");
      appCore.forward();
      UpdateBackForwardButtons();
  }

  function BrowserBackMenu(event)
  {
  //   dump("&&&& In BrowserbackMenu &&&&\n");
     // Get a handle to the back-button
     var bb = document.getElementById("canGoBack");
     // If the button is disabled, don't bother calling in to Appcore
     if ( (bb.getAttribute("disabled")) == "true" ) 
        return;

    if (appCore != null) {
      appCore.backButtonPopup(event.target);
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
	
  }


  function BrowserForwardMenu(event)
  {
  //    dump("&&&& In BrowserForwardMenu &&&&\n");
     // Get a handle to the forward-button
     var bb = document.getElementById("canGoForward");
     // If the button is disabled, don't bother calling in to Appcore
     if ( (bb.getAttribute("disabled")) == "true" ) 
        return;

    if (appCore != null) {
      appCore.forwardButtonPopup(event.target);
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
	
  }



  function BrowserStop() {
     if(!stopButton)
		 stopButton = document.getElementById("stop-button");

     // If the stop button is currently disabled, just return
     if ((stopButton.getAttribute("disabled")) == "true")
		 return;
	
     //Call in to BrowserAppcore to stop the current loading
     appCore.stop();
  }
 

  function BrowserReallyReload(reloadType) {
     // Get a handle to the "canReload" broadcast id
     var reloadBElem = document.getElementById("canReload");
     if (!reloadBElem) {
        dump("Couldn't obtain handle to reload Broadcast element\n");
        return;
	   }

     var canreload = reloadBElem.getAttribute("disabled");
     

     // If the reload button is currently disabled, just return
     if ( canreload) {
	     return;
     }
	
     //Call in to BrowserAppcore to reload the current loading
     if (appCore != null) {
        dump("Going to reload\n");
        appCore.reload(reloadType);
     } else {
        dump("BrowserAppCore has not been created!\n");
     }
  }

  function BrowserHome()
  {
   // this eventual calls nsGlobalWIndow::Home()
   window.content.home();
   RefreshUrlbar();
  }

  function updateGoMenu(event)
  {
      dump("In updategomenu\n");
      if (appCore)
	     appCore.updateGoMenu(event.target);
	  else
	    dump("BrowserAppCore has not been created!\n");

  }


  function setKeyword(index)
  {
      urlbar = document.getElementById('urlbar');
	  if (!urlbar)
	    return;
      switch(index) {
          case 0:		    
	    	urlbar.focus();
			urlbar.value = bundle.GetStringFromName("quoteKeyword");
			urlbar.setSelectionRange(14,33);
			break;
          case 1:
             urlbar.focus();
		     urlbar.value = bundle.GetStringFromName("localKeyword");
			 urlbar.setSelectionRange(12,27);			 
			 break;
          case 2:
	         urlbar.focus();
		     urlbar.value = bundle.GetStringFromName("shopKeyword");
			 urlbar.setSelectionRange(13,22);
			 break;
		  case 3:
		     urlbar.focus();
		     urlbar.value = bundle.GetStringFromName("careerKeyword");
			 urlbar.setSelectionRange(8,19);
			 break;
		  case 4:
			 if (appCore)
			   appCore.loadUrl(bundle.GetStringFromName("webmailKeyword"));
			 else
			    dump("Couldn't find instance of BrowserAppCore\n");
			 break;
		  
		  case 5:
		     if (appCore)
			   appCore.loadUrl(bundle.GetStringFromName("keywordList"));
			 else
			    dump("Couldn't find instance of BrowserAppCore\n");   
			 break;
		     break;		 
	  }

  }

  function OpenBookmarkURL(node, datasources)
  {
    if (node.getAttribute('container') == "true") {
      return false;
    }

	var url = node.getAttribute('id');
	try
	{
		// add support for IE favorites under Win32, and NetPositive URLs under BeOS
		var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
		if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
		if (rdf && datasources)
		{
			var src = rdf.GetResource(url, true);
			var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
			var target = datasources.GetTarget(src, prop, true);
			if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
			if (target)	target = target.Value;
			if (target)	url = target;
		}
	}
	catch(ex)
	{
	}

    // Ignore "NC:" urls.
    if (url.substring(0, 3) == "NC:") {
      return false;
    }
	// Check if we have a browser window
	if ( window.content == null )
	{
		window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url ); 
	}
	else
	{
  	  window.content.location.href = url;
    	RefreshUrlbar();
  	}
  }

function OpenSearch(tabName, forceDialogFlag, searchStr)
{
	var searchMode = 0;
	var searchEngineURI = null;
	var autoOpenSearchPanel = false;
	var defaultSearchURL = null;
	var fallbackDefaultSearchURL = "http://search.netscape.com/cgi-bin/search?search="
	try
	{
		searchMode = pref.GetIntPref("browser.search.powermode");
		autoOpenSearchPanel = pref.GetBoolPref("browser.search.opensidebarsearchpanel");
		searchEngineURI = pref.CopyCharPref("browser.search.defaultengine");
		defaultSearchURL = pref.CopyCharPref("browser.search.defaulturl");
		
	}
	catch(ex)
	{
	}
    dump("Search defaultSearchURL: " + defaultSearchURL + "\n");
	if ((defaultSearchURL == null) || (defaultSearchURL == ""))
	{
		// Fallback to a Netscape default (one that we can get sidebar search results for)
		defaultSearchURL = fallbackDefaultSearchURL;
	}
    dump("This is before the search " + window.content.location.href + "\n");
	dump("This is before the search " + searchStr + "\n");
	if ((window.content.location.href == searchStr) || (searchStr == '')) 
	{
		if (!(defaultSearchURL == fallbackDefaultSearchURL)) {
			window.content.location.href = defaultSearchURL;
		}
		else
		{
			window.content.location.href = "http://search.netscape.com/"
		}
	}
	else
	{
		if ((searchMode == 1) || (forceDialogFlag == true))
		{
			// Use a single search dialog
			var cwindowManager = Components.classes["component://netscape/rdf/datasource?name=window-mediator"].getService();
			var iwindowManager = Components.interfaces.nsIWindowMediator;
			var windowManager  = cwindowManager.QueryInterface(iwindowManager);
			var searchWindow = windowManager.getMostRecentWindow("search:window");
			if (searchWindow)
			{
				searchWindow.focus();
				if (searchWindow.loadPage)	searchWindow.loadPage(tabName, searchStr);
			}
			else
			{
				window.openDialog("chrome://communicator/content/search/search.xul", "SearchWindow", "dialog=no,close,chrome,resizable", tabName, searchStr);
			}
		}
		else 
		{  
			if ((!searchStr) || (searchStr == ""))	return;

			var searchDS = Components.classes["component://netscape/rdf/datasource?name=internetsearch"].getService();
			if (searchDS)	searchDS = searchDS.QueryInterface(Components.interfaces.nsIInternetSearchService);

			var	escapedSearchStr = escape(searchStr);
			defaultSearchURL += escapedSearchStr;

			if (searchDS)
			{
				searchDS.RememberLastSearchText(escapedSearchStr);

				if ((searchEngineURI != null) && (searchEngineURI != ""))
				{
					try
					{
						var	searchURL = searchDS.GetInternetSearchURL(searchEngineURI, escapedSearchStr);
						if ((searchURL != null) && (searchURL != ""))
						{
							defaultSearchURL = searchURL;
						}
					}
					catch(ex)
					{
					}
				}
			}  
			window.content.location.href = defaultSearchURL;
		}
	}

	// should we try and open up the sidebar to show the "Search Results" panel?
	if (autoOpenSearchPanel == true)
	{
		RevealSearchPanel();
	}
}

function RevealSearchPanel()
{
	// rjc Note: the following is all a hack until the sidebar has appropriate APIs
	// to check whether its shown/hidden, open/closed, and can show a particular panel

	var sidebar = document.getElementById("sidebar-box");
	var sidebar_splitter = document.getElementById("sidebar-splitter");
	var searchPanel = document.getElementById("urn:sidebar:panel:search");

	if (sidebar && sidebar_splitter && searchPanel)
	{
		var is_hidden = sidebar.getAttribute("hidden");
		if (is_hidden && is_hidden == "true")
		{
			// SidebarShowHide() lives in sidebarOverlay.js
			SidebarShowHide();
		}
		var splitter_state = sidebar_splitter.getAttribute("state");
		if (splitter_state && splitter_state == "collapsed") {
			sidebar_splitter.removeAttribute("state");
		}
        // SidebarSelectPanel() lives in sidebarOverlay.js
		SidebarSelectPanel(searchPanel);
	}
}

  function BrowserNewWindow()
  {
    OpenBrowserWindow();
  }

  function BrowserEditPage(url)
  {
    window.openDialog( "chrome://editor/content", "_blank", "chrome,all,dialog=no", url );
  }

//Note: BrowserNewEditorWindow() was moved to globalOverlay.xul and renamed to NewEditorWindow()
  
  function BrowserOpenWindow()
  {
    //opens a window where users can select a web location to open
    window.openDialog( "chrome://navigator/content/openLocation.xul", "_blank", "chrome,modal", appCore );
  }

  /* Called from the openLocation dialog. This allows that dialog to instruct
     its opener to open a new window and then step completely out of the way.
     Anything less byzantine is causing horrible crashes, rather believably,
     though oddly only on Linux. */
  function delayedOpenWindow(chrome,flags,url) {
    setTimeout("window.openDialog('"+chrome+"','_blank','"+flags+"','"+url+"')", 10);
  }
  
  function createInstance( progid, iidName ) {
      var iid = Components.interfaces[iidName];
      return Components.classes[ progid ].createInstance( iid );
  }

  function createInstanceById( cid, iidName ) {
      var iid = Components.interfaces[iidName];
      return Components.classesByID[ cid ].createInstance( iid );
  }

  function getService( progid, iidName ) {
      var iid = Components.interfaces[iidName];
      return Components.classes[ progid ].getService( iid );
  }

  function getServiceById( cid, iidName ) {
      var iid = Components.interfaces[iidName];
      return Components.classesByID[ cid ].getService( iid );
  }

  function openNewWindowWith( url ) {
    var newWin = window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url );

    // Fix new window.    
    newWin.saveFileAndPos = true;
  }
  
  const nsIFilePicker = Components.interfaces.nsIFilePicker;
  function BrowserOpenFileWindow()
  {
    // Get filepicker component.
    try {
      var fp = Components.classes["component://mozilla/filepicker"].createInstance(nsIFilePicker);
      fp.init(window, bundle.GetStringFromName("openFile"), nsIFilePicker.modeOpen);
      fp.setFilters(nsIFilePicker.filterAll);
      if (fp.show() == nsIFilePicker.returnOK) {
        openNewWindowWith(fp.fileURL.spec);
      }
    } catch (ex) { }
  }

  function OpenFile(url) {
    // Obsolete (called from C++ code that is no longer called).
    dump( "OpenFile called?\n" );
    openNewWindowWith( url );
  }

  function BrowserCopy()
  {
    if (appCore != null) {
	    dump("Copying\n");
      appCore.copy();
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }


  function BrowserAddBookmark(url,title)
  {
    var bmks = Components.classes["component://netscape/browser/bookmarks-service"].getService();
    bmks = bmks.QueryInterface(Components.interfaces.nsIBookmarksService);
    if ((title == null) || (title == ""))
    {
    	title = url;
    }
    bmks.AddBookmark(url, title);
  }

// Set up a lame hack to avoid opening two bookmarks.
// Could otherwise happen with two Ctrl-B's in a row.
var gDisableBookmarks = false;
function enableBookmarks() {
  gDisableBookmarks = false;
}

function BrowserEditBookmarks()
{ 
  // Use a single sidebar bookmarks dialog

  var cwindowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
  var iwindowManager = Components.interfaces.nsIWindowMediator;
  var windowManager  = cwindowManager.QueryInterface(iwindowManager);

  var bookmarksWindow = windowManager.getMostRecentWindow('bookmarks:manager');

  if (bookmarksWindow) {
    //debug("Reuse existing bookmarks window");
    bookmarksWindow.focus();
  } else {
    //debug("Open a new bookmarks dialog");

    if (true == gDisableBookmarks) {
      //debug("Recently opened one. Wait a little bit.");
      return;
    }
    gDisableBookmarks = true;

    window.open("chrome://communicator/content/bookmarks/bookmarks.xul", "_blank", "chrome,menubar,resizable,scrollbars");
    setTimeout(enableBookmarks, 2000);
  }
}

  function BrowserPrintPreview()
  {
    // Borrowing this method to show how to 
    // dynamically change icons
    dump("BrowserPrintPreview\n");
    if (appCore != null) {
	    dump("Changing Icons\n");
      appCore.printPreview();
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserPrint()
  {
    // Borrowing this method to show how to 
    // dynamically change icons
    if (appCore != null) {
      appCore.print();
    }
  }

  function BrowserChangeTextZoom(aChange)
  {
    if (appCore != null) {
      textZoom += (aChange * 0.1);
      if (textZoom < 0.1) {
        textZoom = 0.1;
      }
      appCore.setTextZoom(textZoom);
    }
  }

  function BrowserSetDefaultCharacterSet(aCharset)
  {
    if (appCore != null) {
      appCore.SetDocumentCharset(aCharset);
      window.content.location.reload();
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserClose()
  {
    // This code replicates stuff in Shutdown().  It is here because
    // window.screenX and window.screenY have real values.  We need
    // to fix this eventually but by replicating the code here, we
    // provide a means of saving position (it just requires that the
    // user close the window via File->Close (vs. close box).

    // Get the current window position/size.
    var x = window.screenX;
    var y = window.screenY;
    var h = window.outerHeight;
    var w = window.outerWidth;

    // Store these into the window attributes (for persistence).
    var win = document.getElementById( "main-window" );
    win.setAttribute( "x", x );
    win.setAttribute( "y", y );
    win.setAttribute( "height", h );
    win.setAttribute( "width", w );

  	 window.close();
  }



  function BrowserSelectAll() {
    if (appCore != null) {
        appCore.selectAll();
    } else {
        dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserFind() {
    if (appCore != null) {
        appCore.find();      
    } else {
        dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserFindAgain() {
    if (appCore != null) {
        appCore.findNext();      
    } else {
        dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserLoadURL()
  {
	if (appCore == null)
	{
		dump("BrowserAppCore has not been initialized\n");
		return;
	}

    // rjc: added support for URL shortcuts (3/30/1999)
    try
    {
      var bmks = Components.classes["component://netscape/browser/bookmarks-service"].getService();
      bmks = bmks.QueryInterface(Components.interfaces.nsIBookmarksService);

      var text = document.getElementById('urlbar').value;
      var shortcutURL = bmks.FindShortcut(text);
      if ((shortcutURL == null) || (shortcutURL == ""))
      {
        // rjc: add support for string substitution with shortcuts (4/4/2000)
        //      (see bug # 29871 for details)
      	var aOffset = text.indexOf(" ");
      	if (aOffset > 0)
      	{
      	  var cmd = text.substr(0, aOffset);
      	  var text = text.substr(aOffset+1);
          var shortcutURL = bmks.FindShortcut(cmd);
          if ((shortcutURL) && (shortcutURL != "") && (text != ""))
          {
            aOffset = shortcutURL.indexOf("%s");
            if (aOffset >= 0)
            {
              shortcutURL = shortcutURL.substr(0, aOffset) + text + shortcutURL.substr(aOffset+2);
            }
            else
            {
              shortcutURL = null;
            }
          }      	  
      	}
      }
      if ((shortcutURL != null) && (shortcutURL != ""))
      {
        document.getElementById('urlbar').value = shortcutURL;
      }
    }
    catch (ex)
    {
      // stifle any exceptions so we're sure to load the URL.
    }

    appCore.loadUrl(document.getElementById('urlbar').value);
  }

  function readFromClipboard()
  {
    // Get clipboard.
    var clipboard = Components
                      .classes["component://netscape/widget/clipboard"]
                        .getService ( Components.interfaces.nsIClipboard );
    // Create tranferable that will transfer the text.
    var trans = Components
                   .classes["component://netscape/widget/transferable"]
                     .createInstance( Components.interfaces.nsITransferable );
    if ( !clipboard || !trans )
      return;

    trans.addDataFlavor( "text/unicode" );
    clipboard.getData(trans, clipboard.kSelectionClipboard);

	var data = new Object();
	var dataLen = new Object();
	trans.getTransferData("text/unicode", data, dataLen);
    var url = null;
	if (data)
    {
      data = data.value.QueryInterface(Components.interfaces
                                                    .nsISupportsWString);
      url = data.data.substring(0, dataLen.value / 2);
    }
	return url;
  }

  function enclosingLink(node)
  {
    while (node)
    {
      var nodeName = node.nodeName;
      if (nodeName == "")
        return "";
      nodeName = nodeName.toLowerCase();
      if (nodeName == "" || nodeName == "body"
          || nodeName == "html" || nodeName == "#document")
        return "";
      var href = node.href;
      if (nodeName == "a" && href != "")
        return href;
      node = node.parentNode;
    }
    return "";
  }

  function browserHandleMiddleClick(event)
  {
    var target = event.target;
    if (pref.GetBoolPref("middlemouse.openNewWindow"))
    {
      var href = enclosingLink(target);
      if (href != "")
      {
        openNewWindowWith(href);
        event.preventBubble();
        return;
      }
    }

    if (pref.GetBoolPref("middlemouse.paste"))
    {
      var tagName = target.tagName;
      if (tagName) tagName = tagName.toLowerCase();
      var type = target.type;
      if (type) type = type.toLowerCase();
      if (!((tagName == "input"
             && (type == "" || type == "text" || type == "password"))
            || tagName == "textarea"))
      {
        var url = readFromClipboard();
        //dump ("Loading URL on clipboard: '" + url + "'; length = " + url.length + "\n");
        if (url.length > 0)
        {
          var urlBar = document.getElementById("urlbar");
          urlBar.value = url;
          BrowserLoadURL();
          event.preventBubble();
        }
      }
    }
  }

  function OpenMessenger()
  {
	window.open("chrome://messenger/content/messenger.xul", "_blank", "chrome,menubar,toolbar,resizable");
  }

  function OpenAddressbook()
  {
	window.open("chrome://messenger/content/addressbook/addressbook.xul", "_blank", "chrome,menubar,toolbar,resizable");
  }

  function BrowserSendLink(pageUrl, pageTitle)
  {
    window.openDialog( "chrome://messenger/content/messengercompose/messengercompose.xul", "_blank", 
                       "chrome,all,dialog=no",
                       "body='" + pageUrl + "',subject='" + pageTitle +
                       "',bodyislink=true");
  }

  function BrowserSendPage(pageUrl, pageTitle)
  {
    window.openDialog( "chrome://messenger/content/messengercompose/messengercompose.xul", "_blank", 
                       "chrome,all,dialog=no", 
                       "attachment='" + pageUrl + "',body='" + pageUrl +
                       "',subject='" + pageTitle + "',bodyislink=true");
  }

  function BrowserViewSource()
  {
	dump("BrowserViewSource(); \n ");
   window.openDialog( "chrome://navigator/content/viewSource.xul",
							 "_blank",
							 "chrome,dialog=no",
							 window.content.location);
  }


        function doTests() {
        }

        function dumpProgress() {
            var meter       = document.getElementById("statusbar-icon");
            dump( "meter mode=" + meter.getAttribute("mode") + "\n" );
            dump( "meter value=" + meter.getAttribute("value") + "\n" );
        }

function BrowserReload() {
    dump( "Sorry, command not implemented.\n" );
}

function hiddenWindowStartup()
{
	// Disable menus which are not appropriate
	var disabledItems = ['cmd_close', 'Browser:SendPage', 'Browser:EditPage', 'Browser:PrintSetup', 'Browser:PrintPreview',
						 'Browser:Print', 'canGoBack', 'canGoForward', 'Browser:Home', 'Browser:AddBookmark', 'cmd_undo', 
						 'cmd_redo', 'cmd_cut', 'cmd_copy','cmd_paste', 'cmd_delete', 'cmd_selectAll'];
	for ( id in disabledItems )
	{
         // dump("disabling "+disabledItems[id]+"\n");
		 var broadcaster = document.getElementById( disabledItems[id]);
		 if (broadcaster)
           broadcaster.setAttribute("disabled","true");
	}
}

// Tile
function TileWindow()
{
	var xShift = 25;
	var yShift = 50;
	var done = false;
	var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
	dump("got window Manager \n");
	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
	
	var enumerator = windowManagerInterface.getEnumerator( null );
	
	var xOffset = screen.availLeft;
	var yOffset = screen.availRight;
	do
	{
		var currentWindow = windowManagerInterface.convertISupportsToDOMWindow ( enumerator.GetNext() );
		if ( currentWindow.screenX == screenX && currentWindow.screenY == screenY )
		{
			alreadyThere = true;
			break;
		}	
	} while ( enumerator.HasMoreElements() )
	
	if ( alreadyThere )
	{
		enumerator = windowManagerInterface.getEnumerator( null );
		do
		{
			var currentWindow = windowManagerInterface.convertISupportsToDOMWindow ( enumerator.GetNext() );
			if ( currentWindow.screenX == screenX+xOffset*xShift+yOffset*xShift   && currentWindow.screenY == screenY+yShift*xOffset && window != currentWindow )
			{
				xOffset++;
				if ( (screenY+outerHeight  < screen.availHeight) && (screenY+outerHeight+yShift*xOffset > screen.availHeight ) )
				{
					dump(" increment yOffset");
					yOffset++;
					xOffset = 0;
				}
				enumerator = windowManagerInterface.getEnumerator( null );
			}	
		} while ( enumerator.HasMoreElements() )
	}
	
	if ( xOffset > 0 || yOffset >0 )
	{
		dump( "offsets:"+xOffset+" "+yOffset+"\n");
		dump("Move by ("+ xOffset*xShift + yOffset*xShift +","+ yShift*xOffset +")\n");
		moveBy( xOffset*xShift + yOffset*xShift, yShift*xOffset );
	}
}
// Make sure that a window fits fully on the screen. Will move to preserve size, and then shrink to fit
function FitToScreen()
{
	var moveX = screenX;
	var sizeX = outerWidth;
	var moveY = screenY;
	var sizeY = outerHeight;
	
	dump( " move to ("+moveX+","+moveY+") size to ("+sizeX+","+sizeY+") \n");
	var totalWidth = screenX+outerWidth;
	if ( totalWidth > screen.availWidth )
	{	
		if( outerWidth > screen.availWidth )
		{
			sizeX = screen.availWidth;
			moveX = screen.availLeft;
		}
		else
		{
			moveX = screen.availWidth- outerWidth;
		}
	}
	
	var totalHeight = screenY+outerHeight;
	if ( totalHeight > screen.availHeight )
	{	
		if( outerWidth > screen.availHeight )
		{
			sizeY = screen.availHeight;
			moveY = screen.availTop;
		}
		else
		{
			moveY = screen.availHeight- outerHeight;
		}
	}
	
	
	dump( " move to ("+moveX+","+moveY+") size to ("+sizeX+","+sizeY+") \n");
	if ( (moveY- screenY != 0 ) ||	(moveX-screenX != 0 ) )
		moveTo( moveX,moveY );
	
	// Maintain a minimum size
	if ( sizeY< 100 )
		sizeY = 100;
	if ( sizeX < 100 )
		sizeX = 100; 
	if ( (sizeY- outerHeight != 0 ) ||	(sizeX-outerWidth != 0 ) )
	{
		//outerHeight = sizeY;
		//outerWidth = sizeX;
		resizeTo( sizeX,sizeY );	
	}
}

// Dumps all properties of anObject.
function dumpObject( anObject, prefix ) {
    if ( prefix == null ) {
        prefix = anObject;
    }
    for ( prop in anObject ) {
        dump( prefix + "." + prop + " = " + anObject[prop] + "\n" );
    }
}

// Takes JS expression and dumps "expr="+expr+"\n"
function dumpExpr( expr ) {
    dump( expr+"="+eval(expr)+"\n" );
}

var leakDetector = null;

// Dumps current set of memory leaks.
function dumpMemoryLeaks() {
	if (leakDetector == null)
		leakDetector = createInstance("component://netscape/xpcom/leakdetector", "nsILeakDetector");
	if (leakDetector != null)
		leakDetector.dumpLeaks();
}
