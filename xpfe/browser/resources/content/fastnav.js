/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blake Ross <blakeross@telocity.com>
 *   Peter Annema <disttsc@bart.nl>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const nsIWebNavigation = Components.interfaces.nsIWebNavigation;

var gURLBar = null;
var gProxyButton = null;
var gNavigatorBundle;
var gBrandBundle;
var gNavigatorRegionBundle;
var gBrandRegionBundle;
var gLastValidURL = "";

var pref = Components.classes["@mozilla.org/preferences;1"]
                     .getService(Components.interfaces.nsIPref);

var appCore = null;

//cached elements
var gBrowser = null;

// focused frame URL
var gFocusedURL = null;


/**

* We can avoid adding multiple load event listeners and save some time by adding
* one listener that calls all real handlers.
*/

function loadEventHandlers(event)
{
  // Filter out events that are not about the document load we are interested in
  if (event.target == _content.document) {
    UpdateBookmarksLastVisitedDate(event);
    UpdateInternetSearchResults(event);
    checkForDirectoryListing();
    getContentAreaFrameCount();
    postURLToNativeWidget();
  }
}

/**
 * Determine whether or not the content area is displaying a page with frames,
 * and if so, toggle the display of the 'save frame as' menu item.
 **/
function getContentAreaFrameCount()
{
  var saveFrameItem = document.getElementById("savepage");
  if (!_content.frames.length || !isDocumentFrame(document.commandDispatcher.focusedWindow))
    saveFrameItem.setAttribute("hidden", "true");
  else
    saveFrameItem.removeAttribute("hidden");
}

// When a content area frame is focused, update the focused frame URL
function contentAreaFrameFocus()
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (isDocumentFrame(focusedWindow)) {
    gFocusedURL = focusedWindow.location.href;
    var saveFrameItem = document.getElementById("savepage");
    saveFrameItem.removeAttribute("hidden");
  }
}

//////////////////////////////// BOOKMARKS ////////////////////////////////////

function UpdateBookmarksLastVisitedDate(event)
{
  // XXX This somehow causes a big leak, back to the old way
  //     till we figure out why. See bug 61886.
  // var url = getWebNavigation().currentURI.spec;
  var url = _content.location.href;
  if (url) {
    // if the URL is bookmarked, update its "Last Visited" date
    var bmks = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                         .getService(Components.interfaces.nsIBookmarksService);

    bmks.UpdateBookmarkLastVisitedDate(url, _content.document.characterSet);
  }
}

function UpdateInternetSearchResults(event)
{
  // XXX This somehow causes a big leak, back to the old way
  //     till we figure out why. See bug 61886.
  // var url = getWebNavigation().currentURI.spec;
  var url = _content.location.href;
  if (url) {
    try {
      var search = Components.classes["@mozilla.org/rdf/datasource;1?name=internetsearch"]
                             .getService(Components.interfaces.nsIInternetSearchService);

      var searchInProgressFlag = search.FindInternetSearchResults(url);

      if (searchInProgressFlag) {
        var autoOpenSearchPanel = pref.GetBoolPref("browser.search.opensidebarsearchpanel");

        if (autoOpenSearchPanel)
          RevealSearchPanel();
      }
    } catch (ex) {
    }
  }
}

function getBrowser()
{
  if (!gBrowser)
    gBrowser = document.getElementById("content");
  return gBrowser;
}

function getWebNavigation()
{
  try {
    return getBrowser().webNavigation;
  } catch (e) {
    return null;
  }
}

function getMarkupDocumentViewer()
{
  return getBrowser().markupDocumentViewer;
}

function getHomePage()
{
  var url;
  try {
    url = pref.getLocalizedUnicharPref("browser.startup.homepage");
  } catch (e) {
  }

  // use this if we can't find the pref
  if (!url)
    url = gNavigatorRegionBundle.getString("homePageDefault");

  return url;
}

function UpdateBackForwardButtons()
{
  var backBroadcaster = document.getElementById("canGoBack");
  var forwardBroadcaster = document.getElementById("canGoForward");
  var webNavigation = getWebNavigation();

  // Avoid setting attributes on broadcasters if the value hasn't changed!
  // Remember, guys, setting attributes on elements is expensive!  They
  // get inherited into anonymous content, broadcast to other widgets, etc.!
  // Don't do it if the value hasn't changed! - dwh

  var backDisabled = (backBroadcaster.getAttribute("disabled") == "true");
  var forwardDisabled = (forwardBroadcaster.getAttribute("disabled") == "true");

  if (backDisabled == webNavigation.canGoBack)
    backBroadcaster.setAttribute("disabled", !backDisabled);
  
  if (forwardDisabled == webNavigation.canGoForward)
    forwardBroadcaster.setAttribute("disabled", !forwardDisabled);
}


function nsButtonPrefListener()
{
  try {
    pref.addObserver(this.domain, this);
  } catch(ex) {
    dump("Failed to observe prefs: " + ex + "\n");
  }
}

// implements nsIObserver
nsButtonPrefListener.prototype =
{
  domain: "browser.toolbars.showbutton",
  observe: function(subject, topic, prefName)
  {
    // verify that we're changing a button pref
    if (topic != "nsPref:changed") return;
    if (prefName.substr(0, this.domain.length) != this.domain) return;

    var buttonName = prefName.substr(this.domain.length+1);
    var buttonId = buttonName + "-button";
    var button = document.getElementById(buttonId);

    var show = pref.GetBoolPref(prefName);
    if (show)
      button.setAttribute("hidden","false");
    else
      button.setAttribute("hidden", "true");
  }
}

function Startup()
{
  // init globals
  gNavigatorBundle = document.getElementById("bundle_navigator");
  gBrandBundle = document.getElementById("bundle_brand");
  gNavigatorRegionBundle = document.getElementById("bundle_navigator_region");
  gBrandRegionBundle = document.getElementById("bundle_brand_region");

  gBrowser = document.getElementById("content");
  gURLBar = document.getElementById("urlbar");
  
  SetPageProxyState("invalid");

  var webNavigation;
  try {
    // Create the browser instance component.
    appCore = Components.classes["@mozilla.org/appshell/component/browser/instance;1"]
                        .createInstance(Components.interfaces.nsIBrowserInstance);
    if (!appCore)
      throw Components.results.NS_ERROR_FAILURE;

    webNavigation = getWebNavigation();
    if (!webNavigation)
      throw Components.results.NS_ERROR_FAILURE;
  } catch (e) {
    alert("Error creating browser instance");
    window.close(); // Give up.
    return;
  }

  // Do all UI building here:

  //setOfflineStatus();

  // set home button tooltip text
  var homePage = getHomePage();
  if (homePage)
    document.getElementById("home-button").setAttribute("tooltiptext", homePage);

  // initialize observers and listeners
  window.XULBrowserWindow = new nsBrowserStatusHandler();
  window.buttonPrefListener = new nsButtonPrefListener();

  window.browserContentListener =
    new nsBrowserContentListener(window, getBrowser());
  
  // XXXjag hack for directory.xul/js
  _content.appCore = appCore;

  // Initialize browser instance..
  appCore.setWebShellWindow(window);

  // Add a capturing event listener to the content area
  // (rjc note: not the entire window, otherwise we'll get sidebar pane loads too!)
  //  so we'll be notified when onloads complete.
  var contentArea = document.getElementById("appcontent");
  contentArea.addEventListener("load", loadEventHandlers, true);
  contentArea.addEventListener("focus", contentAreaFrameFocus, true);

  // set default character set if provided
  if ("arguments" in window && window.arguments.length > 1 && window.arguments[1]) {
    if (window.arguments[1].indexOf("charset=") != -1) {
      var arrayArgComponents = window.arguments[1].split("=");
      if (arrayArgComponents) {
        //we should "inherit" the charset menu setting in a new window
        appCore.setDefaultCharacterSet(arrayArgComponents[1]); //XXXjag see bug 67442
      }
    }
  }

  //initConsoleListener();

  // wire up session history before any possible progress notifications for back/forward button updating
  webNavigation.sessionHistory = Components.classes["@mozilla.org/browser/shistory;1"]
                                           .createInstance(Components.interfaces.nsISHistory);

  // hook up UI through progress listener
  var interfaceRequestor = getBrowser().docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
  var webProgress = interfaceRequestor.getInterface(Components.interfaces.nsIWebProgress);
  webProgress.addProgressListener(window.XULBrowserWindow);

  // XXXjag see bug 68662 (needed to hook up web progress listener)
  getBrowser().boxObject.setPropertyAsSupports("listenerkungfu", window.XULBrowserWindow);

  // load appropriate initial page from commandline
  var isPageCycling = false;

  // page cycling for tinderbox tests
  if (!appCore.cmdLineURLUsed)
    isPageCycling = appCore.startPageCycler();

  // only load url passed in when we're not page cycling
  if (!isPageCycling) {
    var uriToLoad;

    if (!appCore.cmdLineURLUsed) {
      var cmdLineService = Components.classes["@mozilla.org/appshell/commandLineService;1"]
                                     .getService(Components.interfaces.nsICmdLineService);
      uriToLoad = cmdLineService.URLToLoad;
      if (!uriToLoad) {
        var cmdLineHandler = Components.classes["@mozilla.org/commandlinehandler/general-startup;1?type=browser"]
                                       .getService(Components.interfaces.nsICmdLineHandler);
        uriToLoad = cmdLineHandler.defaultArgs;
      }
      appCore.cmdLineURLUsed = true;
    }

    if (!uriToLoad) {
      // Check for window.arguments[0]. If present, use that for uriToLoad.
      if ("arguments" in window && window.arguments.length >= 1 && window.arguments[0])
        uriToLoad = window.arguments[0];
    }

    if (uriToLoad && uriToLoad != "about:blank") {
      gURLBar.value = uriToLoad;
      loadURI(uriToLoad);
    }

    // Focus the content area if the caller instructed us to.
    if ("arguments" in window && window.arguments.length >= 3 && window.arguments[2] == true)
      _content.focus();
    else
      gURLBar.focus();

    // Perform default browser checking.
    checkForDefaultBrowser();
  }
}

function Shutdown()
{
  var browser = getBrowser();

  browser.boxObject.removeProperty("listenerkungfu");

  try {
    var interfaceRequestor = browser.docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    var webProgress = interfaceRequestor.getInterface(Components.interfaces.nsIWebProgress);
    webProgress.removeProgressListener(window.XULBrowserWindow);
  } catch (ex) {
  }

  window.XULBrowserWindow.destroy();
  window.XULBrowserWindow = null;

  try {
    // If bookmarks are dirty, flush 'em to disk
    var bmks = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                         .getService(Components.interfaces.nsIRDFRemoteDataSource);
    bmks.Flush();
  } catch (ex) {
  }

  try {
    // give history a chance at flushing to disk also
    var history = Components.classes["@mozilla.org/browser/global-history;1"]
                            .getService(Components.interfaces.nsIRDFRemoteDataSource);
    history.Flush();
  } catch (ex) {
  }

  // unregister us as a pref listener
  pref.removeObserver(window.buttonPrefListener.domain,
                      window.buttonPrefListener);

  window.browserContentListener.close();
  // Close the app core.
  if (appCore)
    appCore.close();
}

function Translate()
{
  var service = "http://cgi.netscape.com/cgi-bin/translate.cgi?AlisUI=simple_frames/ns_home";

  // if we're already viewing a translated page, then just get the
  // last argument (which we expect to always be "AlisTargetURI")
  // XXX This somehow causes a big leak, back to the old way
  //     till we figure out why. See bug 61886.
  // var targetURI = getWebNavigation().currentURI.spec;
  var targetURI = _content.location.href;
  var targetURIIndex = targetURI.indexOf("AlisTargetURI=");

  if (targetURIIndex >= 0)
    targetURI = targetURI.substring(targetURIIndex + 14);

  service += "&AlisTargetURI=" + escape(targetURI);

  loadURI(service);
}

function gotoHistoryIndex(aEvent)
{
  var index = aEvent.target.getAttribute("index");
  if (!index)
    return false;
  try {
    getWebNavigation().gotoIndex(index);
  }
  catch(ex) {
    return false;
  }
  return true;

}

function BrowserBack()
{
  try {
    getWebNavigation().goBack();
  }
  catch(ex) {
  }
  UpdateBackForwardButtons();
}

function BrowserForward()
{
  try {
    getWebNavigation().goForward();
  }
  catch(ex) {
  }
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
  try {
    const stopFlags = nsIWebNavigation.STOP_ALL;
    getWebNavigation().stop(stopFlags);
  }
  catch(ex) {
  }
}

function BrowserReload()
{
  const reloadFlags = nsIWebNavigation.LOAD_FLAGS_NONE;
  return BrowserReloadWithFlags(reloadFlags);
}

function BrowserReloadSkipCache()
{
  // Bypass proxy and cache.
  const reloadFlags = nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY | nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
  return BrowserReloadWithFlags(reloadFlags);
}

function BrowserReloadWithFlags(reloadFlags)
{
   try {
     /* Need to get SessionHistory from docshell so that
      * reload on framed pages will work right. This 
      * method should not be used for the context menu item "Reload frame".
      * "Reload frame" should directly call into docshell as it does right now
      */
     var sh = getWebNavigation().sessionHistory;
     var webNav = sh.QueryInterface(Components.interfaces.nsIWebNavigation);      
     webNav.reload(reloadFlags);
   }
   catch(ex) {
   }
 }

function BrowserHome()
{
  var homePage = getHomePage();
  loadURI(homePage);
}

function OpenBookmarkURL(node, datasources)
{
  if (node.getAttribute("container") == "true")
    return;

  var url = node.getAttribute("id");
  try {
    // add support for IE favorites under Win32, and NetPositive URLs under BeOS
    if (datasources) {
      var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                          .getService(Components.interfaces.nsIRDFService);
      var src = rdf.GetResource(url, true);
      var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
      var target = datasources.GetTarget(src, prop, true);
      if (target) {
        target = target.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
        if (target)
          url = target;
      }
    }
  } catch (ex) {
    return;
  }

  // Ignore "NC:" urls.
  if (url.substring(0, 3) == "NC:")
    return;

  // Check if we have a browser window
  if (_content)
    loadURI(url);
  else
    openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", url);
}

function urlDomain(url)
     {
		urlReg = /http:\/\/([\w.]+)\//;
	    return url.match(urlReg)[0];
	 }


function readRDFString(aDS,aRes,aProp) {
          var n = aDS.GetTarget(aRes, aProp, true);
          if (n)
            return n.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
}


function ensureDefaultEnginePrefs(aRDF,aDS) 
   {

    mPrefs = Components.classes["@mozilla.org/preferences;1"].getService(Components.interfaces.nsIPrefBranch);
    var defaultName = mPrefs.getComplexValue("browser.search.defaultenginename" , Components.interfaces.nsIPrefLocalizedString);
	kNC_Root = aRDF.GetResource("NC:SearchEngineRoot");
    kNC_child = aRDF.GetResource("http://home.netscape.com/NC-rdf#child");
    kNC_Name = aRDF.GetResource("http://home.netscape.com/NC-rdf#Name");
          
    var arcs = aDS.GetTargets(kNC_Root, kNC_child, true);
    while (arcs.hasMoreElements()) {
        var engineRes = arcs.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
        var name = readRDFString(aDS, engineRes, kNC_Name);
		if (name == defaultName)
           mPrefs.setCharPref("browser.search.defaultengine", engineRes.Value);
    }
  }


function ensureSearchPref() {
   
   var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
   var ds = rdf.GetDataSource("rdf:internetsearch");
   kNC_Name = rdf.GetResource("http://home.netscape.com/NC-rdf#Name");
   try {
            defaultEngine = mPrefs.getCharPref("browser.search.defaultengine");
        } catch(ex) {
            ensureDefaultEnginePrefs(rdf, ds);
            defaultEngine = mPrefs.getCharPref("browser.search.defaultengine");
            }
   }

function OpenSearch(tabName, forceDialogFlag, searchStr)
{

  //This function needs to be split up someday.

  var autoOpenSearchPanel = false;
  var defaultSearchURL = null;
  var fallbackDefaultSearchURL = gNavigatorRegionBundle.getString("fallbackDefaultSearchURL");
  // XXX This somehow causes a big leak, back to the old way
  //     till we figure out why. See bug 61886.
  // var url = getWebNavigation().currentURI.spec;
  var url = _content.location.href;
  ensureSearchPref()
  //Check to see if search string contains "://" or "ftp." or white space.
  //If it does treat as url and match for pattern
  
    var urlmatch= /(:\/\/|^ftp\.)[^ \S]+$/ 
    var forceAsURL = urlmatch.test(searchStr);

  try {
    autoOpenSearchPanel = pref.GetBoolPref("browser.search.opensidebarsearchpanel");
    defaultSearchURL = pref.getLocalizedUnicharPref("browser.search.defaulturl");
  } catch (ex) {
  }

  // Fallback to a default url (one that we can get sidebar search results for)
  if (!defaultSearchURL)
    defaultSearchURL = fallbackDefaultSearchURL;

  //Check to see if content url equals url in location bar.
  //If they match then go to default search URL engine

  if ((!searchStr || searchStr == url)) {
      loadURI(gNavigatorRegionBundle.getString("otherSearchURL"));

  } else {

  //Check to see if location bar field is a url
  //If it is a url go to URL.  A Url is "://" or "." as commented above
  //Otherwise search on entry
  if (forceAsURL) {
     BrowserLoadURL()
   } else {
    var searchMode = 0;
    try {
      searchMode = pref.GetIntPref("browser.search.powermode");
    } catch(ex) {
    }
    if (forceDialogFlag || searchMode == 1) {
      // Use a single search dialog
      var windowManager = Components.classes["@mozilla.org/rdf/datasource;1?name=window-mediator"]
                                    .getService(Components.interfaces.nsIWindowMediator);

      var searchWindow = windowManager.getMostRecentWindow("search:window");
      if (!searchWindow) {
        openDialog("chrome://communicator/content/search/search.xul", "SearchWindow", "dialog=no,close,chrome,resizable", tabName, searchStr);
      } else {
        // Already had one, focus it and load the page
        searchWindow.focus();

        if ("loadPage" in searchWindow)
          searchWindow.loadPage(tabName, searchStr);
      }
    } else {
      if (searchStr) {
        var escapedSearchStr = escape(searchStr);
        defaultSearchURL += escapedSearchStr;

        var searchDS = Components.classes["@mozilla.org/rdf/datasource;1?name=internetsearch"]
                                 .getService(Components.interfaces.nsIInternetSearchService);

        searchDS.RememberLastSearchText(escapedSearchStr);
        try {
          var searchEngineURI = pref.CopyCharPref("browser.search.defaultengine");
          if (searchEngineURI) {
            var searchURL = searchDS.GetInternetSearchURL(searchEngineURI, escapedSearchStr);
          if (searchURL)
              defaultSearchURL = searchURL;
          }
        } catch (ex) {
        }
        loadURI(defaultSearchURL);
      }
     }
    }
  }

  // should we try and open up the sidebar to show the "Search Results" panel?
  if (autoOpenSearchPanel)
    RevealSearchPanel();
}

function RevealSearchPanel()
{
  var searchPanel = document.getElementById("urn:sidebar:panel:search");
  if (searchPanel)
    SidebarSelectPanel(searchPanel, true, true); // lives in sidebarOverlay.js
}

//Note: BrowserNewEditorWindow() was moved to globalOverlay.xul and renamed to NewEditorWindow()

function BrowserOpenWindow()
{
  //opens a window where users can select a web location to open
  openDialog("chrome://communicator/content/openLocation.xul", "_blank", "chrome,modal,titlebar", window);
}

/* Called from the openLocation dialog. This allows that dialog to instruct
   its opener to open a new window and then step completely out of the way.
   Anything less byzantine is causing horrible crashes, rather believably,
   though oddly only on Linux. */
function delayedOpenWindow(chrome,flags,url)
{
  setTimeout("openDialog('"+chrome+"','_blank','"+flags+"','"+url+"')", 10);
}

function BrowserOpenFileWindow()
{
  // Get filepicker component.
  try {
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, gNavigatorBundle.getString("openFile"), nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterAll | nsIFilePicker.filterText | nsIFilePicker.filterImages |
                     nsIFilePicker.filterXML | nsIFilePicker.filterHTML);

    if (fp.show() == nsIFilePicker.returnOK)
      openTopWin(fp.fileURL.spec);
  } catch (ex) {
  }
}

// Set up a lame hack to avoid opening two bookmarks.
// Could otherwise happen with two Ctrl-B's in a row.
var gDisableBookmarks = false;
function enableBookmarks()
{
  gDisableBookmarks = false;
}

function BrowserEditBookmarks()
{
  // Use a single sidebar bookmarks dialog
  var windowManager = Components.classes["@mozilla.org/rdf/datasource;1?name=window-mediator"]
                                .getService(Components.interfaces.nsIWindowMediator);

  var bookmarksWindow = windowManager.getMostRecentWindow("bookmarks:manager");

  if (bookmarksWindow) {
    bookmarksWindow.focus();
  } else {
    // while disabled, don't open new bookmarks window
    if (!gDisableBookmarks) {
      gDisableBookmarks = true;

      open("chrome://communicator/content/bookmarks/bookmarks.xul", "_blank", "chrome,menubar,resizable,scrollbars");
      setTimeout(enableBookmarks, 2000);
    }
  }
}

function BrowserPrintPreview()
{
  // implement me
}

function BrowserPrint()
{
  // using _content.print() until printing becomes scriptable on docShell
  try {
    _content.print();
  } catch (e) {
    // Pressing cancel is expressed as an NS_ERROR_FAILURE return value,
    // causing an exception to be thrown which we catch here.
    // Unfortunately this will also consume helpful failures, so add a
    // dump(e); // if you need to debug
  }
}

function BrowserSetDefaultCharacterSet(aCharset)
{
  appCore.setDefaultCharacterSet(aCharset); //XXXjag see bug 67442
  BrowserReload();
}

function BrowserSetForcedCharacterSet(aCharset)
{
  var charsetConverterManager = Components.classes["@mozilla.org/charset-converter-manager;1"]
                                          .getService(Components.interfaces.nsICharsetConverterManager2);
  var characterSet = charsetConverterManager.GetCharsetAtom(aCharset);
  getBrowser().documentCharsetInfo.forcedCharset = characterSet;
}

function BrowserSetForcedDetector()
{
  getBrowser().documentCharsetInfo.forcedDetector = true;
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

function BrowserFind()
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (!focusedWindow || focusedWindow == window)
    focusedWindow = window._content;

  findInPage(getBrowser(), window._content, focusedWindow)
}

function BrowserFindAgain()
{
    var focusedWindow = document.commandDispatcher.focusedWindow;
    if (!focusedWindow || focusedWindow == window)
      focusedWindow = window._content;

  findAgainInPage(getBrowser(), window._content, focusedWindow)
}

function BrowserCanFindAgain()
{
  return canFindAgainInPage();
}

function loadURI(uri)
{
  try {
    getWebNavigation().loadURI(uri, nsIWebNavigation.LOAD_FLAGS_NONE);
  } catch (e) {
  }
}

function BrowserLoadURL()
{
  var url = gURLBar.value;
  if (url.match(/^view-source:/)) {
    BrowserViewSourceOfURL(url.replace(/^view-source:/, ""), null);
  } else {
    loadURI(getShortcutOrURI(url));
    _content.focus();
  }
}

function getShortcutOrURI(url)
{
  // rjc: added support for URL shortcuts (3/30/1999)
  try {
    var bmks = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                         .getService(Components.interfaces.nsIBookmarksService);

    var shortcutURL = bmks.FindShortcut(url);
    if (!shortcutURL) {
      // rjc: add support for string substitution with shortcuts (4/4/2000)
      //      (see bug # 29871 for details)
      var aOffset = url.indexOf(" ");
      if (aOffset > 0) {
        var cmd = url.substr(0, aOffset);
        var text = url.substr(aOffset+1);
        shortcutURL = bmks.FindShortcut(cmd);
        if (shortcutURL && text) {
          aOffset = shortcutURL.indexOf("%s");
          if (aOffset >= 0)
            shortcutURL = shortcutURL.substr(0, aOffset) + text + shortcutURL.substr(aOffset+2);
          else
            shortcutURL = null;
        }
      }
    }

    if (shortcutURL)
      url = shortcutURL;

  } catch (ex) {
  }
  return url;
}

function readFromClipboard()
{
  var url;

  try {
    // Get clipboard.
    var clipboard = Components.classes["@mozilla.org/widget/clipboard;1"]
                              .getService(Components.interfaces.nsIClipboard);

    // Create tranferable that will transfer the text.
    var trans = Components.classes["@mozilla.org/widget/transferable;1"]
                          .createInstance(Components.interfaces.nsITransferable);

    trans.addDataFlavor("text/unicode");
    clipboard.getData(trans, clipboard.kSelectionClipboard);

    var data = {};
    var dataLen = {};
    trans.getTransferData("text/unicode", data, dataLen);

    if (data) {
      data = data.value.QueryInterface(Components.interfaces.nsISupportsWString);
      url = data.data.substring(0, dataLen.value / 2);
    }
  } catch (ex) {
  }

  return url;
}

function OpenMessenger()
{
  open("chrome://messenger/content/messenger.xul", "_blank", "chrome,menubar,toolbar,resizable");
}

function OpenAddressbook()
{
  open("chrome://messenger/content/addressbook/addressbook.xul", "_blank", "chrome,menubar,toolbar,resizable");
}

function BrowserViewSource()
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (focusedWindow == window)
    focusedWindow = _content;

  dump("focusedWindow = " + focusedWindow + "\n");
  if (focusedWindow)
    var docCharset = "charset=" + focusedWindow.document.characterSet;

  BrowserViewSourceOfURL(_content.location, docCharset);
}

function BrowserViewSourceOfURL(url, charset)
{
  // try to open a view-source window while inheriting the charset (if any)
  openDialog("chrome://navigator/content/viewSource.xul",
             "_blank",
             "scrollbars,resizable,chrome,dialog=no",
             url, charset);
}

// doc=null for regular page, doc=owner document for frame.
function BrowserPageInfo(doc)
{
  window.openDialog("chrome://navigator/content/pageInfo.xul",
                    "_blank",
                    "chrome,dialog=no",
                    doc);
}

function hiddenWindowStartup()
{
  // Disable menus which are not appropriate
  var disabledItems = ['cmd_close', 'Browser:SendPage', 'Browser:EditPage', /*'Browser:PrintSetup', 'Browser:PrintPreview',*/
                       'Browser:Print', 'canGoBack', 'canGoForward', 'Browser:Home', 'Browser:AddBookmark', 'cmd_undo',
                       'cmd_redo', 'cmd_cut', 'cmd_copy','cmd_paste', 'cmd_delete', 'cmd_selectAll'];
  for (id in disabledItems) {
    var broadcaster = document.getElementById(disabledItems[id]);
    if (broadcaster)
      broadcaster.setAttribute("disabled", "true");
  }
}

// Initialize the LeakDetector class.
function LeakDetector(verbose)
{
  this.verbose = verbose;
}

try {
  LeakDetector.prototype = Components.classes["@mozilla.org/xpcom/leakdetector;1"]
                           .createInstance(Components.interfaces.nsILeakDetector);
} catch (err) {
  LeakDetector.prototype = Object.prototype;
}

var leakDetector = new LeakDetector(false);

// Dumps current set of memory leaks.
function dumpMemoryLeaks()
{
  leakDetector.dumpLeaks();
}

// Traces all objects reachable from the chrome document.
function traceChrome()
{
  leakDetector.traceObject(document, leakDetector.verbose);
}

// Traces all objects reachable from the content document.
function traceDocument()
{
  // keep the chrome document out of the dump.
  leakDetector.markObject(document, true);
  leakDetector.traceObject(_content, leakDetector.verbose);
  leakDetector.markObject(document, false);
}

// Controls whether or not we do verbose tracing.
function traceVerbose(verbose)
{
  leakDetector.verbose = (verbose == "true");
}

var consoleListener = {
  observe: function (aMsgObject)
  {
    const nsIScriptError = Components.interfaces.nsIScriptError;
    var scriptError = aMsgObject.QueryInterface(nsIScriptError);
    var isWarning = scriptError.flags & nsIScriptError.warningFlag != 0;
    if (!isWarning) {
      var statusbarDisplay = document.getElementById("statusbar-display");
      statusbarDisplay.setAttribute("error", "true");
      statusbarDisplay.addEventListener("click", loadErrorConsole, true);
      statusbarDisplay.label = gNavigatorBundle.getString("jserror");
      this.isShowingError = true;
    }
  },

  // whether or not an error alert is being displayed
  isShowingError: false
};

function initConsoleListener()
{
  /**
   * XXX - console launch hookup requires some work that I'm not sure
   * how to do.
   *
   *       1) ideally, the notification would disappear when the
   *       document that had the error was flushed. how do I know when
   *       this happens? All the nsIScriptError object I get tells me
   *       is the URL. Where is it located in the content area?
   *       2) the notification service should not display chrome
   *       script errors.  web developers and users are not interested
   *       in the failings of our shitty, exception unsafe js. One
   *       could argue that this should also extend to the console by
   *       default (although toggle-able via setting for chrome
   *       authors) At any rate, no status indication should be given
   *       for chrome script errors.
   *
   *       As a result I am commenting out this for the moment.
   *

  var consoleService = Components.classes["@mozilla.org/consoleservice;1"]
                                 .getService(Components.interfaces.nsIConsoleService);

  if (consoleService)
    consoleService.registerListener(consoleListener);
  */
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

var urlWidgetService = null;
try {
  urlWidgetService = Components.classes["@mozilla.org/urlwidget;1"]
                               .getService(Components.interfaces.nsIUrlWidget);
} catch (ex) {
}

//Posts the currently displayed url to a native widget so third-party apps can observe it.
function postURLToNativeWidget()
{
  if (urlWidgetService) {
    // XXX This somehow causes a big leak, back to the old way
    //     till we figure out why. See bug 61886.
    // var url = getWebNavigation().currentURI.spec;
    var url = _content.location.href;
    try {
      urlWidgetService.SetURLToHiddenControl(url, window);
    } catch(ex) {
    }
  }
}

function checkForDirectoryListing()
{
  if ( "HTTPIndex" in _content &&
       _content.HTTPIndex instanceof Components.interfaces.nsIHTTPIndex ) {
    _content.defaultCharacterset = getMarkupDocumentViewer().defaultCharacterSet;
    _content.parentWindow = window;
  }
}

/**
 * Content area tooltip.
 * XXX - this must move into XBL binding/equiv! Do not want to pollute
 *       navigator.js with functionality that can be encapsulated into
 *       browser widget. TEMPORARY!
 *
 * NOTE: Any changes to this routine need to be mirrored in ChromeListener::FindTitleText()
 *       (located in mozilla/embedding/browser/webBrowser/nsDocShellTreeOwner.cpp)
 *       which performs the same function, but for embedded clients that
 *       don't use a XUL/JS layer. It is important that the logic of
 *       these two routines be kept more or less in sync.
 *       (pinkerton)
 **/
function FillInHTMLTooltip(tipElement)
{
  const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  const XLinkNS = "http://www.w3.org/1999/xlink";
  const Node = { ELEMENT_NODE : 1 }; // XXX Components.interfaces.Node;

  var retVal = false;
  var tipNode = document.getElementById("HTML_TOOLTIP_tooltipBox");
  try {
    while (tipNode.hasChildNodes())
      tipNode.removeChild(tipNode.firstChild);

    var titleText = "";
    var XLinkTitleText = "";

    while (!titleText && !XLinkTitleText && tipElement) {
      if (tipElement.nodeType == Node.ELEMENT_NODE) {
        titleText = tipElement.getAttribute("title");
        XLinkTitleText = tipElement.getAttributeNS(XLinkNS, "title");
      }
      tipElement = tipElement.parentNode;
    }

    var texts = [titleText, XLinkTitleText];

    for (var i = 0; i < texts.length; ++i) {
      var t = texts[i];
      if (t.search(/\S/) >= 0) {
        var tipLineElem = tipNode.ownerDocument.createElementNS(XULNS, "text");
        tipLineElem.setAttribute("value", t);
        tipNode.appendChild(tipLineElem);

        retVal = true;
      }
    }
  } catch (e) {
  }

  return retVal;
}

/**
 * Use Stylesheet functions.
 *     Written by Tim Hill (bug 6782)
 **/
function stylesheetFillPopup(forDocument, menuPopup, itemNoOptStyles)
{
  var children = menuPopup.childNodes;
  var i;
  for (i = 0; i < children.length; ++i) {
    var child = children[i];
    if (child.getAttribute("class") == "authssmenuitem")
      menuPopup.removeChild(child);
  }

  var noOptionalStyles = true;
  var styleSheets = forDocument.styleSheets;
  var currentStyleSheets = [];

  for (i = 0; i < styleSheets.length; ++i) {
    var currentStyleSheet = styleSheets[i];

    if (currentStyleSheet.title) {
      if (!currentStyleSheet.disabled)
        noOptionalStyles = false;

      var lastWithSameTitle = null;
      if (currentStyleSheet.title in currentStyleSheets)
        lastWithSameTitle = currentStyleSheets[currentStyleSheet.title];

      if (!lastWithSameTitle) {
        var menuItem = document.createElement("menuitem");
        menuItem.setAttribute("label", currentStyleSheet.title);
        menuItem.setAttribute("data", currentStyleSheet.title);
        menuItem.setAttribute("type", "radio");
        menuItem.setAttribute("checked", !currentStyleSheet.disabled);
        menuItem.setAttribute("class", "authssmenuitem");
        menuItem.setAttribute("name", "authorstyle");
        menuItem.setAttribute("oncommand", "stylesheetSwitch(_content.document, this.getAttribute('data'))");
        menuPopup.appendChild(menuItem);
        currentStyleSheets[currentStyleSheet.title] = menuItem;
      } else {
        if (currentStyleSheet.disabled)
          lastWithSameTitle.setAttribute("checked", false);
      }
    }
  }
  itemNoOptStyles.setAttribute("checked", noOptionalStyles);
}

function stylesheetSwitch(forDocument, title)
{
  var docStyleSheets = forDocument.styleSheets;

  for (var i = 0; i < docStyleSheets.length; ++i) {
    var docStyleSheet = docStyleSheets[i];

    if (docStyleSheet.title)
      docStyleSheet.disabled = (docStyleSheet.title != title);
    else if (docStyleSheet.disabled)
      docStyleSheet.disabled = false;
  }
}

function applyTheme(themeName)
{
  var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                                 .getService(Components.interfaces.nsIChromeRegistry);

  chromeRegistry.selectSkin(themeName.getAttribute("name"), true);
  chromeRegistry.refreshSkins();
}

function getNewThemes()
{
  loadURI(gBrandRegionBundle.getString("getNewThemesURL"));
}

function URLBarMouseupHandler(aEvent)
{
  if (aEvent.button == 0 && pref.GetBoolPref("browser.urlbar.clickSelectsAll")) {
    var selectionLen = gURLBar.selectionEnd - gURLBar.selectionStart;
    if (selectionLen == 0)
      gURLBar.setSelectionRange(0, gURLBar.textLength);
  }
}

function URLBarBlurHandler(aEvent)
{
  if (pref.GetBoolPref("browser.urlbar.clickSelectsAll"))
    gURLBar.setSelectionRange(0, 0);
}

// This function gets the "windows hooks" service and has it check its setting
// This will do nothing on platforms other than Windows.
function checkForDefaultBrowser()
{
  try {
    Components.classes["@mozilla.org/winhooks;1"]
              .getService(Components.interfaces.nsIWindowsHooks)
              .checkSettings(window);
  } catch(e) {
  }
}

function ShowAndSelectContentsOfURLBar()
{
  var navBar = document.getElementById("nav-bar");
  
  // If it's hidden, show it.
  if (navBar.getAttribute("hidden") == "true")
    goToggleToolbar('nav-bar','cmd_viewnavbar');
	
  if (gURLBar.value)
    gURLBar.select();
  else
    gURLBar.focus();
}

// If "ESC" is pressed in the url bar, we replace the urlbar's value with the url of the page
// and highlight it, unless it is about:blank, where we reset it to "".
function handleURLBarRevert()
{
  var url = _content.location.href;
  var throbberElement = document.getElementById("navigator-throbber");

  var isScrolling = gURLBar.userAction == "scrolling";
  
  // don't revert to last valid url unless page is NOT loading
  // and user is NOT key-scrolling through autocomplete list
  if (!throbberElement.getAttribute("busy") && !isScrolling) {
    if (url != "about:blank") { 
      gURLBar.value = url;
      gURLBar.select();
    } else { //if about:blank, urlbar becomes ""
      gURLBar.value = "";
    }
    SetPageProxyState("valid");
  }

  // tell widget to revert to last typed text only if the user
  // was scrolling when they hit escape
  return isScrolling; 
}

function handleURLBarCommand(aUserAction)
{
//  if (aUserAction == "typing")
//    addToUrlbarHistory();

  BrowserLoadURL(); 
}

function UpdatePageProxyState()
{
  if (gURLBar.value != gLastValidURL)
    SetPageProxyState("invalid");
}

function SetPageProxyState(aState)
{
  if (!gProxyButton)
    gProxyButton = document.getElementById("page-proxy-button");

  gProxyButton.setAttribute("pageproxystate", aState);

  if (aState == "valid") {
    gLastValidURL = gURLBar.value;
    gURLBar.addEventListener("input", UpdatePageProxyState, false);
  } else if (aState == "invalid")
    gURLBar.removeEventListener("input", UpdatePageProxyState, false);
  
}

function PageProxyDragGesture(aEvent)
{
  if (gProxyButton.getAttribute("pageproxystate") == "valid")
    nsDragAndDrop.startDrag(aEvent, proxyIconDNDObserver);
  else
    return false;
}

