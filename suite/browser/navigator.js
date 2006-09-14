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
 *   Dean Tessman <dean_tessman@hotmail.com>
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

const XREMOTESERVICE_CONTRACTID = "@mozilla.org/browser/xremoteservice;1";
var gURLBar = null;
var gProxyButton = null;
var gProxyFavIcon = null;
var gProxyDeck = null;
var gBookmarksService = null;
var gSearchService = null;
var gNavigatorBundle;
var gBrandBundle;
var gNavigatorRegionBundle;
var gBrandRegionBundle;
var gLastValidURLStr = "";
var gLastValidURL = null;
var gHaveUpdatedToolbarState = false;
var gClickSelectsAll = false;
var gIgnoreFocus = false;
var gIgnoreClick = false;
var gURIFixup = null;

var pref = null;

var appCore = null;

//cached elements
var gBrowser = null;

// focused frame URL
var gFocusedURL = null;
var gFocusedDocument = null;

// Pref listener constants
const gButtonPrefListener =
{
  domain: "browser.toolbars.showbutton",
  observe: function(subject, topic, prefName)
  {
    // verify that we're changing a button pref
    if (topic != "nsPref:changed")
      return;

    var buttonName = prefName.substr(this.domain.length+1);
    var buttonId = buttonName + "-button";
    var button = document.getElementById(buttonId);

    // We need to explicitly set "hidden" to "false"
    // in order for persistence to work correctly
    var show = pref.getBoolPref(prefName);
    if (show)
      button.setAttribute("hidden","false");
    else
      button.setAttribute("hidden", "true");

    // If all buttons before the separator are hidden, also hide the separator
    if (allLeftButtonsAreHidden())
      document.getElementById("home-bm-separator").setAttribute("hidden", "true");
    else
      document.getElementById("home-bm-separator").removeAttribute("hidden");
  }
};

const gTabStripPrefListener =
{
  domain: "browser.tabs.autoHide",
  observe: function(subject, topic, prefName)
  {
    // verify that we're changing the tab browser strip auto hide pref
    if (topic != "nsPref:changed")
      return;

    var stripVisibility = !pref.getBoolPref(prefName);
    if (gBrowser.mTabContainer.childNodes.length == 1) {
      gBrowser.setStripVisibilityTo(stripVisibility);
      pref.setBoolPref("browser.tabs.forceHide", false);
    }
  }
};

const gHomepagePrefListener =
{
  domain: "browser.startup.homepage",
  observe: function(subject, topic, prefName)
  {
    // verify that we're changing the home page pref
    if (topic != "nsPref:changed")
      return;

    updateHomeButtonTooltip();
  }
};

// popup window permission change listener
const gPopupPermListener = {

  observe: function(subject, topic, data) {
    if (topic == "popup-perm-close") {
      // close the window if we're a popup and our opener's URI matches
      // the URI in the notification
      var popupOpenerURI = maybeInitPopupContext();
      if (popupOpenerURI) {
        const IOS = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);
        closeURI = IOS.newURI(data, null, null);
        if (closeURI.host == popupOpenerURI.host)
          window.close();
      }
    }
  }
};

/**
* Pref listener handler functions.
* Both functions assume that observer.domain is set to 
* the pref domain we want to start/stop listening to.
*/
function addPrefListener(observer)
{
  try {
    var pbi = pref.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
    pbi.addObserver(observer.domain, observer, false);
  } catch(ex) {
    dump("Failed to observe prefs: " + ex + "\n");
  }
}

function removePrefListener(observer)
{
  try {
    var pbi = pref.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
    pbi.removeObserver(observer.domain, observer);
  } catch(ex) {
    dump("Failed to remove pref observer: " + ex + "\n");
  }
}

function addPopupPermListener(observer)
{
  const OS = Components.classes["@mozilla.org/observer-service;1"]
             .getService(Components.interfaces.nsIObserverService);
  OS.addObserver(observer, "popup-perm-close", false);
}

function removePopupPermListener(observer)
{
  const OS = Components.classes["@mozilla.org/observer-service;1"]
             .getService(Components.interfaces.nsIObserverService);
  OS.removeObserver(observer, "popup-perm-close");
}

/**
* We can avoid adding multiple load event listeners and save some time by adding
* one listener that calls all real handlers.
*/

function loadEventHandlers(event)
{
  // Filter out events that are not about the document load we are interested in
  if (event.originalTarget == _content.document) {
    UpdateBookmarksLastVisitedDate(event);
    UpdateInternetSearchResults(event);
    checkForDirectoryListing();
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
  if (!content || !content.frames.length || !isContentFrame(document.commandDispatcher.focusedWindow))
    saveFrameItem.setAttribute("hidden", "true");
  else
    saveFrameItem.removeAttribute("hidden");
}

// When a content area frame is focused, update the focused frame URL
function contentAreaFrameFocus()
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (isContentFrame(focusedWindow)) {
    gFocusedURL = Components.lookupMethod(focusedWindow, 'location').call(focusedWindow).href;
    gFocusedDocument = focusedWindow.document;
  }
}

function updateHomeButtonTooltip()
{
  const XUL_NAMESPACE = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  var homePage = getHomePage();
  var tooltip = document.getElementById("home-button-tooltip-inner");

  while (tooltip.firstChild)
    tooltip.removeChild(tooltip.firstChild);

  for (var i in homePage) {
    var label = document.createElementNS(XUL_NAMESPACE, "label");
    label.setAttribute("value", homePage[i]);
    tooltip.appendChild(label);
  }
}

//////////////////////////////// BOOKMARKS ////////////////////////////////////

function UpdateBookmarksLastVisitedDate(event)
{
  var url = getWebNavigation().currentURI.spec;
  if (url) {
    // if the URL is bookmarked, update its "Last Visited" date
    if (!gBookmarksService)
      gBookmarksService = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                                    .getService(Components.interfaces.nsIBookmarksService);

    gBookmarksService.updateLastVisitedDate(url, _content.document.characterSet);
  }
}

function HandleBookmarkIcon(iconURL, addFlag)
{
  var url = getWebNavigation().currentURI.spec
  if (url) {
    // update URL with new icon reference
    if (!gBookmarksService)
      gBookmarksService = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                                    .getService(Components.interfaces.nsIBookmarksService);
    if (addFlag)    gBookmarksService.updateBookmarkIcon(url, iconURL);
    else            gBookmarksService.removeBookmarkIcon(url, iconURL);
  }
}

function UpdateInternetSearchResults(event)
{
  var url = getWebNavigation().currentURI.spec;
  if (url) {
    try {
      var autoOpenSearchPanel = 
        pref.getBoolPref("browser.search.opensidebarsearchpanel");

      if (autoOpenSearchPanel || isSearchPanelOpen())
      {
        if (!gSearchService)
          gSearchService = Components.classes["@mozilla.org/rdf/datasource;1?name=internetsearch"]
                                         .getService(Components.interfaces.nsIInternetSearchService);

        var searchInProgressFlag = gSearchService.FindInternetSearchResults(url);

        if (searchInProgressFlag) {
          if (autoOpenSearchPanel)
            RevealSearchPanel();
        }
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

function getHomePage()
{
  var URIs = [];
  try {
    URIs[0] = pref.getComplexValue("browser.startup.homepage",
                                   Components.interfaces.nsIPrefLocalizedString).data;
    var count = pref.getIntPref("browser.startup.homepage.count");
    for (var i = 1; i < count; ++i) {
      URIs[i] = pref.getComplexValue("browser.startup.homepage."+i,
                                     Components.interfaces.nsIPrefLocalizedString).data;
    }
  } catch(e) {
  }

  return URIs;
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

  var backDisabled = backBroadcaster.hasAttribute("disabled");
  var forwardDisabled = forwardBroadcaster.hasAttribute("disabled");
  if (backDisabled == webNavigation.canGoBack) {
    if (backDisabled)
      backBroadcaster.removeAttribute("disabled");
    else
      backBroadcaster.setAttribute("disabled", true);
  }
  if (forwardDisabled == webNavigation.canGoForward) {
    if (forwardDisabled)
      forwardBroadcaster.removeAttribute("disabled");
    else
      forwardBroadcaster.setAttribute("disabled", true);
  }
}

// Function allLeftButtonsAreHidden
// Returns true if all the buttons left of the separator in the personal
// toolbar are hidden, false otherwise.
// Used by nsButtonPrefListener to hide the separator if needed
function allLeftButtonsAreHidden()
{
  var buttonNode = document.getElementById("PersonalToolbar").firstChild;
  while(buttonNode.tagName != "toolbarseparator") {
    if(!buttonNode.hasAttribute("hidden") || buttonNode.getAttribute("hidden") == "false")
      return false;
    buttonNode = buttonNode.nextSibling;
  }
  return true;
}

function RegisterTabOpenObserver()
{
  const observer = {
    observe: function(subject, topic, data)
    {
      if (topic != "open-new-tab-request" || subject != window)
        return;

      delayedOpenTab(data);
    }
  };

  var service = Components.classes["@mozilla.org/observer-service;1"]
    .getService(Components.interfaces.nsIObserverService);
  service.addObserver(observer, "open-new-tab-request", false);
  // Null out service variable so the closure of the observer doesn't
  // own the service and create a cycle (bug 170022).
  service = null;
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
  
  SetPageProxyState("invalid", null);

  var webNavigation;
  try {
    // Create the browser instance component.
    appCore = Components.classes["@mozilla.org/appshell/component/browser/instance;1"]
                        .createInstance(Components.interfaces.nsIBrowserInstance);
    if (!appCore)
      throw "couldn't create a browser instance";

    // Get the preferences service
    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                                .getService(Components.interfaces.nsIPrefService);
    pref = prefService.getBranch(null);

    webNavigation = getWebNavigation();
    if (!webNavigation)
      throw "no XBL binding for browser";
  } catch (e) {
    alert("Error launching browser window:" + e);
    window.close(); // Give up.
    return;
  }

  // Do all UI building here:

  // set home button tooltip text
  updateHomeButtonTooltip();

  // initialize observers and listeners
  window.XULBrowserWindow = new nsBrowserStatusHandler();

  addPrefListener(gButtonPrefListener); 
  addPrefListener(gTabStripPrefListener);
  addPrefListener(gHomepagePrefListener);
  addPopupPermListener(gPopupPermListener);

  window.browserContentListener =
    new nsBrowserContentListener(window, getBrowser());
  
  // Initialize browser instance..
  appCore.setWebShellWindow(window);

  // Add a capturing event listener to the content area
  // (rjc note: not the entire window, otherwise we'll get sidebar pane loads too!)
  //  so we'll be notified when onloads complete.
  var contentArea = document.getElementById("appcontent");
  contentArea.addEventListener("load", loadEventHandlers, true);
  contentArea.addEventListener("focus", contentAreaFrameFocus, true);

  var turboMode = false;
  // set default character set if provided
  if ("arguments" in window && window.arguments.length > 1 && window.arguments[1]) {
    if (window.arguments[1].indexOf("charset=") != -1) {
      var arrayArgComponents = window.arguments[1].split("=");
      if (arrayArgComponents) {
        //we should "inherit" the charset menu setting in a new window
        getMarkupDocumentViewer().defaultCharacterSet = arrayArgComponents[1];
      }
    } else if (window.arguments[1].indexOf("turbo=yes") != -1) {
      turboMode = true;
    }
  }

  //initConsoleListener();

  // XXXjag work-around for bug 113076
  // there's another bug where we throw an exception when getting
  // sessionHistory if it is null, which I'm exploiting here to
  // detect the situation described in bug 113076.
  // The same problem caused bug 139522, also worked around below.
  try {
    getBrowser().sessionHistory;
  } catch (e) {
    // sessionHistory wasn't set from the browser's constructor
    // so we'll just have to set it here.
 
    // Wire up session and global history before any possible
    // progress notifications for back/forward button updating
    webNavigation.sessionHistory = Components.classes["@mozilla.org/browser/shistory;1"]
                                             .createInstance(Components.interfaces.nsISHistory);

    // wire up global history.  the same applies here.
    var globalHistory = Components.classes["@mozilla.org/browser/global-history;1"]
                                  .getService(Components.interfaces.nsIGlobalHistory);
    getBrowser().docShell.QueryInterface(Components.interfaces.nsIDocShellHistory).globalHistory = globalHistory;

    const selectedBrowser = getBrowser().selectedBrowser;
    if (selectedBrowser.securityUI)
      selectedBrowser.securityUI.init(selectedBrowser.contentWindow);
  }

  // hook up UI through progress listener
  getBrowser().addProgressListener(window.XULBrowserWindow, Components.interfaces.nsIWebProgress.NOTIFY_ALL);

  // load appropriate initial page from commandline
  var isPageCycling = false;

  // page cycling for tinderbox tests
  if (!appCore.cmdLineURLUsed)
    isPageCycling = appCore.startPageCycler();

  // only load url passed in when we're not page cycling
  if (!isPageCycling) {
    var uriToLoad;

    // Check for window.arguments[0]. If present, use that for uriToLoad.
    if ("arguments" in window && window.arguments.length >= 1 && window.arguments[0]) {
      var uriArray = window.arguments[0].toString().split('\n'); // stringify and split
      uriToLoad = uriArray.splice(0, 1)[0];
      if (uriArray.length > 0)
        window.setTimeout(function(arg) { for (var i in arg) gBrowser.addTab(arg[i]); }, 0, uriArray);
    }
    
    if (uriToLoad && uriToLoad != "about:blank") {
      gURLBar.value = uriToLoad;
      if ("arguments" in window && window.arguments.length >= 3) {
        loadURI(uriToLoad, window.arguments[2]);
      } else {
        loadURI(uriToLoad);
      }
    }

    // Close the window now, if it's for turbo mode startup.
    if ( turboMode ) {
        // Set "command line used" flag.  If we don't do this, then when a cmd line url
        // for a "real* invocation comes in, we will override it with the "cmd line url"
        // from the turbo-mode process (i.e., the home page).
        appCore.cmdLineURLUsed = true;
        // For some reason, window.close() directly doesn't work, so do it in the future.
        window.setTimeout( "window.close()", 100 );
        return;
    }

    // Focus the content area unless we're loading a blank page
    var navBar = document.getElementById("nav-bar");
    if (uriToLoad == "about:blank" && !navBar.hidden && window.locationbar.visible)
      setTimeout(WindowFocusTimerCallback, 0, gURLBar);
    else
      setTimeout(WindowFocusTimerCallback, 0, _content);

    // Perform default browser checking (after window opens).
    setTimeout( checkForDefaultBrowser, 0 );

    // hook up remote support
    if (XREMOTESERVICE_CONTRACTID in Components.classes) {
      var remoteService;
      remoteService = Components.classes[XREMOTESERVICE_CONTRACTID]
                                .getService(Components.interfaces.nsIXRemoteService);
      remoteService.addBrowserInstance(window);

      RegisterTabOpenObserver();
    }
  }
  
  // called when we go into full screen, even if it is 
  // initiated by a web page script
  addEventListener("fullscreen", onFullScreen, false);

  // does clicking on the urlbar select its contents?
  gClickSelectsAll = pref.getBoolPref("browser.urlbar.clickSelectsAll");

  // now load bookmarks after a delay
  setTimeout(LoadBookmarksCallback, 0);
}

function LoadBookmarksCallback()
{
  try {
    if (!gBookmarksService)
      gBookmarksService = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                                    .getService(Components.interfaces.nsIBookmarksService);
    gBookmarksService.ReadBookmarks();
    // tickle personal toolbar to load personal toolbar items
    var personalToolbar = document.getElementById("NC:PersonalToolbarFolder");
    personalToolbar.builder.rebuild();
  } catch (e) {
  }
}

function WindowFocusTimerCallback(element)
{
  // This fuction is a redo of the fix for jag bug 91884
  var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                     .getService(Components.interfaces.nsIWindowWatcher);
  if (window == ww.activeWindow) {
    element.focus();
  } else {
    // set the element in command dispatcher so focus will restore properly
    // when the window does become active
    if (element instanceof Components.interfaces.nsIDOMElement)
      document.commandDispatcher.focusedElement = element;
    else if (element instanceof Components.interfaces.nsIDOMWindow)
      document.commandDispatcher.focusedWindow = element;
  }
}

function BrowserFlushBookmarksAndHistory()
{
  // Flush bookmarks and history (used when window closes or is cached).
  try {
    // If bookmarks are dirty, flush 'em to disk
    var bmks = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                         .getService(Components.interfaces.nsIRDFRemoteDataSource);
    bmks.Flush();

    // give history a chance at flushing to disk also
    var history = Components.classes["@mozilla.org/browser/global-history;1"]
                            .getService(Components.interfaces.nsIRDFRemoteDataSource);
    history.Flush();
  } catch(ex) {
  }
}

function Shutdown()
{
  // remove remote support
  if (XREMOTESERVICE_CONTRACTID in Components.classes) {
    var remoteService;
    remoteService = Components.classes[XREMOTESERVICE_CONTRACTID]
                              .getService(Components.interfaces.nsIXRemoteService);
    remoteService.removeBrowserInstance(window);
  }

  try {
    getBrowser().removeProgressListener(window.XULBrowserWindow);
  } catch (ex) {
  }

  window.XULBrowserWindow.destroy();
  window.XULBrowserWindow = null;

  BrowserFlushBookmarksAndHistory();

  // unregister us as a pref listener
  removePrefListener(gButtonPrefListener);
  removePrefListener(gTabStripPrefListener);
  removePrefListener(gHomepagePrefListener);
  removePopupPermListener(gPopupPermListener);

  window.browserContentListener.close();
  // Close the app core.
  if (appCore)
    appCore.close();
}

function Translate()
{
  var service = pref.getCharPref("browser.translation.service");
  var serviceDomain = pref.getCharPref("browser.translation.serviceDomain");
  var targetURI = getWebNavigation().currentURI.spec;

  // if we're already viewing a translated page, then just reload
  if (targetURI.indexOf(serviceDomain) >= 0)
    BrowserReload();
  else {
    loadURI(service + escape(targetURI));
  }
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
}

function BrowserForward()
{
  try {
    getWebNavigation().goForward();
  }
  catch(ex) {
  }
}

function BrowserBackMenu(event)
{
  return FillHistoryMenu(event.target, "back");
}

function BrowserForwardMenu(event)
{
  return FillHistoryMenu(event.target, "forward");
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

function BrowserHome()
{
  var homePage = getHomePage();
  if (homePage.length == 1) {
    loadURI(homePage[0]);
  } else {
    for (var i in homePage)
      gBrowser.addTab(homePage[i]);
  }
}

function OpenBookmarkGroup(element, datasource)
{
  if (!datasource)
    return;
    
  var id = element.getAttribute("id");
  var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                          .getService(Components.interfaces.nsIRDFService);
  var resource = rdf.GetResource(id, true);
  OpenBookmarkGroupFromResource(resource, datasource, rdf);
}

function OpenBookmarkGroupFromResource(resource, datasource, rdf) {
  var urlResource = rdf.GetResource("http://home.netscape.com/NC-rdf#URL");
  var rdfContainer = Components.classes["@mozilla.org/rdf/container;1"].getService(Components.interfaces.nsIRDFContainer);
  rdfContainer.Init(datasource, resource);
  var containerChildren = rdfContainer.GetElements();
  var tabPanels = gBrowser.mPanelContainer.childNodes;
  var tabCount = tabPanels.length;
  var index = 0;
  for (; containerChildren.hasMoreElements(); ++index) {
    var res = containerChildren.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
    var target = datasource.GetTarget(res, urlResource, true);
    if (target) {
      var uri = target.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
      gBrowser.addTab(uri);
    }
  }  
  
  if (index == 0)
    return; // If the bookmark group was completely invalid, just bail.
     
  // Select the first tab in the group if we aren't loading in the background.
  if (!pref.getBoolPref("browser.tabs.loadInBackground")) {
    var tabs = gBrowser.mTabContainer.childNodes;
    gBrowser.selectedTab = tabs[tabCount];
  }
}

function OpenBookmarkURL(node, datasources)
{
  if (node.getAttribute("group") == "true")
    OpenBookmarkGroup(node, datasources);
    
  if (node.getAttribute("container") == "true")
    return;

  var url = node.getAttribute("id");
  if (!url) // if empty url (most likely a normal menu item like "Manage Bookmarks",
    return; // don't bother loading it
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
  if (_content) {
    loadURI(url);
    _content.focus();
  }
  else
    openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", url);
}

function addBookmarkAs()
{
  const browsers = gBrowser.browsers;
  if (browsers.length > 1)
    BookmarksUtils.addBookmarkForTabBrowser(gBrowser);
  else
    BookmarksUtils.addBookmarkForBrowser(gBrowser.webNavigation, true);
}

function addGroupmarkAs()
{
  BookmarksUtils.addBookmarkForTabBrowser(gBrowser, true);
}

function updateGroupmarkMenuitem(id)
{
  const disabled = gBrowser.browsers.length == 1;
  document.getElementById(id).setAttribute("disabled", disabled);
}

function readRDFString(aDS,aRes,aProp)
{
  var n = aDS.GetTarget(aRes, aProp, true);
  return n ? n.QueryInterface(Components.interfaces.nsIRDFLiteral).Value : "";
}


function ensureDefaultEnginePrefs(aRDF,aDS) 
{
  var mPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  var defaultName = mPrefs.getComplexValue("browser.search.defaultenginename", Components.interfaces.nsIPrefLocalizedString).data;
  var kNC_Root = aRDF.GetResource("NC:SearchEngineRoot");
  var kNC_child = aRDF.GetResource("http://home.netscape.com/NC-rdf#child");
  var kNC_Name = aRDF.GetResource("http://home.netscape.com/NC-rdf#Name");
          
  var arcs = aDS.GetTargets(kNC_Root, kNC_child, true);
  while (arcs.hasMoreElements()) {
    var engineRes = arcs.getNext().QueryInterface(Components.interfaces.nsIRDFResource);       
    var name = readRDFString(aDS, engineRes, kNC_Name);
    if (name == defaultName)
      mPrefs.setCharPref("browser.search.defaultengine", engineRes.Value);
  }
}

function ensureSearchPref()
{
  var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
  var ds = rdf.GetDataSource("rdf:internetsearch");
  var mPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  var kNC_Name = rdf.GetResource("http://home.netscape.com/NC-rdf#Name");
  var defaultEngine;
  try {
    defaultEngine = mPrefs.getCharPref("browser.search.defaultengine");
  } catch(ex) {
    ensureDefaultEnginePrefs(rdf, ds);
    defaultEngine = mPrefs.getCharPref("browser.search.defaultengine");
  }
}

function getSearchUrl(attr)
{
  var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService); 
  var ds = rdf.GetDataSource("rdf:internetsearch"); 
  var kNC_Root = rdf.GetResource("NC:SearchEngineRoot");
  var mPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  var defaultEngine = mPrefs.getCharPref("browser.search.defaultengine");
  var engineRes = rdf.GetResource(defaultEngine);
  var prop = "http://home.netscape.com/NC-rdf#" + attr;
  var kNC_attr = rdf.GetResource(prop);
  var searchURL = readRDFString(ds, engineRes, kNC_attr);
  return searchURL;
}

function QualifySearchTerm()
{
  // If the text in the URL bar is the same as the currently loaded
  // page's URL then treat this as an empty search term.  This way
  // the user is taken to the search page where s/he can enter a term.
  if (window.XULBrowserWindow.userTyped.value)
    return document.getElementById("urlbar").value;
  return "";
}

function OpenSearch(tabName, forceDialogFlag, searchStr, newWindowFlag)
{
  //This function needs to be split up someday.

  var autoOpenSearchPanel = false;
  var defaultSearchURL = null;
  var fallbackDefaultSearchURL = gNavigatorRegionBundle.getString("fallbackDefaultSearchURL");
  ensureSearchPref()
  //Check to see if search string contains "://" or "ftp." or white space.
  //If it does treat as url and match for pattern
  
  var urlmatch= /(:\/\/|^ftp\.)[^ \S]+$/ 
  var forceAsURL = urlmatch.test(searchStr);

  try {
    autoOpenSearchPanel = pref.getBoolPref("browser.search.opensidebarsearchpanel");
    defaultSearchURL = pref.getComplexValue("browser.search.defaulturl",
                                            Components.interfaces.nsIPrefLocalizedString).data;
  } catch (ex) {
  }

  // Fallback to a default url (one that we can get sidebar search results for)
  if (!defaultSearchURL)
    defaultSearchURL = fallbackDefaultSearchURL;

  if (!searchStr) {
    BrowserSearchInternet();
  } else {

    //Check to see if location bar field is a url
    //If it is a url go to URL.  A Url is "://" or "." as commented above
    //Otherwise search on entry
    if (forceAsURL) {
       BrowserLoadURL()
    } else {
      var searchMode = 0;
      try {
        searchMode = pref.getIntPref("browser.search.powermode");
      } catch(ex) {
      }

      if (forceDialogFlag || searchMode == 1) {
        // Use a single search dialog
        var windowManager = Components.classes["@mozilla.org/appshell/window-mediator;1"]
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
            var searchEngineURI = pref.getCharPref("browser.search.defaultengine");
            if (searchEngineURI) {          
              var searchURL = getSearchUrl("actionButton");
              if (searchURL) {
                defaultSearchURL = searchURL + escapedSearchStr; 
              } else {
                searchURL = searchDS.GetInternetSearchURL(searchEngineURI, escapedSearchStr, 0, 0, {value:0});
                if (searchURL)
                  defaultSearchURL = searchURL;
              }
            }
          } catch (ex) {
          }

          if (!newWindowFlag)
            loadURI(defaultSearchURL);
          else
            window.open(defaultSearchURL, "_blank");
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

function isSearchPanelOpen()
{
  return ( !sidebar_is_hidden()    && 
           !sidebar_is_collapsed() && 
           SidebarGetLastSelectedPanel() == "urn:sidebar:panel:search"
         );
}

function BrowserSearchInternet()
{
  try {
    var searchEngineURI = pref.getCharPref("browser.search.defaultengine");
    if (searchEngineURI) {          
      var searchRoot = getSearchUrl("searchForm");
      if (searchRoot) {
        loadURI(searchRoot);
        return;
      } else {
        // Get a search URL and guess that the front page of the site has a search form.
        var searchDS = Components.classes["@mozilla.org/rdf/datasource;1?name=internetsearch"]
                                 .getService(Components.interfaces.nsIInternetSearchService);
        searchURL = searchDS.GetInternetSearchURL(searchEngineURI, "ABC", 0, 0, {value:0});
        if (searchURL) {
          searchRoot = searchURL.match(/[a-z]+:\/\/[a-z.-]+/);
          if (searchRoot) {
            loadURI(searchRoot + "/");
            return;
          }
        }
      }
    }
  } catch (ex) {
  }

  // Fallback if the stuff above fails: use the hard-coded search engine
  loadURI(gNavigatorRegionBundle.getString("otherSearchURL"));
}


//Note: BrowserNewEditorWindow() was moved to globalOverlay.xul and renamed to NewEditorWindow()

function BrowserOpenWindow()
{
  //opens a window where users can select a web location to open
  openDialog("chrome://communicator/content/openLocation.xul", "_blank", "chrome,modal,titlebar", window);
}

function BrowserOpenTab()
{
  if (!gInPrintPreviewMode) {
    gBrowser.selectedTab = gBrowser.addTab('about:blank');
    setTimeout("gURLBar.focus();", 0); 
  }
}

/* Called from the openLocation dialog. This allows that dialog to instruct
   its opener to open a new window and then step completely out of the way.
   Anything less byzantine is causing horrible crashes, rather believably,
   though oddly only on Linux. */
function delayedOpenWindow(chrome,flags,url)
{
  setTimeout("openDialog('"+chrome+"','_blank','"+flags+"','"+url+"')", 10);
}

/* Required because the tab needs time to set up its content viewers and get the load of
   the URI kicked off before becoming the active content area. */
function delayedOpenTab(url)
{
  setTimeout(function(aTabElt) { getBrowser().selectedTab = aTabElt; }, 0, getBrowser().addTab(url));
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
  var windowManager = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                                .getService(Components.interfaces.nsIWindowMediator);

  var bookmarksWindow = windowManager.getMostRecentWindow("bookmarks:manager");

  if (bookmarksWindow) {
    bookmarksWindow.focus();
  } else {
    // while disabled, don't open new bookmarks window
    if (!gDisableBookmarks) {
      gDisableBookmarks = true;

      open("chrome://communicator/content/bookmarks/bookmarks.xul", "_blank",
        "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
      setTimeout(enableBookmarks, 2000);
    }
  }
}

function BrowserCloseTabOrWindow()
{
  var browser = getBrowser();
  if (browser && browser.localName == 'tabbrowser' && browser.mTabContainer.childNodes.length > 1) {
    // Just close up a tab.
    browser.removeCurrentTab();
    return;
  }

  BrowserCloseWindow();
}

function BrowserCloseWindow() 
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

function loadURI(uri, referrer)
{
  try {
    getWebNavigation().loadURI(uri, nsIWebNavigation.LOAD_FLAGS_NONE, referrer, null, null);
  } catch (e) {
  }
}

function BrowserLoadURL(aTriggeringEvent)
{
  var url = gURLBar.value;
  if (url.match(/^view-source:/)) {
    BrowserViewSourceOfURL(url.replace(/^view-source:/, ""), null, null);
  } else {
    // Check the pressed modifiers: (also see bug 97123)
    // Modifier Mac | Modifier PC | Action
    // -------------+-------------+-----------
    // Command      | Control     | New Window/Tab
    // Shift+Cmd    | Shift+Ctrl  | New Window/Tab behind current one
    // Option       | Shift       | Save URL (show Filepicker)

    // If false, the save modifier is Alt, which is Option on Mac.
    var modifierIsShift = true;
    try {
      modifierIsShift = pref.getBoolPref("ui.key.saveLink.shift");
    }
    catch (ex) {}

    var shiftPressed = false;
    var saveModifier = false; // if the save modifier was pressed
    if (aTriggeringEvent && 'shiftKey' in aTriggeringEvent &&
        'altKey' in aTriggeringEvent) {
      saveModifier = modifierIsShift ? aTriggeringEvent.shiftKey
                     : aTriggeringEvent.altKey;
      shiftPressed = aTriggeringEvent.shiftKey;
    }

    url = getShortcutOrURI(url);
    // Accept both Control and Meta (=Command) as New-Window-Modifiers
    if (aTriggeringEvent &&
        (('ctrlKey' in aTriggeringEvent && aTriggeringEvent.ctrlKey) ||
         ('metaKey' in aTriggeringEvent && aTriggeringEvent.metaKey))) {
      // Check if user requests Tabs instead of windows
      var openTab = false;
      try {
        openTab = pref.getBoolPref("browser.tabs.opentabfor.urlbar");
      }
      catch (ex) {}

      if (openTab && getBrowser().localName == "tabbrowser") {
        // Open link in new tab
        var t = getBrowser().addTab(url);
        // Focus new tab unless shift is pressed
        if (!shiftPressed)
          getBrowser().selectedTab = t;
      }
      else {
        // Open a new window with the URL
        var newWin = openDialog(getBrowserURL(), "_blank", "all,dialog=no", url);
        // Reset url in the urlbar, copied from handleURLBarRevert()
        var oldURL = getWebNavigation().currentURI.spec;
        if (oldURL != "about:blank") {
          gURLBar.value = oldURL;
          SetPageProxyState("valid", null);
        }
        else
          gURLBar.value = "";

        // Focus old window if shift was pressed, as there's no
        // way to open a new window in the background
        // XXX this doesn't seem to work
        if (shiftPressed) {
          //newWin.blur();
          content.focus();
        }
      }
    }
    else if (saveModifier) {
      try {
        // Firstly, fixup the url so that (e.g.) "www.foo.com" works
        const nsIURIFixup = Components.interfaces.nsIURIFixup;
        if (!gURIFixup)
          gURIFixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                                .getService(nsIURIFixup);
        url = gURIFixup.createFixupURI(url, nsIURIFixup.FIXUP_FLAGS_MAKE_ALTERNATE_URI).spec;
        // Open filepicker to save the url
        saveURL(url, "");
      }
      catch(ex) {
        // XXX Do nothing for now.
        // Do we want to put up an alert in the future?  Mmm, l10n...
      }
    }
    else {
      // No modifier was pressed, load the URL normally and
      // focus the content area
      loadURI(url);
      content.focus();
    }
  }
}

function getShortcutOrURI(url)
{
  // rjc: added support for URL shortcuts (3/30/1999)
  try {
    if (!gBookmarksService)
      gBookmarksService = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                                    .getService(Components.interfaces.nsIBookmarksService);

    var shortcutURL = gBookmarksService.resolveKeyword(url);
    if (!shortcutURL) {
      // rjc: add support for string substitution with shortcuts (4/4/2000)
      //      (see bug # 29871 for details)
      var aOffset = url.indexOf(" ");
      if (aOffset > 0) {
        var cmd = url.substr(0, aOffset);
        var text = url.substr(aOffset+1);
        shortcutURL = gBookmarksService.resolveKeyword(cmd);
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
      data = data.value.QueryInterface(Components.interfaces.nsISupportsString);
      url = data.data.substring(0, dataLen.value / 2);
    }
  } catch (ex) {
  }

  return url;
}

function OpenMessenger()
{
  open("chrome://messenger/content/messenger.xul", "_blank",
    "chrome,extrachrome,menubar,resizable,status,toolbar");
}

function OpenAddressbook()
{
  open("chrome://messenger/content/addressbook/addressbook.xul", "_blank",
    "chrome,extrachrome,menubar,resizable,status,toolbar");
}

function BrowserViewSourceOfDocument(aDocument)
{
  var docCharset;
  var pageCookie;
  var webNav;

  // Get the document charset
  docCharset = "charset=" + aDocument.characterSet;

  // Get the nsIWebNavigation associated with the document
  try {
      var win;
      var ifRequestor;

      // Get the DOMWindow for the requested document.  If the DOMWindow
      // cannot be found, then just use the _content window...
      //
      // XXX:  This is a bit of a hack...
      win = aDocument.defaultView;
      if (win == window) {
        win = _content;
      }
      ifRequestor = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor);

      webNav = ifRequestor.getInterface(Components.interfaces.nsIWebNavigation);
  } catch(err) {
      // If nsIWebNavigation cannot be found, just get the one for the whole
      // window...
      webNav = getWebNavigation();
  }
  //
  // Get the 'PageDescriptor' for the current document. This allows the
  // view-source to access the cached copy of the content rather than
  // refetching it from the network...
  //
  try{
    var PageLoader = webNav.QueryInterface(Components.interfaces.nsIWebPageDescriptor);

    pageCookie = PageLoader.currentDescriptor;
  } catch(err) {
    // If no page descriptor is available, just use the view-source URL...
  }

  BrowserViewSourceOfURL(webNav.currentURI.spec, docCharset, pageCookie);
}

function BrowserViewSourceOfURL(url, charset, pageCookie)
{
  // try to open a view-source window while inheriting the charset (if any)
  openDialog("chrome://navigator/content/viewSource.xul",
             "_blank",
             "scrollbars,resizable,chrome,dialog=no",
             url, charset, pageCookie);
}

// doc=null for regular page info, doc=owner document for frame info.
function BrowserPageInfo(doc, tab)
{
  window.openDialog("chrome://navigator/content/pageInfo.xul",
                    "_blank",
                    "chrome,dialog=no",
                    doc,
                    tab);
}

function hiddenWindowStartup()
{
  // focus the hidden window
  window.focus();

  // Disable menus which are not appropriate
  var disabledItems = ['cmd_close', 'Browser:SendPage', 'Browser:EditPage', 'Browser:PrintSetup', /*'Browser:PrintPreview',*/
                       'Browser:Print', 'canGoBack', 'canGoForward', 'Browser:Home', 'Browser:AddBookmark', 'cmd_undo',
                       'cmd_redo', 'cmd_cut', 'cmd_copy','cmd_paste', 'cmd_delete', 'cmd_selectAll', 'menu_textZoom'];
  for (var id in disabledItems) {
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

const NS_LEAKDETECTOR_CONTRACTID = "@mozilla.org/xpcom/leakdetector;1";

if (NS_LEAKDETECTOR_CONTRACTID in Components.classes) {
  try {
    LeakDetector.prototype = Components.classes[NS_LEAKDETECTOR_CONTRACTID]
                                       .createInstance(Components.interfaces.nsILeakDetector);
  } catch (err) {
    LeakDetector.prototype = Object.prototype;
  }
} else {
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

const NS_URLWIDGET_CONTRACTID = "@mozilla.org/urlwidget;1";
var urlWidgetService = null;
if (NS_URLWIDGET_CONTRACTID in Components.classes) {
  urlWidgetService = Components.classes[NS_URLWIDGET_CONTRACTID]
                               .getService(Components.interfaces.nsIUrlWidget);
}

//Posts the currently displayed url to a native widget so third-party apps can observe it.
function postURLToNativeWidget()
{
  if (urlWidgetService) {
    var url = getWebNavigation().currentURI.spec;
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
  }
}

/**
 * Use Stylesheet functions.
 *     Written by Tim Hill (bug 6782)
 *     Frameset handling by Neil Rashbrook <neil@parkwaycc.co.uk>
 **/
function getStyleSheetArray(frame)
{
  var styleSheets = frame.document.styleSheets;
  var styleSheetsArray = new Array(styleSheets.length);
  for (var i = 0; i < styleSheets.length; i++) {
    styleSheetsArray[i] = styleSheets[i];
  }
  return styleSheetsArray;
}

function getAllStyleSheets(frameset)
{
  var styleSheetsArray = getStyleSheetArray(frameset);
  for (var i = 0; i < frameset.frames.length; i++) {
    var frameSheets = getAllStyleSheets(frameset.frames[i]);
    styleSheetsArray = styleSheetsArray.concat(frameSheets);
  }
  return styleSheetsArray;
}

function stylesheetFillPopup(menuPopup)
{
  var itemNoOptStyles = menuPopup.firstChild;
  while (itemNoOptStyles.nextSibling)
    menuPopup.removeChild(itemNoOptStyles.nextSibling);

  var noOptionalStyles = true;
  var styleSheets = getAllStyleSheets(window._content);
  var currentStyleSheets = [];

  for (var i = 0; i < styleSheets.length; ++i) {
    var currentStyleSheet = styleSheets[i];

    if (currentStyleSheet.title) {
      if (!currentStyleSheet.disabled)
        noOptionalStyles = false;

      var lastWithSameTitle = null;
      if (currentStyleSheet.title in currentStyleSheets)
        lastWithSameTitle = currentStyleSheets[currentStyleSheet.title];

      if (!lastWithSameTitle) {
        var menuItem = document.createElement("menuitem");
        menuItem.setAttribute("type", "radio");
        menuItem.setAttribute("label", currentStyleSheet.title);
        menuItem.setAttribute("data", currentStyleSheet.title);
        menuItem.setAttribute("checked", !currentStyleSheet.disabled);
        menuPopup.appendChild(menuItem);
        currentStyleSheets[currentStyleSheet.title] = menuItem;
      } else {
        if (currentStyleSheet.disabled)
          lastWithSameTitle.removeAttribute("checked");
      }
    }
  }
  itemNoOptStyles.setAttribute("checked", noOptionalStyles);
}

function stylesheetInFrame(frame, title) {
  var docStyleSheets = frame.document.styleSheets;

  for (var i = 0; i < docStyleSheets.length; ++i) {
    if (docStyleSheets[i].title == title)
      return true;
  }
  return false;
}

function stylesheetSwitchFrame(frame, title) {
  var docStyleSheets = frame.document.styleSheets;

  for (var i = 0; i < docStyleSheets.length; ++i) {
    var docStyleSheet = docStyleSheets[i];

    if (docStyleSheet.title)
      docStyleSheet.disabled = (docStyleSheet.title != title);
    else if (docStyleSheet.disabled)
      docStyleSheet.disabled = false;
  }
}

function stylesheetSwitchAll(frameset, title) {
  if (!title || stylesheetInFrame(frameset, title)) {
    stylesheetSwitchFrame(frameset, title);
  }
  for (var i = 0; i < frameset.frames.length; i++) {
    stylesheetSwitchAll(frameset.frames[i], title);
  }
}

function applyTheme(themeName)
{
  var id = themeName.getAttribute('id'); 
  var name=id.substring('urn:mozilla.skin.'.length, id.length);
  if (!name)
    return;

  var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Components.interfaces.nsIXULChromeRegistry);

  var oldTheme = false;
  try {
    oldTheme = !chromeRegistry.checkThemeVersion(name);
  }
  catch(e) {
  }

  var str = Components.classes["@mozilla.org/supports-string;1"]
                      .createInstance(Components.interfaces.nsISupportsString);

  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  if (oldTheme) {
    var title = gNavigatorBundle.getString("oldthemetitle");
    var message = gNavigatorBundle.getString("oldTheme");

    message = message.replace(/%theme_name%/, themeName.getAttribute("displayName"));
    message = message.replace(/%brand%/g, gBrandBundle.getString("brandShortName"));

    if (promptService.confirm(window, title, message)){
      var inUse = chromeRegistry.isSkinSelected(name, true);

      chromeRegistry.uninstallSkin( name, true );
      // XXX - this sucks and should only be temporary.

      str.data = true;
      pref.setComplexValue("general.skins.removelist." + name,
                           Components.interfaces.nsISupportsString, str);
      
      if (inUse)
        chromeRegistry.refreshSkins();
    }

    return;
  }

 // XXX XXX BAD BAD BAD BAD !! XXX XXX                                         
 // we STILL haven't fixed editor skin switch problems                         
 // hacking around it yet again                                                

 str.data = name;
 pref.setComplexValue("general.skins.selectedSkin", Components.interfaces.nsISupportsString, str);
 
 // shut down quicklaunch so the next launch will have the new skin
 var appShell = Components.classes['@mozilla.org/appshell/appShellService;1'].getService();
 appShell = appShell.QueryInterface(Components.interfaces.nsIAppShellService);
 try {
   appShell.nativeAppSupport.isServerMode = false;
 }
 catch(ex) {
 }
 
 if (promptService) {                                                          
   var dialogTitle = gNavigatorBundle.getString("switchskinstitle");           
   var brandName = gBrandBundle.getString("brandShortName");                   
   var msg = gNavigatorBundle.getFormattedString("switchskins", [brandName]);  
   promptService.alert(window, dialogTitle, msg);                              
 }                                                                             
}

function getNewThemes()
{
  loadURI(gBrandRegionBundle.getString("getNewThemesURL"));
}

function URLBarFocusHandler(aEvent)
{
  if (gIgnoreFocus)
    gIgnoreFocus = false;
  else if (gClickSelectsAll)
    gURLBar.select();
}

function URLBarMouseDownHandler(aEvent)
{
  if (gURLBar.hasAttribute("focused")) {
    gIgnoreClick = true;
  } else {
    gIgnoreFocus = true;
    gIgnoreClick = false;
    gURLBar.setSelectionRange(0, 0);
  }
}

function URLBarClickHandler(aEvent)
{
  if (!gIgnoreClick && gClickSelectsAll && gURLBar.selectionStart == gURLBar.selectionEnd && gURLBar.selectionStart < gURLBar.value.length)
    gURLBar.select();
}

// This function gets the "windows hooks" service and has it check its setting
// This will do nothing on platforms other than Windows.
function checkForDefaultBrowser()
{
  const NS_WINHOOKS_CONTRACTID = "@mozilla.org/winhooks;1";
  var dialogShown = false;
  if (NS_WINHOOKS_CONTRACTID in Components.classes) {
    try {
      dialogShown = Components.classes[NS_WINHOOKS_CONTRACTID]
                      .getService(Components.interfaces.nsIWindowsHooks)
                      .checkSettings(window);
    } catch(e) {
    }

    if (dialogShown)  
    {
      // Force the sidebar to build since the windows 
      // integration dialog may have come up.
      SidebarRebuild();
    }
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
  var url = getWebNavigation().currentURI.spec;
  var throbberElement = document.getElementById("navigator-throbber");

  var isScrolling = gURLBar.userAction == "scrolling";
  
  // don't revert to last valid url unless page is NOT loading
  // and user is NOT key-scrolling through autocomplete list
  if (!throbberElement.hasAttribute("busy") && !isScrolling) {
    if (url != "about:blank") { 
      gURLBar.value = url;
      gURLBar.select();
      SetPageProxyState("valid", null); // XXX Build a URI and pass it in here.
    } else { //if about:blank, urlbar becomes ""
      gURLBar.value = "";
    }
  }

  // tell widget to revert to last typed text only if the user
  // was scrolling when they hit escape
  return isScrolling; 
}

function handleURLBarCommand(aUserAction, aTriggeringEvent)
{
  try { 
    addToUrlbarHistory();
  } catch (ex) {
    // Things may go wrong when adding url to session history,
    // but don't let that interfere with the loading of the url.
  }
  
  BrowserLoadURL(aTriggeringEvent); 
}

function UpdatePageProxyState()
{
  if (gURLBar.value != gLastValidURLStr)
    SetPageProxyState("invalid", null);
}

function SetPageProxyState(aState, aURI)
{
  if (!gProxyButton)
    gProxyButton = document.getElementById("page-proxy-button");
  if (!gProxyFavIcon)
    gProxyFavIcon = document.getElementById("page-proxy-favicon");
  if (!gProxyDeck)
    gProxyDeck = document.getElementById("page-proxy-deck");

  gProxyButton.setAttribute("pageproxystate", aState);

  if (aState == "valid") {
    gLastValidURLStr = gURLBar.value;
    gURLBar.addEventListener("input", UpdatePageProxyState, false);
    if (gBrowser.shouldLoadFavIcon(aURI)) {
      var favStr = gBrowser.buildFavIconString(aURI);
      if (favStr != gProxyFavIcon.src) {
        gBrowser.loadFavIcon(aURI, "src", gProxyFavIcon);
        gProxyDeck.selectedIndex = 0;
      }
      else gProxyDeck.selectedIndex = 1;
    }
    else {
      gProxyDeck.selectedIndex = 0;
      gProxyFavIcon.removeAttribute("src");
    }
  } else if (aState == "invalid") {
    gURLBar.removeEventListener("input", UpdatePageProxyState, false);
    gProxyDeck.selectedIndex = 0;
  }
}

function PageProxyDragGesture(aEvent)
{
  if (gProxyButton.getAttribute("pageproxystate") == "valid") {
    nsDragAndDrop.startDrag(aEvent, proxyIconDNDObserver);
    return true;
  }
  return false;
}

function updateComponentBarBroadcaster()
{ 
  var compBarBroadcaster = document.getElementById('cmd_viewcomponentbar');
  var taskBarBroadcaster = document.getElementById('cmd_viewtaskbar');
  var compBar = document.getElementById('component-bar');
  if (taskBarBroadcaster.getAttribute('checked') == 'true') {
    compBarBroadcaster.removeAttribute('disabled');
    if (compBar.getAttribute('hidden') != 'true')
      compBarBroadcaster.setAttribute('checked', 'true');
  }
  else {
    compBarBroadcaster.setAttribute('disabled', 'true');
    compBarBroadcaster.removeAttribute('checked');
  }
}

function updateToolbarStates(toolbarMenuElt)
{
  if (!gHaveUpdatedToolbarState) {
    var mainWindow = document.getElementById("main-window");
    if (mainWindow.hasAttribute("chromehidden")) {
      gHaveUpdatedToolbarState = true;
      var i;
      for (i = 0; i < toolbarMenuElt.childNodes.length; ++i)
        document.getElementById(toolbarMenuElt.childNodes[i].getAttribute("observes")).removeAttribute("checked");
      var toolbars = document.getElementsByTagName("toolbar");
      for (i = 0; i < toolbars.length; ++i) {
        if (toolbars[i].getAttribute("class").indexOf("chromeclass") != -1)
          toolbars[i].setAttribute("hidden", "true");
      }
      var statusbars = document.getElementsByTagName("statusbar");
      for (i = 0; i < statusbars.length; ++i) {
        if (statusbars[i].getAttribute("class").indexOf("chromeclass") != -1)
          statusbars[i].setAttribute("hidden", "true");
      }
      mainWindow.removeAttribute("chromehidden");
    }
  }
  updateComponentBarBroadcaster();

  const tabbarMenuItem = document.getElementById("menuitem_showhide_tabbar");
  // Make show/hide menu item reflect current state
  const visibility = gBrowser.getStripVisibility();
  tabbarMenuItem.setAttribute("checked", visibility);

  // Don't allow the tab bar to be shown/hidden when more than one tab is open
  // or when we have 1 tab and the autoHide pref is set
  const disabled = gBrowser.browsers.length > 1 ||
                   pref.getBoolPref("browser.tabs.autoHide");
  tabbarMenuItem.setAttribute("disabled", disabled);
}

function showHideTabbar()
{
  const visibility = gBrowser.getStripVisibility();
  pref.setBoolPref("browser.tabs.forceHide", visibility);
  gBrowser.setStripVisibilityTo(!visibility);
}

// Fill in tooltips for personal toolbar
function FillInPTTooltip(tipElement)
{

  var title = tipElement.label;
  var url = tipElement.statusText;

  if (!title && !url) {
    // bail out early if there is nothing to show
    return false;
  }

  var tooltipTitle = document.getElementById("ptTitleText");
  var tooltipUrl = document.getElementById("ptUrlText"); 

  if (title && title != url) {
    tooltipTitle.removeAttribute("hidden");
    tooltipTitle.setAttribute("value", title);
  } else  {
    tooltipTitle.setAttribute("hidden", "true");
  }

  if (url) {
    tooltipUrl.removeAttribute("hidden");
    tooltipUrl.setAttribute("value", url);
  } else {
    tooltipUrl.setAttribute("hidden", "true");
  }

  return true; // show tooltip
}

function BrowserFullScreen()
{
  window.fullScreen = !window.fullScreen;
}

function onFullScreen()
{
  FullScreen.toggle();
}

// Set up a lame hack to avoid opening two bookmarks.
// Could otherwise happen with two Ctrl-B's in a row.
var gDisableHistory = false;
function enableHistory() {
  gDisableHistory = false;
}

function toHistory()
{
  // Use a single sidebar history dialog

  var cwindowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
  var iwindowManager = Components.interfaces.nsIWindowMediator;
  var windowManager  = cwindowManager.QueryInterface(iwindowManager);

  var historyWindow = windowManager.getMostRecentWindow('history:manager');

  if (historyWindow) {
    //debug("Reuse existing history window");
    historyWindow.focus();
  } else {
    //debug("Open a new history dialog");

    if (true == gDisableHistory) {
      //debug("Recently opened one. Wait a little bit.");
      return;
    }
    gDisableHistory = true;

    window.open( "chrome://communicator/content/history/history.xul", "_blank",
        "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar" );
    setTimeout(enableHistory, 2000);
  }

}

function checkTheme()
{
  var theSkinKids = document.getElementById("theme");
  var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Components.interfaces.nsIXULChromeRegistry);
  for (var i = 0; i < theSkinKids.childNodes.length; ++i) {
    var child = theSkinKids.childNodes[i];
    var id=child.getAttribute("id");
    if (id.length > 0) {
      var themeName = id.substring('urn:mozilla:skin:'.length, id.length);       
      var selected = chromeRegistry.isSkinSelected(themeName, true);
      if (selected == Components.interfaces.nsIChromeRegistry.FULL) {
        var menuitem=document.getElementById(id);
        menuitem.setAttribute("checked", true);
        break;
      }
    }
  } 
}

// opener may not have been initialized by load time (chrome windows only)
// so call this function some time later.
function maybeInitPopupContext()
{
  // it's not a popup with no opener
  if (!window.content.opener)
    return null;

  try {
    // are we a popup window?
    const CI = Components.interfaces;
    var xulwin = window
                 .QueryInterface(CI.nsIInterfaceRequestor)
                 .getInterface(CI.nsIWebNavigation)
                 .QueryInterface(CI.nsIDocShellTreeItem).treeOwner
                 .QueryInterface(CI.nsIInterfaceRequestor)
                 .getInterface(CI.nsIXULWindow);
    if (xulwin.contextFlags &
        CI.nsIWindowCreator2.PARENT_IS_LOADING_OR_RUNNING_TIMEOUT) {
      // return our opener's URI
      const IOS = Components.classes["@mozilla.org/network/io-service;1"]
                  .getService(CI.nsIIOService);
      var spec = Components.lookupMethod(window.content.opener, "location")
                 .call();
      return IOS.newURI(spec, null, null);
    }
  } catch(e) {
  }
  return null;
}
