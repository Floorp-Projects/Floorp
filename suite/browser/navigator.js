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
 *  Blake Ross <BlakeR1234@aol.com>
 *  Peter Annema <disttsc@bart.nl>
 */

var pref = null;
var bundle = srGetStrBundle("chrome://navigator/locale/navigator.properties");
var brandBundle = srGetStrBundle("chrome://global/locale/brand.properties");

// in case we fail to get the start page, load this
var startPageDefault = "about:blank";

// in case we fail to get the home page, load this
var homePageDefault = bundle.GetStringFromName( "homePageDefault" );

try
{
	pref = Components.classes["component://netscape/preferences"];
	if (pref)	pref = pref.getService();
	if (pref)	pref = pref.QueryInterface(Components.interfaces.nsIPref);
}
catch (ex)
{
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
  var startTime = (new Date()).getTime();

  //cached elements/ fields
  var statusTextFld = null;
  var statusMeter = null;
  var throbberElement = null;
  var stopButton = null;
  var stopMenu = null;
  var stopContext = null;
  var locationFld = null;
  var backButton	= null;
  var forwardButton = null;

  var useRealProgressFlag = false;
  var totalRequests = 0;
  var finishedRequests = 0;
///////////////////////////// DOCUMENT SAVE ///////////////////////////////////

// focused frame URL
var gFocusedURL = null;

/**
 * Save the document at a given location to disk
 **/  
function savePage( url ) 
{
  // Default is to save current page.
  url = url ? url : window._content.location.href;
  // Use stream xfer component to prompt for destination and save.
  var xfer = getService("component://netscape/appshell/component/xfer", "nsIStreamTransfer");
  try {
    // When Necko lands, we need to receive the real nsIChannel and
    // do SelectFileAndTransferLocation!
    // Use this for now...
    xfer.SelectFileAndTransferLocationSpec( url, window, "", "" );
  } 
  catch( exception ) { 
    // suppress NS_ERROR_ABORT exceptions for cancellation 
  }
}

/** 
 * Determine whether or not the content area is displaying a page with frames,
 * and if so, toggle the display of the 'save frame as' menu item.
 **/
function getContentAreaFrameCount()
{
  var saveFrameItem = document.getElementById("savepage");
  if (!window._content.frames.length ||
      !isDocumentFrame(document.commandDispatcher.focusedWindow))
    saveFrameItem.setAttribute("hidden", "true");
  else 
    saveFrameItem.removeAttribute("hidden");
}

/**
 * When a content area frame is focused, update the focused frame URL 
 **/
function contentAreaFrameFocus()
{
  var saveFrameItem = document.getElementById("savepage");
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (isDocumentFrame(focusedWindow)) {
    gFocusedURL = focusedWindow.location.href;
    saveFrameItem.removeAttribute("hidden");
  }
}

/** 
 * Determine whether or not a given focused DOMWindow is in the content 
 * area.
 **/   
function isDocumentFrame(aFocusedWindow)
{
  var contentFrames = window._content.frames;
  if (contentFrames.length) {
    for (var i = 0; i < contentFrames.length; i++ ) {
      if (aFocusedWindow == contentFrames[i])
        return true;
    }
  }
  return false;
}

//////////////////////////////// BOOKMARKS ////////////////////////////////////

function UpdateBookmarksLastVisitedDate(event)
{
	if ((window._content.location.href) && (window._content.location.href != ""))
	{
		try
		{
			// if the URL is bookmarked, update its "Last Visited" date
			var bmks = Components.classes["component://netscape/browser/bookmarks-service"].getService();
			if (bmks)	bmks = bmks.QueryInterface(Components.interfaces.nsIBookmarksService);
			if (bmks)	bmks.UpdateBookmarkLastVisitedDate(window._content.location.href, window._content.document.characterSet);
		}
		catch(ex)
		{
			dump("failed to update bookmark last visited date.\n");
		}
	}
}

function UpdateInternetSearchResults(event)
{
	if ((window._content.location.href) && (window._content.location.href != ""))
	{
		var	searchInProgressFlag = false;

		try
		{
			var search = Components.classes["component://netscape/rdf/datasource?name=internetsearch"].getService();
			if (search)	search = search.QueryInterface(Components.interfaces.nsIInternetSearchService);
			if (search)	searchInProgressFlag = search.FindInternetSearchResults(window._content.location.href);
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
		statusTextFld = document.getElementById("statusbar-display");

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

    if (!useRealProgressFlag && (channel != null)) {
      return;
    }

    if (max > 0) {
      percentage = (current * 100) / max ;
      if (statusMeter.getAttribute("mode") != "normal")     
        statusMeter.setAttribute("mode", "normal");
      statusMeter.value = percentage;
      statusMeter.progresstext = Math.round(percentage) + "%";
    }
    else {
      if (statusMeter.getAttribute("mode") != "undetermined")
        statusMeter.setAttribute("mode","undetermined");
    }
  },
  onStateChange : function(channel, state)
  {
    if(!throbberElement)
      throbberElement = document.getElementById("navigator-throbber");
    if(!statusMeter)
      statusMeter = document.getElementById("statusbar-icon");
    if(!stopButton)
      stopButton = document.getElementById("stop-button");
    if(!stopMenu)
      stopMenu = document.getElementById("menuitem-stop");
    if(!stopContext)
      stopContext = document.getElementById("context-stop");

    if (state & Components.interfaces.nsIWebProgressListener.STATE_START) {
      if(state & Components.interfaces.nsIWebProgressListener.STATE_IS_NETWORK) {
        // Remember when loading commenced.
        startTime = (new Date()).getTime();
        // Turn progress meter on.
        statusMeter.setAttribute("mode","undetermined");
        throbberElement.setAttribute("busy", true);

        // XXX: These need to be based on window activity...    
        stopButton.setAttribute("disabled", false);
        stopMenu.setAttribute("disabled", false);
        stopContext.setAttribute("disabled", false);

        // Initialize the progress stuff...
        statusMeter.setAttribute("mode","undetermined");
        useRealProgressFlag = false;
        totalRequests = 0;
        finishedRequests = 0;
      }
      if (state & Components.interfaces.nsIWebProgressListener.STATE_IS_REQUEST) {
        totalRequests += 1;
      }
      EnableBusyCursor(throbberElement.getAttribute("busy") == "true");
    }
    else if (state & Components.interfaces.nsIWebProgressListener.STATE_STOP) {
      if (state & Components.interfaces.nsIWebProgressListener.STATE_IS_REQUEST) {
        finishedRequests += 1;
        if (!useRealProgressFlag) {
          this.onProgress(null, finishedRequests, totalRequests);
        }
      }
      if(state & Components.interfaces.nsIWebProgressListener.STATE_IS_NETWORK) {
        // Record page loading time.
        var elapsed = ( (new Date()).getTime() - startTime ) / 1000;
        var msg = bundle.GetStringFromName("nv_done");
        msg = msg.replace(/%elapsed%/, elapsed);
        defaultStatus = msg;
        UpdateStatusField();
        //window.XULBrowserWindow.setDefaultStatus(msg);
        //this.setDefaultStatus(msg);
        this.setOverLink(msg);
        // Turn progress meter off.
        statusMeter.setAttribute("mode","normal");
        statusMeter.value = 0;  // be sure to clear the progress bar
        statusMeter.progresstext = "";
        throbberElement.setAttribute("busy", false);

        // XXX: These need to be based on window activity...
        stopButton.setAttribute("disabled", true);
        stopMenu.setAttribute("disabled", true);
        stopContext.setAttribute("disabled", true);

        EnableBusyCursor(false);
      }
    }
    else if (state & Components.interfaces.nsIWebProgressListener.STATE_TRANSFERRING) {
      if (state & Components.interfaces.nsIWebProgressListener.STATE_IS_DOCUMENT) {
        var ctype=channel.contentType;
  
        if (ctype != "text/html") {
          useRealProgressFlag = true;
        }
        statusMeter.setAttribute("mode", "normal");
      }
      if (state & Components.interfaces.nsIWebProgressListener.STATE_IS_REQUEST) {
        if (!useRealProgressFlag) {
          this.onProgress(null, finishedRequests, totalRequests);
        }
      }
    }

  },
  onLocationChange : function(location)
  {
    if(!locationFld)
      locationFld = document.getElementById("urlbar");

    // We should probably not do this if the value has changed since the user 
    // searched
    locationFld.value = location;

    UpdateBackForwardButtons();
  },
  onStatus : function(channel, status, msg)
  {
    this.setOverLink(msg);
    //this.setDefaultStatus(msg);
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
    
	window._content.appCore= appCore;
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

    // set the offline/online mode
    setOfflineStatus();
    
    tryToSetContentWindow();
	
    // Add a capturing event listener to the content area
    // (rjc note: not the entire window, otherwise we'll get sidebar pane loads too!)
    //  so we'll be notified when onloads complete.
    var contentArea = document.getElementById("appcontent");
    if (contentArea)
    {
	    contentArea.addEventListener("load", UpdateBookmarksLastVisitedDate, true);
	    contentArea.addEventListener("load", UpdateInternetSearchResults, true);
      contentArea.addEventListener("load", checkForDirectoryListing, true);
      contentArea.addEventListener("load", getContentAreaFrameCount, true);
      contentArea.addEventListener("focus", contentAreaFrameFocus, true);
      contentArea.addEventListener("load",postURLToNativeWidget, true);
    }

    dump("*** Pulling out the charset\n");
    if ( window.arguments && window.arguments[1] ) {
        if (window.arguments[1].indexOf('charset=') != -1) {
              arrayArgComponents = window.arguments[1].split('=');
            if (arrayArgComponents) {
                if (appCore != null) {
                 //we should "inherit" the charset menu setting in a new window
                  appCore.SetDocumentCharset(arrayArgComponents[1]);
                } 
                dump("*** SetDocumentCharset(" + arrayArgComponents[1] + ")\n");
            }
        }
    }

    try
    {
    	var searchMode = pref.GetIntPref("browser.search.mode");
    	setBrowserSearchMode(searchMode);
    }
    catch(ex)
    {
    }

	 // Check for window.arguments[0].  If present, go to that url.
    if ( window.arguments && window.arguments[0] ) {
        // Load it using yet another psuedo-onload handler.
        onLoadViaOpenDialog();
    }
    gURLBar = document.getElementById("urlbar");
    
    // set home button tooltip text
    var homepage; 
    try {
      homepage = pref.getLocalizedUnicharPref("browser.startup.homepage");
    }
    catch(e) {
      homepage = null;
    }
    if (homepage)
      setTooltipText("homebutton", homepage);

    initConsoleListener();
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
    if ( window._content ) {
        dump("Setting content window\n");
        appCore.setContentWindow( window._content );
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
	var targetURI = window._content.location.href;
	var targetURIIndex = targetURI.indexOf("AlisTargetURI=");
	if (targetURIIndex >= 0)
	{
		targetURI = targetURI.substring(targetURIIndex + 14);
	}
	service += "&AlisTargetURI=" + escape(targetURI);

	//window._content.location.href = service;
	if (appCore)
	  appCore.loadUrl(service);
    else
	  dump("BrowserAppCore is not initialised\n");
  }

  function RefreshUrlbar()
  {
   //Refresh the urlbar bar
    document.getElementById('urlbar').value = window._content.location.href;
  }

function gotoHistoryIndex( aEvent )
  {
	  var index = aEvent.target.getAttribute("index");
	  if (index) 
      {
        appCore.gotoHistoryIndex(index);
	      return true;
	    }
	  else 
      {
	      var id = aEvent.target.getAttribute("id");
	      if (id == "menuitem-back")
	        BrowserBack();
	      else if (id == "menuitem-forward")
	        BrowserForward();
      }
  }

function BrowserBack()
  {
    appCore.back();
		UpdateBackForwardButtons();
  }


function BrowserForward()
  {
    appCore.forward();
    UpdateBackForwardButtons();
  }

function BrowserBackMenu(event)
  {
    FillHistoryMenu(event.target, "back");
  }


function BrowserForwardMenu(event)
  {
    FillHistoryMenu(event.target, "forward");
  }



function BrowserStop() 
  {
    appCore.stop();
  }
 

function BrowserReallyReload(event) 
  {
    var nsIWebNavigation = Components.interfaces.nsIWebNavigation;
    // Default is loadReloadNormal.
    var reloadType = nsIWebNavigation.loadReloadNormal;
    // See if the event was a shift-click.
    if ( event.shiftKey ) {
        // Shift key means loadReloadBypassProxyAndCache.
        reloadType = nsIWebNavigation.loadReloadBypassProxyAndCache;
    }
    appCore.reload(reloadType);
  }

function BrowserHome()
  {
   window._content.home();
   RefreshUrlbar();
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
	if ( window._content == null )
	{
		window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url ); 
	}
	else
	{
  	  //window._content.location.href = url;
	  if (appCore)
	     appCore.loadUrl(url);
	  else
	     dump("BrowserAppCore is not initialised\n");
    	RefreshUrlbar();
  	}
  }

function OpenSearch(tabName, forceDialogFlag, searchStr)
{
	var searchMode = 0;
	var searchEngineURI = null;
	var autoOpenSearchPanel = false;
	var defaultSearchURL = null;
	var fallbackDefaultSearchURL = bundle.GetStringFromName("fallbackDefaultSearchURL")

	try
	{
		searchMode = pref.GetIntPref("browser.search.powermode");
		autoOpenSearchPanel = pref.GetBoolPref("browser.search.opensidebarsearchpanel");
		searchEngineURI = pref.CopyCharPref("browser.search.defaultengine");
		defaultSearchURL = pref.getLocalizedUnicharPref("browser.search.defaulturl");
		
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
    dump("This is before the search " + window._content.location.href + "\n");
	dump("This is before the search " + searchStr + "\n");
	if ((window._content.location.href == searchStr) || (searchStr == '')) 
	{
		if (!(defaultSearchURL == fallbackDefaultSearchURL)) {
		//	window._content.location.href = defaultSearchURL;
		// Call in to BrowserAppCore instead of replacing 
		// the url in the content area so that B/F buttons work right
		    if (appCore)
			  appCore.loadUrl(defaultSearchURL);
			else
			  dump("BrowserAppCore is not initialised\n");
		}
		else
		{
			//window._content.location.href = "http://search.netscape.com/"
			// Call in to BrowserAppCore instead of replacing 
		    // the url in the content area so that B/F buttons work right
			if (appCore)
			   appCore.loadUrl("http://search.netscape.com/");
			else
			   dump("BrowserAppCore is not initialised\n");
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
		//	window._content.location.href = defaultSearchURL
		// Call in to BrowserAppCore instead of replacing 
		// the url in the content area so that B/F buttons work right
		    if (appCore)
			   appCore.loadUrl(defaultSearchURL);
			else
			   dump("BrowserAppCore is not initialised\n");
		}
	}

	// should we try and open up the sidebar to show the "Search Results" panel?
	if (autoOpenSearchPanel == true)
	{
		RevealSearchPanel();
	}
}

function setBrowserSearchMode(searchMode)
{
	// set search mode preference
	try
	{
		pref.SetIntPref("browser.search.mode", searchMode);
	}
	catch(ex)
	{
	}

	// update search menu
	var simpleMenuItem = document.getElementById("simpleSearch");
	if (simpleMenuItem)
	{
		simpleMenuItem.setAttribute("checked", (searchMode == 0) ? "true" : "false");
	}
	var advancedMenuItem = document.getElementById("advancedSearch");
	if (advancedMenuItem)
	{
		advancedMenuItem.setAttribute("checked", (searchMode == 1) ? "true" : "false");
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
    window.openDialog( "chrome://navigator/content/openLocation.xul", "_blank", "chrome,modal,titlebar", appCore );
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
    var newWin;
    var wintype = document.firstChild.getAttribute('windowtype');
    
    // if and only if the current window is a browser window and it has a document with a character
    // set, then extract the current charset menu setting from the current document and use it to
    // initialize the new browser window...
    if (window && (wintype == "navigator:browser") && window._content && window._content.document)
    {
      var DocCharset = window._content.document.characterSet;
      charsetArg = "charset="+DocCharset;
      dump("*** Current document charset: " + DocCharset + "\n");

      //we should "inherit" the charset menu setting in a new window
      newWin = window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url, charsetArg );
    }
    else // forget about the charset information.
    {
      newWin = window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url );
    }

    // Fix new window.    
    newWin.saveFileAndPos = true;
  }
  
  function BrowserOpenFileWindow()
  {
    // Get filepicker component.
    try {
      var nsIFilePicker = Components.interfaces.nsIFilePicker;
      var fp = Components.classes["component://mozilla/filepicker"].createInstance(nsIFilePicker);
      fp.init(window, bundle.GetStringFromName("openFile"), nsIFilePicker.modeOpen);
      fp.appendFilters(nsIFilePicker.filterHTML | nsIFilePicker.filterText | 
			nsIFilePicker.filterAll | nsIFilePicker.filterImages | nsIFilePicker.filterXML);
      if (fp.show() == nsIFilePicker.returnOK) {
        openTopWin(fp.fileURL.spec);
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
    var wnd = document.commandDispatcher.focusedWindow;
    if (window == wnd) wnd = window._content;
    var docCharset = wnd.document.characterSet;

    bmks = bmks.QueryInterface(Components.interfaces.nsIBookmarksService);
    if ((title == null) || (title == ""))
    {
    	title = url;
    }
    bmks.AddBookmark(url, title, bmks.BOOKMARK_DEFAULT_TYPE, docCharset);
    dump("BrowserAddBookmark: " + docCharset + "\n");
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

function BrowserChangeTextSize(newSize)
  {

      var oldSize = document.getElementById("menu_TextSize_Popup").getAttribute("oldsize");
      var diff = (newSize - oldSize); 
      if (diff != 0) BrowserChangeTextZoom(diff);
      document.getElementById("menu_TextSize_Popup").setAttribute("oldsize", newSize);           
      
  }
                
  function BrowserSetDefaultCharacterSet(aCharset)
  {
    if (appCore != null) {
      appCore.SetDocumentCharset(aCharset);
      window._content.location.reload();
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserSetForcedCharacterSet(aCharset)
  {
    if (appCore != null) {
      appCore.SetForcedCharset(aCharset);
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserSetForcedDetector()
  {
    if (appCore != null) {
      appCore.SetForcedDetector();
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

    try {
      appCore.loadUrl(gURLBar.value);
      window._content.focus();
    }
    catch(e) {
    }
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
      var nodeName = node.localName;
      if (nodeName == "")
        return null;
      nodeName = nodeName.toLowerCase();
      if (nodeName == "" || nodeName == "body"
          || nodeName == "html" || nodeName == "#document")
        return null;
      var href = node.href;
      if (nodeName == "a" && href != "")
        return node;
      node = node.parentNode;
    }
    return null;
  }

   function isScrollbar(node)
   {
     while (node)
     {
       var nodeName = node.localName;
       if (nodeName == "")
         return false;
       if (nodeName == "scrollbar")
         return true;
       node = node.parentNode;
     }
     return false;
  }

  function browserHandleMiddleClick(event)
  {
    var target = event.target;
    if (pref.GetBoolPref("middlemouse.openNewWindow"))
    {
      var node = enclosingLink(target);
      var href ="";
      if (node)
        href = node.href;
      if (href != "")
      {
        openNewWindowWith(href);
        event.preventBubble();
        return;
      }
    }

    if (pref.GetBoolPref("middlemouse.paste"))
    {
      if (isScrollbar(target)) {
         return;
	  }
      var tagName = target.tagName;
      if (tagName) tagName = tagName.toLowerCase();
      var type = target.type;
      if (type) type = type.toLowerCase();

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

  function OpenMessenger()
  {
	window.open("chrome://messenger/content/messenger.xul", "_blank", "chrome,menubar,toolbar,resizable");
  }

  function OpenAddressbook()
  {
	window.open("chrome://messenger/content/addressbook/addressbook.xul", "_blank", "chrome,menubar,toolbar,resizable");
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
    var docCharset = null;

    try 
    {
      var wnd = document.commandDispatcher.focusedWindow;
      if (window == wnd) wnd = window._content;
      docCharset = "charset="+ wnd.document.characterSet;
      // dump("*** Current document charset: " + docCharset + "\n");
    }

    catch(ex) 
    { 
      docCharset = null;
      dump("*** Failed to determine current document charset \n");
    }


    if (docCharset != null) 
    {
      try 
      {
        //now try to open a view-source window while inheriting the charset
        window.openDialog( "chrome://navigator/content/viewSource.xul",
						         "_blank",
						         "scrollbars,resizable,chrome,dialog=no",
					           window._content.location, docCharset);
      }

      catch(ex) 
      { 
        dump("*** Failed to open view-source window with preset charset menu.\n");
      }

    } else {
        //default: forcing the view-source widow
        dump("*** Failed to preset charset menu for the view-source window\n");
        window.openDialog( "chrome://navigator/content/viewSource.xul",
					         "_blank",
					         "scrollbars,resizable,chrome,dialog=no",
					         window._content.location);
    }
  }

  function BrowserPageInfo()
  {
    var charsetArg = new String();
    
    if (appCore != null) {
      
      try 
        {
          //let's try to extract the current charset menu setting
          var DocCharset = appCore.GetDocumentCharset();
          charsetArg = "charset="+DocCharset;
          dump("*** Current document charset: " + DocCharset + "\n");
          
          //we should "inherit" the charset menu setting in a new window
          window.openDialog( "chrome://navigator/content/pageInfo.xul",
                             "_blank",
                             "chrome,dialog=no",
                             window._content.location, charsetArg);
        }
      
      catch(ex) 
        { 
          dump("*** failed to read document charset \n");
        }
      
    } else {
      //if everythig else fails, forget about the charset
      window.openDialog( "chrome://navigator/content/pageInfo.xul",
                         "_blank",
                         "chrome,dialog=no",
                         window._content.location);
    }
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
		var currentWindow = windowManagerInterface.convertISupportsToDOMWindow ( enumerator.getNext() );
		if ( currentWindow.screenX == screenX && currentWindow.screenY == screenY )
		{
			alreadyThere = true;
			break;
		}	
	} while ( enumerator.hasMoreElements() )
	
	if ( alreadyThere )
	{
		enumerator = windowManagerInterface.getEnumerator( null );
		do
		{
			var currentWindow = windowManagerInterface.convertISupportsToDOMWindow ( enumerator.getNext() );
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
		} while ( enumerator.hasMoreElements() )
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

function getLeakDetector()
{
    leakDetector = createInstance("component://netscape/xpcom/leakdetector", "nsILeakDetector");
    if (leakDetector == null) {
        dump("Could not create leak detector, leak detection probably\n");
        dump("not compiled into this browser\n");
    }
}

// Dumps current set of memory leaks.
function dumpMemoryLeaks() {
    if (leakDetector == null)
        getLeakDetector();
	if (leakDetector != null)
		leakDetector.dumpLeaks();
}

function traceObject(object)
{
    if (leakDetector == null)
        getLeakDetector();
	if (leakDetector != null)
		leakDetector.traceObject(object);
}

var consoleListener = {
  observe: function (aMsgObject) 
    {
      const nsIScriptError = Components.interfaces.nsIScriptError;
      var scriptError = aMsgObject.QueryInterface(nsIScriptError);
      var isWarning = scriptError.flags & nsIScriptError.warningFlag != 0;
      if (!isWarning) 
        {
          var statusbarDisplay = document.getElementById("statusbar-display");
          statusbarDisplay.setAttribute("error", "true");
          statusbarDisplay.addEventListener("click", loadErrorConsole, true);
          statusbarDisplay.value = bundle.GetStringFromName("jserror");
          this.isShowingError = true;
        }
    },

  // whether or not an error alert is being displayed
  isShowingError: false,
    
};

function initConsoleListener()
{
  var consoleService = nsJSComponentManager.getService("mozilla.consoleservice.1", "nsIConsoleService");

  /**
   * XXX - console launch hookup requires some work that I'm not sure how to
   *       do.
   *
   *       1) ideally, the notification would disappear when the document that had the
   *          error was flushed. how do I know when this happens? All the nsIScriptError
   *          object I get tells me is the URL. Where is it located in the content area?
   *       2) the notification service should not display chrome script errors.
   *          web developers and users are not interested in the failings of our shitty, 
   *          exception unsafe js. One could argue that this should also extend to
   *          the console by default (although toggle-able via setting for chrome authors)
   *          At any rate, no status indication should be given for chrome script
   *          errors. 
   * 
   *       As a result I am commenting out this for the moment. 
   **/  
  //if (consoleService)
  //  consoleService.registerListener(consoleListener);
} 

function loadErrorConsole(aEvent)
{
  if (aEvent.detail == 2)
    toJavaScriptConsole();
}

function clearErrorNotification()
{
  var statusbarDisplay = document.getElementById("statusbar-display");
  statusbarDisplay.removeAttribute("error");
  statusbarDisplay.removeEventListener("click", loadErrorConsole, true);
  consoleListener.isShowingError = false;
}

//Posts the currently displayed url to a native widget so third-party apps can observe it.
var urlWidgetService = null;
try {
    urlWidgetService = getService( "component://mozilla/urlwidget", "nsIUrlWidget" );
} catch( exception ) {
    //dump( "Error getting url widget service: " + exception + "\n" );
}
function postURLToNativeWidget() {
    if ( urlWidgetService ) {
        var url = window._content.location.href;
        try {
            urlWidgetService.SetURLToHiddenControl( url, window );
        } catch( exception ) {
            dump( " SetURLToHiddenControl failed: " + exception + "\n" );
        }
    }
}

function checkForDirectoryListing() {
    if ( window._content.HTTPIndex == "[xpconnect wrapped nsIHTTPIndex]"
         &&
         typeof window._content.HTTPIndex == "object"
         &&
         !window._content.HTTPIndex.constructor ) {
        // Give directory .xul/.js access to browser instance.
        window._content.defaultCharacterset = appCore.GetDocumentCharset();
        window._content.parentWindow = window;
    }
}

/**
 * Content area tooltip. 
 * XXX - this must move into XBL binding/equiv! Do not want to pollute 
 *       navigator.js with functionality that can be encapsulated into
 *       browser widget. TEMPORARY!
 **/
function FillInHTMLTooltip ( tipElement )
{
  var HTMLNS = "http://www.w3.org/1999/xhtml";
  var XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  var XLinkNS = "http://www.w3.org/1999/xlink";
  
  var retVal = false;
  var tipNode = document.getElementById("HTML_TOOLTIP_tooltipBox");
  if ( tipNode ) {
    try {
      while ( tipNode.hasChildNodes() )
        tipNode.removeChild( tipNode.firstChild );
        
      var titleText = "";
      var XLinkTitleText = "";
      
      while ( !titleText && !XLinkTitleText && tipElement ) {
        if ( tipElement.nodeType == 1 ) {
          titleText = tipElement.getAttributeNS(HTMLNS, "title");
          XLinkTitleText = tipElement.getAttributeNS(XLinkNS, "title");
        }
        tipElement = tipElement.parentNode;
      }
      
      var texts = [ titleText, XLinkTitleText ];
      
      for (var i = 0; i < texts.length; i++) {
        var t = texts[i];
        if ( t.search(/\S/) >= 0 ) {
          var tipLineElem = tipNode.ownerDocument.createElementNS(XULNS, "text");
          tipLineElem.setAttribute("value", t);
          tipNode.appendChild(tipLineElem);
          
          retVal = true;
        }
      }
    }
    catch (e) { retVal = false; }
  }
  
  return retVal;
}

function EnableBusyCursor(doEnable) {
  if (doEnable) {
    window.setCursor("spinning");
    window._content.setCursor("spinning");
  }
  else {
    window.setCursor("auto");
    window._content.setCursor("auto");
  }
}


/**
 * Use Stylesheet functions. 
 *     Written by Tim Hill (bug 6782)
 **/
function stylesheetFillPopup(forDocument, menuPopup, itmNoOptStyles) {
  children = menuPopup.childNodes;
  ccount = children.length;
  for (i=0; i<ccount; i++) {
    mnuitem = children[i];
    if (mnuitem.getAttribute("class") == "authssmenuitem")
      menuPopup.removeChild(mnuitem);
  }

  checkNoOptStyles = true;
  var numSS = forDocument.styleSheets.length;
  var SS = forDocument.styleSheets;
  existingSS = new Array();

  for (var i=0; i<numSS; i++) {
    curSS = SS[i];

    if (curSS.title == "") continue;

    if (!curSS.disabled)
      checkNoOptStyles = false;

    lastWithSameTitle = existingSS[curSS.title];
    if (lastWithSameTitle == undefined) {
      mnuitem = document.createElement("menuitem");
      mnuitem.setAttribute("value", curSS.title);
      mnuitem.setAttribute("data", curSS.title);
      mnuitem.setAttribute("type", "radio");
      mnuitem.setAttribute("checked", !curSS.disabled);
      mnuitem.setAttribute("class", "authssmenuitem");
      mnuitem.setAttribute("name", "authorstyle");
      mnuitem.setAttribute("oncommand", "stylesheetSwitch(window._content.document, this.getAttribute('data'))");
      menuPopup.appendChild(mnuitem);
      existingSS[curSS.title] = mnuitem;
    }
    else {
      if (curSS.disabled) lastWithSameTitle.setAttribute("checked", false);
    }
  }
  itmNoOptStyles.setAttribute("checked", checkNoOptStyles);
}

function stylesheetSwitch(forDocument, title) {
  docStyleSheets = forDocument.styleSheets;
  numSS = docStyleSheets.length;

  for (i=0; i<numSS; i++) {
    docStyleSheet = docStyleSheets[i];
    if (docStyleSheet.title == "") {
      if (docStyleSheet.disabled == true)
        docStyleSheet.disabled = false;
      continue;
    }
  } 
    docStyleSheet.disabled = (title != docStyleSheet.title);
}

function applyTheme(themeName)
{
try {
  var chromeRegistry = Components.classes["component://netscape/chrome/chrome-registry"].getService();
  if ( chromeRegistry )
    chromeRegistry = chromeRegistry.QueryInterface( Components.interfaces.nsIChromeRegistry );
}
catch(e) {}

chromeRegistry.selectSkin( themeName.getAttribute('name'), true ); 
chromeRegistry.refreshSkins();
}  

function getNewThemes()
{
window._content.location.href = brandBundle.GetStringFromName("getNewThemesURL");
}
