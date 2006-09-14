/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const XREMOTESERVICE_CONTRACTID = "@mozilla.org/browser/xremoteservice;1";
const XUL_NAMESPACE = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
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
var gClickAtEndSelects = false;
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
  init: function()
  {
    var array = pref.getChildList(this.domain, {});
    for (var i in array)
      this.updateButton(array[i]);
    this.updateSeparator();
  },
  observe: function(subject, topic, prefName)
  {
    // verify that we're changing a button pref
    if (topic != "nsPref:changed")
      return;

    this.updateButton(prefName);
    this.updateSeparator();
  },
  updateButton: function(prefName)
  {
    var buttonName = prefName.substr(this.domain.length+1);
    var buttonId = buttonName + "-button";
    var button = document.getElementById(buttonId);
    if (button)
      button.hidden = !pref.getBoolPref(prefName);
  },
  updateSeparator: function()
  {
    // If all buttons before the separator are hidden, also hide the separator
    var separator = document.getElementById("home-bm-separator");
    separator.hidden = allLeftButtonsAreHidden();
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
    if (gBrowser.tabContainer.childNodes.length == 1) {
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

const POPUP_TYPE = "popup";
const gPopupPrefListener =
{
  domain: "dom.disable_open_during_load",
  observe: function(subject, topic, prefName)
  {        
    if (topic != "nsPref:changed" || prefName != this.domain)
      return;

    var browsers = getBrowser().browsers;
    var policy = pref.getBoolPref(prefName);

    if (policy && pref.getBoolPref("privacy.popups.first_popup"))
      pref.setBoolPref("privacy.popups.first_popup", false);

    var hosts = [];

    var permManager = Components.classes["@mozilla.org/permissionmanager;1"]
                                .getService(Components.interfaces.nsIPermissionManager);
    
    var enumerator = permManager.enumerator;
    var count=0;
    while (enumerator.hasMoreElements()) {
      var permission = enumerator.getNext()
                                 .QueryInterface(Components.interfaces.nsIPermission);
      if (permission.type == POPUP_TYPE && permission.capability == policy)
        hosts[permission.host] = permission.host;
    }

    var popupIcon = document.getElementById("popupIcon");
    
    if (!policy) {
      for (var i = 0; i < browsers.length; i++) {
        if (browsers[i].popupDomain in hosts)
          break;
        browsers[i].popupDomain = null;
        popupIcon.hidden = true;
      }
    } else {
      for (i = 0; i < browsers.length; i++) {
        if (browsers[i].popupDomain in hosts) {
          browsers[i].popupDomain = null;
          popupIcon.hidden = true;
        }
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
  if (event.originalTarget == content.document) {
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
  var focusedWindow = new XPCNativeWrapper(document.commandDispatcher.focusedWindow, "top", "document", "location");
  if (focusedWindow.top == window.content) {
    gFocusedURL = focusedWindow.location.href;
    gFocusedDocument = focusedWindow.document;
  }
}

function updateHomeButtonTooltip()
{
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

    gBookmarksService.updateLastVisitedDate(url, content.document.characterSet);
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
  var browser = getBrowser();

  // Avoid setting attributes on broadcasters if the value hasn't changed!
  // Remember, guys, setting attributes on elements is expensive!  They
  // get inherited into anonymous content, broadcast to other widgets, etc.!
  // Don't do it if the value hasn't changed! - dwh

  var backDisabled = backBroadcaster.hasAttribute("disabled");
  var forwardDisabled = forwardBroadcaster.hasAttribute("disabled");
  if (backDisabled == browser.canGoBack) {
    if (backDisabled)
      backBroadcaster.removeAttribute("disabled");
    else
      backBroadcaster.setAttribute("disabled", true);
  }
  if (forwardDisabled == browser.canGoForward) {
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
  var buttonNode = document.getElementById("home-bm-separator").previousSibling;
  while (buttonNode) {
    if (buttonNode.localName != "tooltip" && !buttonNode.hidden)
      return false;
    buttonNode = buttonNode.previousSibling;
  }
  return true;
}

const gTabOpenObserver = {
  observe: function(subject, topic, data)
  {
    if (topic != "open-new-tab-request" || subject != window)
      return;

    delayedOpenTab(data);
  }
};

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

    window.tryToClose = WindowIsClosing;

    webNavigation = getWebNavigation();
    if (!webNavigation)
      throw "no XBL binding for browser";
  } catch (e) {
    alert("Error launching browser window:" + e);
    window.close(); // Give up.
    return;
  }

  // Do all UI building here:

  // Ensure button visibility matches prefs
  gButtonPrefListener.init();

  // set home button tooltip text
  updateHomeButtonTooltip();

  // initialize observers and listeners
  window.XULBrowserWindow = new nsBrowserStatusHandler();

  addPrefListener(gButtonPrefListener); 
  addPrefListener(gTabStripPrefListener);
  addPrefListener(gHomepagePrefListener);
  addPopupPermListener(gPopupPermListener);
  addPrefListener(gPopupPrefListener);  

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
    /* Session history might not be available,
       so we wrap access to it in a try block */
    try {
      // sessionHistory wasn't set from the browser's constructor
      // so we'll just have to set it here.

      // Wire up session and global history before any possible
      // progress notifications for back/forward button updating
      webNavigation.sessionHistory = Components.classes["@mozilla.org/browser/shistory;1"]
                                               .createInstance(Components.interfaces.nsISHistory);

      // enable global history
      getBrowser().docShell.QueryInterface(Components.interfaces.nsIDocShellHistory).useGlobalHistory = true;
    } catch (e) {}

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
    var uriToLoad = "";

    // Check window.arguments[0]. If not null then use it for uriArray
    // otherwise the new window is being called when another browser
    // window already exists so use the New Window pref for uriArray
    if ("arguments" in window && window.arguments.length >= 1) {
      var uriArray;
      if (window.arguments[0]) {
        uriArray = window.arguments[0].toString().split('\n'); // stringify and split
      } else {
        try {
          switch (pref.getIntPref("browser.windows.loadOnNewWindow"))
          {
            case -1:
              var handler = Components.classes['@mozilla.org/commandlinehandler/general-startup;1?type=browser']
                                      .getService(Components.interfaces.nsICmdLineHandler);
              uriArray = handler.defaultArgs.split('\n');
              break;
            default:
              uriArray = ["about:blank"];
              break;
            case 1:
              uriArray = getHomePage();
              break;
            case 2:
              var history = Components.classes["@mozilla.org/browser/global-history;2"]
                                      .getService(Components.interfaces.nsIBrowserHistory);
              uriArray = [history.lastPageVisited];
              break;
          }
        } catch(e) {
          uriArray = ["about:blank"];
        }
      }
      uriToLoad = uriArray.splice(0, 1)[0];

      if (uriArray.length > 0)
        window.setTimeout(function(arg) { for (var i in arg) gBrowser.addTab(arg[i]); }, 0, uriArray);
    }
    
    if (/^\s*$/.test(uriToLoad))
      uriToLoad = "about:blank";

    if (uriToLoad != "about:blank") {
      gURLBar.value = uriToLoad;
      getBrowser().userTypedValue = uriToLoad;
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

    // Focus the content area unless we're loading a blank page, or if
    // we weren't passed any arguments. This "breaks" the
    // javascript:window.open(); case where we don't get any arguments
    // either, but we're loading about:blank, but focusing the content
    // are is arguably correct in that case as well since the opener
    // is very likely to put some content in the new window, and then
    // the focus should be in the content area.
    var navBar = document.getElementById("nav-bar");
    if ("arguments" in window && uriToLoad == "about:blank" && !navBar.hidden && window.locationbar.visible)
      setTimeout(WindowFocusTimerCallback, 0, gURLBar);
    else
      setTimeout(WindowFocusTimerCallback, 0, content);

    // Perform default browser checking (after window opens).
    setTimeout( checkForDefaultBrowser, 0 );

    // hook up remote support
    if (XREMOTESERVICE_CONTRACTID in Components.classes) {
      var remoteService;
      remoteService = Components.classes[XREMOTESERVICE_CONTRACTID]
                                .getService(Components.interfaces.nsIXRemoteService);
      remoteService.addBrowserInstance(window);

      var observerService = Components.classes["@mozilla.org/observer-service;1"]
        .getService(Components.interfaces.nsIObserverService);
      observerService.addObserver(gTabOpenObserver, "open-new-tab-request", false);
    }
  }
  
  // called when we go into full screen, even if it is 
  // initiated by a web page script
  addEventListener("fullscreen", onFullScreen, false);

  addEventListener("PopupWindow", onPopupWindow, false);
  addEventListener("DOMPopupBlocked", onPopupBlocked, false);

  // does clicking on the urlbar select its contents?
  gClickSelectsAll = pref.getBoolPref("browser.urlbar.clickSelectsAll");
  gClickAtEndSelects = pref.getBoolPref("browser.urlbar.clickAtEndSelects");

  // now load bookmarks after a delay
  setTimeout(LoadBookmarksCallback, 0);
}

function LoadBookmarksCallback()
{
  // loads the services
  initServices();
  initBMService();
  BMSVC.readBookmarks();
  var bt = document.getElementById("bookmarks-ptf");
  if (bt) {
    bt.database.AddObserver(BookmarksToolbarRDFObserver);
  }
  window.addEventListener("resize", BookmarksToolbar.resizeFunc, false);
  controllers.appendController(BookmarksMenuController);
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

    if (element instanceof Components.interfaces.nsIDOMWindow) {
      document.commandDispatcher.focusedWindow = element;
      document.commandDispatcher.focusedElement = null;
    } else if (element instanceof Components.interfaces.nsIDOMElement) {
      document.commandDispatcher.focusedWindow = element.ownerDocument.defaultView;
      document.commandDispatcher.focusedElement = element;
    }
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
    var history = Components.classes["@mozilla.org/browser/global-history;2"]
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

    var observerService = Components.classes["@mozilla.org/observer-service;1"]
      .getService(Components.interfaces.nsIObserverService);
    observerService.removeObserver(gTabOpenObserver, "open-new-tab-request", false);
  }

  try {
    getBrowser().removeProgressListener(window.XULBrowserWindow);
  } catch (ex) {
  }

  var bt = document.getElementById("bookmarks-ptf");
  if (bt) {
    bt.database.RemoveObserver(BookmarksToolbarRDFObserver);
  }
  controllers.removeController(BookmarksMenuController);

  window.XULBrowserWindow.destroy();
  window.XULBrowserWindow = null;

  BrowserFlushBookmarksAndHistory();

  // unregister us as a pref listener
  removePrefListener(gButtonPrefListener);
  removePrefListener(gTabStripPrefListener);
  removePrefListener(gHomepagePrefListener);
  removePopupPermListener(gPopupPermListener);
  removePrefListener(gPopupPrefListener);

  window.browserContentListener.close();
  // Close the app core.
  if (appCore)
    appCore.close();
}

function Translate()
{
  var service = pref.getComplexValue("browser.translation.service",
                                     Components.interfaces.nsIPrefLocalizedString).data;
  var serviceDomain = pref.getComplexValue("browser.translation.serviceDomain",
                                           Components.interfaces.nsIPrefLocalizedString).data;
  var targetURI = getWebNavigation().currentURI.spec;

  // if we're already viewing a translated page, then just reload
  if (targetURI.indexOf(serviceDomain) >= 0)
    BrowserReload();
  else {
    loadURI(encodeURI(service + targetURI));
  }
}

function gotoHistoryIndex(aEvent)
{
  var index = aEvent.target.getAttribute("index");
  if (!index)
    return false;

  if (index == "back")
    gBrowser.goBackGroup();
  else if (index ==  "forward")
    gBrowser.goForwardGroup();
  else {
    try {
      getWebNavigation().gotoIndex(index);
    }
    catch(ex) {
      return false;
    }
  }
  return true;

}

function BrowserBack()
{
  try {
    getBrowser().goBack();
  }
  catch(ex) {
  }
}

function BrowserHandleBackspace()
{
  // The order of seeing keystrokes is this:
  // 1) Chrome, 2) Typeahead, 3) [platform]HTMLBindings.xml
  // Rather than have typeaheadfind responsible for making VK_BACK 
  // go back in history, we handle backspace it here as follows:
  // When backspace is pressed, it might mean back
  // in typeaheadfind if that's active, or it might mean back in history

  var typeAhead = null;
  const TYPE_AHEAD_FIND_CONTRACTID = "@mozilla.org/typeaheadfind;1";
  if (TYPE_AHEAD_FIND_CONTRACTID in Components.classes) {
    typeAhead = Components.classes[TYPE_AHEAD_FIND_CONTRACTID]
                .getService(Components.interfaces.nsITypeAheadFind);
  }
  
  if (!typeAhead || !typeAhead.backOneChar()) {
    BrowserBack();
  }
}

function BrowserForward()
{
  try {
    getBrowser().goForward();
  }
  catch(ex) {
  }
}

function SetGroupHistory(popupMenu, direction)
{
  while (popupMenu.firstChild)
    popupMenu.removeChild(popupMenu.firstChild);

  var menuItem = document.createElementNS(XUL_NAMESPACE, "menuitem");
  var label = gNavigatorBundle.getString("tabs.historyItem");
  menuItem.setAttribute("label", label);
  menuItem.setAttribute("index", direction);
  popupMenu.appendChild(menuItem);
}

function BrowserBackMenu(event)
{
  if (gBrowser.backBrowserGroup.length != 0) {
    SetGroupHistory(event.target, "back");
    return true;
  }

  return FillHistoryMenu(event.target, "back");
}

function BrowserForwardMenu(event)
{
  if (gBrowser.forwardBrowserGroup.length != 0) {
    SetGroupHistory(event.target, "forward");
    return true;
  }

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
    var URIs = [];
    for (var i in homePage)
      URIs.push({URI: homePage[i]});

    var tab = gBrowser.loadGroup(URIs);
    if (!pref.getBoolPref("browser.tabs.loadInBackground"))
      gBrowser.selectedTab = tab;
  }
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
  if (gBrowser.userTypedValue !== null)
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
          var escapedSearchStr = encodeURIComponent(searchStr);
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
  // first lets check if the search panel will be shown at all
  // by checking the sidebar datasource to see if there is an entry
  // for the search panel, and if it is excluded for navigator or not
  
  var searchPanelExists = false;
  
  if (document.getElementById("urn:sidebar:panel:search")) {
    var myPanel = document.getElementById("urn:sidebar:panel:search");
    var panel = sidebarObj.panels.get_panel_from_header_node(myPanel);

    searchPanelExists = !panel.is_excluded();
  } else if (sidebarObj.never_built) {

    try{
      var datasource = RDF.GetDataSourceBlocking(sidebarObj.datasource_uri);
      var aboutValue = RDF.GetResource("urn:sidebar:panel:search");

      // check if the panel is even in the list by checking for its content
      var contentProp = RDF.GetResource("http://home.netscape.com/NC-rdf#content");
      var content = datasource.GetTarget(aboutValue, contentProp, true);
     
      if (content instanceof Components.interfaces.nsIRDFLiteral){
        // the search panel entry exists, now check if it is excluded
        // for navigator
        var excludeProp = RDF.GetResource("http://home.netscape.com/NC-rdf#exclude");
        var exclude = datasource.GetTarget(aboutValue, excludeProp, true);

        if (exclude instanceof Components.interfaces.nsIRDFLiteral) {
          searchPanelExists = (exclude.Value.indexOf("navigator:browser") < 0);
        } else {
          // panel exists and no exclude set
          searchPanelExists = true;
        }
      }
    } catch(e){
      searchPanelExists = false;
    }
  }

  if (searchPanelExists) {
    // make sure the sidebar is open, else SidebarSelectPanel() will fail
    if (sidebar_is_hidden())
      SidebarShowHide();
  
    if (sidebar_is_collapsed())
      SidebarExpandCollapse();

    var searchPanel = document.getElementById("urn:sidebar:panel:search");
    if (searchPanel)
      SidebarSelectPanel(searchPanel, true, true); // lives in sidebarOverlay.js      
  }
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
        var searchURL = searchDS.GetInternetSearchURL(searchEngineURI, "ABC", 0, 0, {value:0});
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
    var uriToLoad;
    try {
      switch ( pref.getIntPref("browser.tabs.loadOnNewTab") )
      {
        case -1:
          var handler = Components.classes['@mozilla.org/commandlinehandler/general-startup;1?type=browser']
                                  .getService(Components.interfaces.nsICmdLineHandler);
          uriToLoad = handler.defaultArgs.split("\n")[0];
          if (!/\S/.test(uriToLoad))
            uriToLoad = "about:blank";
          break;
        default:
          uriToLoad = "about:blank";
          break;
        case 1:
          uriToLoad = pref.getCharPref("browser.startup.homepage");
          break;
        case 2:
          uriToLoad = getWebNavigation().currentURI.spec;
          break;
      }
    } catch(e) {
      uriToLoad = "about:blank";
    }

    gBrowser.selectedTab = gBrowser.addTab(uriToLoad);
    var navBar = document.getElementById("nav-bar");
    if (uriToLoad == "about:blank" && !navBar.hidden && window.locationbar.visible)
      setTimeout("gURLBar.focus();", 0);
    else
      setTimeout("content.focus();", 0);
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

/* Show file picker dialog configured for opening a file, and return 
 * the selected nsIFileURL instance. */
function selectFileToOpen(label, prefRoot)
{
  var fileURL = null;

  // Get filepicker component.
  const nsIFilePicker = Components.interfaces.nsIFilePicker;
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  fp.init(window, gNavigatorBundle.getString(label), nsIFilePicker.modeOpen);
  fp.appendFilters(nsIFilePicker.filterAll | nsIFilePicker.filterText | nsIFilePicker.filterImages |
                   nsIFilePicker.filterXML | nsIFilePicker.filterHTML);

  const filterIndexPref = prefRoot + "filterIndex";
  const lastDirPref = prefRoot + "dir";

  // use a pref to remember the filterIndex selected by the user.
  var index = 0;
  try {
    index = pref.getIntPref(filterIndexPref);
  } catch (ex) {
  }
  fp.filterIndex = index;

  // use a pref to remember the displayDirectory selected by the user.
  try {
    fp.displayDirectory = pref.getComplexValue(lastDirPref, Components.interfaces.nsILocalFile);
  } catch (ex) {
  }

  if (fp.show() == nsIFilePicker.returnOK) {
    pref.setIntPref(filterIndexPref, fp.filterIndex);
    pref.setComplexValue(lastDirPref,
                         Components.interfaces.nsILocalFile,
                         fp.file.parent);
    fileURL = fp.fileURL;
  }

  return fileURL;
}

function BrowserOpenFileWindow()
{
  try {
    openTopWin(selectFileToOpen("openFile", "browser.open.").spec);
  } catch (e) {}
}

function BrowserEditBookmarks()
{
  toOpenWindowByType("bookmarks:manager", "chrome://communicator/content/bookmarks/bookmarksManager.xul");
}

function updateCloseItems()
{
  var browser = getBrowser();
  if (browser.getStripVisibility()) {
    document.getElementById('menu_close').setAttribute('label', gNavigatorBundle.getString('tabs.closeTab'));
    document.getElementById('menu_closeWindow').hidden = false;
    document.getElementById('menu_closeOtherTabs').hidden = false;
    if (browser.tabContainer.childNodes.length > 1)
      document.getElementById('cmd_closeOtherTabs').removeAttribute('disabled');
    else
      document.getElementById('cmd_closeOtherTabs').setAttribute('disabled', 'true');
  } else {
    document.getElementById('menu_close').setAttribute('label', gNavigatorBundle.getString('tabs.close'));
    document.getElementById('menu_closeWindow').hidden = true;
    document.getElementById('menu_closeOtherTabs').hidden = true;
  }
}

function BrowserCloseOtherTabs()
{
  var browser = getBrowser();
  browser.removeAllTabsBut(browser.mCurrentTab);
}

function BrowserCloseTabOrWindow()
{
  var browser = getBrowser();
  if (browser.tabContainer.childNodes.length > 1) {
    // Just close up a tab.
    browser.removeCurrentTab();
    return;
  }

  BrowserCloseWindow();
}

function BrowserTryToCloseWindow()
{
  //give tryToClose a chance to veto if it is defined
  if (typeof(window.tryToClose) != "function" || window.tryToClose())
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
    getBrowser().loadURI(uri, referrer);
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

    var browser = getBrowser();
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

      if (openTab) {
        // Open link in new tab
        var t = browser.addTab(url);

        // Focus new tab unless shift is pressed
        if (!shiftPressed) {
          browser.userTypedValue = null;
          browser.selectedTab = t;
        }
      } else {
        // Open a new window with the URL
        var newWin = openDialog(getBrowserURL(), "_blank", "all,dialog=no", url);
        // Reset url in the urlbar, copied from handleURLBarRevert()
        var oldURL = browser.currentURI.spec;
        if (oldURL != "about:blank") {
          gURLBar.value = oldURL;
          SetPageProxyState("valid", null);
        } else
          gURLBar.value = "";

        browser.userTypedValue = null;

        // Focus old window if shift was pressed, as there's no
        // way to open a new window in the background
        // XXX this doesn't seem to work
        if (shiftPressed) {
          //newWin.blur();
          content.focus();
        }
      }
    } else if (saveModifier) {
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
    } else {
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
    // If available, use selection clipboard, otherwise global one
    if (clipboard.supportsSelectionClipboard())
      clipboard.getData(trans, clipboard.kSelectionClipboard);
    else
      clipboard.getData(trans, clipboard.kGlobalClipboard);

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
      // cannot be found, then just use the content window...
      //
      // XXX:  This is a bit of a hack...
      win = aDocument.defaultView;
      if (win == window) {
        win = content;
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
             "all,dialog=no",
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
  var disabledItems = ['cmd_close', 'Browser:SendPage', 'Browser:EditPage',
                       'cmd_printSetup', /*'Browser:PrintPreview',*/
                       'Browser:Print', 'canGoBack', 'canGoForward',
                       'Browser:Home', 'Browser:AddBookmark',
                       'Browser:AddBookmarkAs', 'Browser:AddGroupmarkAs',
                       'cmd_undo', 'cmd_redo', 'cmd_cut', 'cmd_copy',
                       'cmd_paste', 'cmd_delete', 'cmd_selectAll',
                       'menu_textZoom'];
  for (var id in disabledItems) {
    var broadcaster = document.getElementById(disabledItems[id]);
    if (broadcaster)
      broadcaster.setAttribute("disabled", "true");
  }

  // now load bookmarks after a delay
  setTimeout(hiddenWindowLoadBookmarksCallback, 0);
}

function hiddenWindowLoadBookmarksCallback()
{
  // loads the services
  initServices();
  initBMService();
  BMSVC.readBookmarks();  
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
  leakDetector.traceObject(content, leakDetector.verbose);
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
  if ( "HTTPIndex" in content &&
       content.HTTPIndex instanceof Components.interfaces.nsIHTTPIndex ) {
    content.defaultCharacterset = getMarkupDocumentViewer().defaultCharacterSet;
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
  var styleSheets = getAllStyleSheets(window.content);
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
  if (!gIgnoreClick && gClickSelectsAll && gURLBar.selectionStart == gURLBar.selectionEnd)
    if (gClickAtEndSelects || gURLBar.selectionStart < gURLBar.value.length)
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

    gBrowser.userTypedValue = null;
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

function handlePageProxyClick(aEvent)
{
  switch (aEvent.button) {
  case 0:
    // bug 52784 - select location field contents
    gURLBar.select();
    break;
  case 1:
    // bug 111337 - load url/keyword from clipboard
    return middleMousePaste(aEvent);
    break;
  }
  return true;
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

function BrowserFullScreen()
{
  window.fullScreen = !window.fullScreen;
}

function onFullScreen()
{
  FullScreen.toggle();
}

function onPopupWindow(aEvent) {
  var firstPopup = pref.getBoolPref("privacy.popups.first_popup");
  var blockingEnabled = pref.getBoolPref("dom.disable_open_during_load");
  if (blockingEnabled) {
    pref.setBoolPref("privacy.popups.first_popup", false);
    return;
  }
  if (firstPopup) { 
    var showDialog = true;
    var specialList = "";
    try {
      specialList = pref.getComplexValue("privacy.popups.default_whitelist",
                                         Components.interfaces.nsIPrefLocalizedString).data;
    }
    catch(ex) { }
    if (specialList) {
      hosts = specialList.split(",");
      var browser = getBrowserForDocument(aEvent.target);
      if (!browser)
        return;
      var currentHost = browser.currentURI.hostPort;
      for (var i = 0; i < hosts.length; i++) {
        var nextHost = hosts[i];
        if (nextHost == currentHost ||
            "."+nextHost == currentHost.substr(currentHost.length - (nextHost.length+1))) {
          showDialog = false;
          break;
        }       
      }
    }
    if (showDialog) {
      window.openDialog("chrome://communicator/content/aboutPopups.xul", "",
                        "chrome,centerscreen,dependent", true);
      pref.setBoolPref("privacy.popups.first_popup", false);
    }
  }
}

function onPopupBlocked(aEvent) {
  var playSound = pref.getBoolPref("privacy.popups.sound_enabled");

  if (playSound) {
    var sound = Components.classes["@mozilla.org/sound;1"]
                          .createInstance(Components.interfaces.nsISound);

    var soundUrlSpec = pref.getCharPref("privacy.popups.sound_url");

    if (!soundUrlSpec)
      sound.beep();

    if (soundUrlSpec.substr(0, 7) == "file://") {
      var soundUrl = Components.classes["@mozilla.org/network/standard-url;1"]
                               .createInstance(Components.interfaces.nsIFileURL);
      soundUrl.spec = soundUrlSpec;
      var file = soundUrl.file;
      if (file.exists)
        sound.play(soundUrl);
    } 
    else {
      sound.playSystemSound(soundUrlSpec);
    }
  }

  var showIcon = pref.getBoolPref("privacy.popups.statusbar_icon_enabled");
  if (showIcon) {
    var browser = getBrowserForDocument(aEvent.target);      
    if (browser) {
      var hostPort = browser.currentURI.hostPort;
      browser.popupDomain = hostPort;
      if (browser == getBrowser().selectedBrowser) {
        var popupIcon = document.getElementById("popupIcon");
        popupIcon.hidden = false;
      }
      if (!browser.popupUrls) {
        browser.popupUrls = [];
        browser.popupFeatures = [];
      }
      // Check for duplicates, remove the old occurence of this url,
      // to update the features, and put it at the end of the list.
      for (var i = 0; i < browser.popupUrls.length; ++i) {
        if (browser.popupUrls[i].equals(aEvent.popupWindowURI)) {
          browser.popupUrls.splice(i, 1);
          browser.popupFeatures.splice(i, 1);
          break;
        }
      }
      // Limit the length of the menu to some reasonable size.
      // We only add one item every time, so no need for more complex stuff.
      if (browser.popupUrls.length >= 100) {
        browser.popupUrls.shift();
        browser.popupFeatures.shift();
      }
      browser.popupUrls.push(aEvent.popupWindowURI);
      browser.popupFeatures.push(aEvent.popupWindowFeatures);
    }
  }
}

function getBrowserForDocument(doc) {  
  var browsers = getBrowser().browsers;
  for (var i = 0; i < browsers.length; i++) {
    if (browsers[i].contentDocument == doc)
      return browsers[i];
  }
  return null;
}

function StatusbarViewPopupManager() {
  var hostPort = "";
  try {
    hostPort = getBrowser().selectedBrowser.currentURI.hostPort;
  }
  catch(ex) { }
  
  // open whitelist with site prefilled to unblock
  window.openDialog("chrome://communicator/content/popupManager.xul", "",
                      "chrome,resizable=yes", hostPort);
}

function popupBlockerMenuShowing(event) {
  var parent = event.target;
  var browser = getBrowser().selectedBrowser;      
  var separator = document.getElementById("popupMenuSeparator");

  if ("popupDomain" in browser) {
    createShowPopupsMenu(parent);
    if (separator)
      separator.hidden = false;
  } else {
    if (separator)
      separator.hidden = true;
  }  
}

function createShowPopupsMenu(parent) {
  while (parent.lastChild && parent.lastChild.hasAttribute("uri"))
    parent.removeChild(parent.lastChild);

  var browser = getBrowser().selectedBrowser;      

  for (var i = 0; i < browser.popupUrls.length; i++) {
    var menuitem = document.createElement("menuitem");
    menuitem.setAttribute("label", gNavigatorBundle.getFormattedString('popupMenuShow', [browser.popupUrls[i].spec]));
    menuitem.setAttribute("uri", browser.popupUrls[i].spec);
    menuitem.setAttribute("features", browser.popupFeatures[i]);
    parent.appendChild(menuitem);
  }

  return true;
}

function popupBlockerMenuCommand(target) {
  var uri = target.getAttribute("uri");
  if (uri) {
    window.content.open(uri, "", target.getAttribute("features"));
  }
}

function toHistory()
{
  toOpenWindowByType("history:manager", "chrome://communicator/content/history/history.xul");
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
      var opener = new XPCNativeWrapper(window.content, "opener").opener;
      var location = new XPCNativeWrapper(opener, "location").location;
      return IOS.newURI(location.href, null, null);
    }
  } catch(e) {
  }
  return null;
}

function WindowIsClosing()
{
  var browser = getBrowser();
  var cn = browser.tabContainer.childNodes;
  var numtabs = cn.length;
  var reallyClose = true;

  if (numtabs > 1) {
    var shouldPrompt = pref.getBoolPref("browser.tabs.warnOnClose");
    if (shouldPrompt) {
      var promptService =
        Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                  .getService(Components.interfaces.nsIPromptService);
      //default to true: if it were false, we wouldn't get this far
      var warnOnClose = {value:true};

       var buttonPressed = promptService.confirmEx(window, 
         gNavigatorBundle.getString('tabs.closeWarningTitle'), 
         gNavigatorBundle.getFormattedString("tabs.closeWarning", [numtabs]),
         (promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0)
          + (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1),
            gNavigatorBundle.getString('tabs.closeButton'),
            null, null,
            gNavigatorBundle.getString('tabs.closeWarningPromptMe'),
            warnOnClose);
      reallyClose = (buttonPressed == 0);
      //don't set the pref unless they press OK and it's false
      if (reallyClose && !warnOnClose.value) {
        pref.setBoolPref("browser.tabs.warnOnClose", false);
      }
    } //if the warn-me pref was true
  } //if multiple tabs are open

  for (var i = 0; reallyClose && i < numtabs; ++i) {
    var ds = browser.getBrowserForTab(cn[i]).docShell;
  
    if (ds.contentViewer && !ds.contentViewer.permitUnload())
      reallyClose = false;
  }

  return reallyClose;
}

/**
 * file upload support
 */

/* This function returns the URI of the currently focused content frame
 * or frameset.
 */
function getCurrentURI()
{
  const CI = Components.interfaces;

  var focusedWindow = document.commandDispatcher.focusedWindow;
  var contentFrame = isContentFrame(focusedWindow) ? focusedWindow : window.content;

  var nav = contentFrame.QueryInterface(CI.nsIInterfaceRequestor)
                        .getInterface(CI.nsIWebNavigation);
  return nav.currentURI;
}

function uploadFile(fileURL)
{
  const CI = Components.interfaces;

  var targetBaseURI = getCurrentURI();

  // generate the target URI.  we use fileURL.file.leafName to get the
  // unicode value of the target filename w/o any URI-escaped chars.
  // this gives the protocol handler the best chance of generating a
  // properly formatted URI spec.  we pass null for the origin charset
  // parameter since we want the URI to inherit the origin charset
  // property from targetBaseURI.

  var leafName = fileURL.file.leafName;

  const IOS = Components.classes["@mozilla.org/network/io-service;1"]
                        .getService(CI.nsIIOService);
  var targetURI = IOS.newURI(leafName, null, targetBaseURI);

  // ok, start uploading...

  var dialog = Components.classes["@mozilla.org/progressdialog;1"]
                         .createInstance(CI.nsIProgressDialog);

  var persist = Components.classes["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                          .createInstance(CI.nsIWebBrowserPersist);

  dialog.init(fileURL, targetURI, leafName, null, Date.now()*1000, persist);
  dialog.open(window);

  persist.progressListener = dialog;
  persist.saveURI(fileURL, null, null, null, null, targetURI);
}

function BrowserUploadFile()
{
  try {
    uploadFile(selectFileToOpen("uploadFile", "browser.upload."));
  } catch (e) {}
}

/* This function is called whenever the file menu is about to be displayed.
 * Enable the upload menu item if appropriate. */
function updateFileUploadItem()
{
  var canUpload = false;
  try {
    canUpload = getCurrentURI().schemeIs('ftp');
  } catch (e) {}

  var item = document.getElementById('Browser:UploadFile');
  if (canUpload)
    item.removeAttribute('disabled');
  else
    item.setAttribute('disabled', 'true');
}
