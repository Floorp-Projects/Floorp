/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Blake Ross <blakeross@telocity.com>
 *   Peter Annema <disttsc@bart.nl>
 */

const nsIWebNavigation = Components.interfaces.nsIWebNavigation;

var gURLBar = null;
var bundle = srGetStrBundle("chrome://navigator/locale/navigator.properties");
var brandBundle = srGetStrBundle("chrome://global/locale/brand.properties");

var pref = Components.classes["@mozilla.org/preferences;1"]
                     .getService(Components.interfaces.nsIPref);

var appCore = null;

//cached elements/fields
var statusTextFld = null;
var statusMeter = null;
var throbberElement = null;
var stopButton = null;
var stopMenu = null;
var stopContext = null;

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

function nsXULBrowserWindow()
{
  this.defaultStatus = bundle.GetStringFromName("defaultStatus");
}

nsXULBrowserWindow.prototype =
{
  useRealProgressFlag : false,
  totalRequests : 0,
  finishedRequests : 0,

  // Stored Status, Link and Loading values
  defaultStatus : "",
  jsStatus : "",
  jsDefaultStatus : "",
  overLink : "",
  startTime : 0,

  hideAboutBlank : true,

  QueryInterface : function(iid)
  {
    if (iid.equals(Components.interfaces.nsIXULBrowserWindow))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

  setJSStatus : function(status)
  {
    this.jsStatus = status;
    this.updateStatusField();
    // Status is now on status bar; don't use it next time.
    // This will cause us to revert to defaultStatus/jsDefaultStatus when the
    // user leaves the link (e.g., if the script set window.status in onmouseover).
    this.jsStatus = "";
  },

  setJSDefaultStatus : function(status)
  {
    this.jsDefaultStatus = status;
    this.updateStatusField();
  },

  setDefaultStatus : function(status)
  {
    this.defaultStatus = status;
    this.updateStatusField();
  },

  setOverLink : function(link)
  {
    this.overLink = link;
    this.updateStatusField();
  },

  updateStatusField : function()
  {
    var text = this.jsStatus || this.overLink || this.jsDefaultStatus || this.defaultStatus;

    if (!statusTextFld)
      statusTextFld = document.getElementById("statusbar-display");

    statusTextFld.setAttribute("value", text);
  },

  onProgress : function (channel, current, max)
  {
    if (!this.useRealProgressFlag && channel)
      return;

    if (!statusMeter)
      statusMeter = document.getElementById("statusbar-icon");

    if (max > 0) {
      if (statusMeter.getAttribute("mode") != "normal")
        statusMeter.setAttribute("mode", "normal");

      var percentage = (current * 100) / max ;
      statusMeter.value = percentage;
      statusMeter.progresstext = Math.round(percentage) + "%";
    } else {
      if (statusMeter.getAttribute("mode") != "undetermined")
        statusMeter.setAttribute("mode","undetermined");
    }
  },

  onStateChange : function(channel, state)
  {
    const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;

    if (!throbberElement)
      throbberElement = document.getElementById("navigator-throbber");
    if (!statusMeter)
      statusMeter = document.getElementById("statusbar-icon");
    if (!stopButton)
      stopButton = document.getElementById("stop-button");
    if (!stopMenu)
      stopMenu = document.getElementById("menuitem-stop");
    if (!stopContext)
      stopContext = document.getElementById("context-stop");

    if (state & nsIWebProgressListener.STATE_START) {
      if (state & nsIWebProgressListener.STATE_IS_NETWORK) {
        // Remember when loading commenced.
        this.startTime = (new Date()).getTime();

        // Turn progress meter on.
        statusMeter.setAttribute("mode","undetermined");
        throbberElement.setAttribute("busy", true);

        // XXX: These need to be based on window activity...
        stopButton.setAttribute("disabled", false);
        stopMenu.setAttribute("disabled", false);
        stopContext.setAttribute("disabled", false);

        // Initialize the progress stuff...
        statusMeter.setAttribute("mode","undetermined");
        this.useRealProgressFlag = false;
        this.totalRequests = 0;
        this.finishedRequests = 0;
      }

      if (state & nsIWebProgressListener.STATE_IS_REQUEST) {
        this.totalRequests += 1;
      }
      EnableBusyCursor(throbberElement.getAttribute("busy") == "true");
    }
    else if (state & nsIWebProgressListener.STATE_STOP) {
      if (state & nsIWebProgressListener.STATE_IS_REQUEST) {
        this.finishedRequests += 1;
        if (!this.useRealProgressFlag)
          this.onProgress(null, this.finishedRequests, this.totalRequests);
      }

      if (state & nsIWebProgressListener.STATE_IS_NETWORK) {
        var location = channel.URI.spec;
        var msg = "";
        if (location != "about:blank") {
          // Record page loading time.
          var elapsed = ((new Date()).getTime() - this.startTime) / 1000;
          msg = bundle.GetStringFromName("nv_done");
          msg = msg.replace(/%elapsed%/, elapsed);
        }
        this.setDefaultStatus(msg);

        // Turn progress meter off.
        statusMeter.setAttribute("mode","normal");
        statusMeter.value = 0;  // be sure to clear the progress bar
        statusMeter.progresstext = "";
        throbberElement.removeAttribute("busy");

        // XXX: These need to be based on window activity...
        stopButton.setAttribute("disabled", true);
        stopMenu.setAttribute("disabled", true);
        stopContext.setAttribute("disabled", true);

        // Set buttons in form menu
        setFormToolbar();

        EnableBusyCursor(false);
      }
    }
    else if (state & nsIWebProgressListener.STATE_TRANSFERRING) {
      if (state & nsIWebProgressListener.STATE_IS_DOCUMENT) {
        var ctype=channel.contentType;

        if (ctype != "text/html")
          this.useRealProgressFlag = true;

        statusMeter.setAttribute("mode", "normal");
      }

      if (state & nsIWebProgressListener.STATE_IS_REQUEST) {
        if (!this.useRealProgressFlag)
          this.onProgress(null, this.finishedRequests, this.totalRequests);
      }
    }
  },

  onLocationChange : function(location)
  {
    if (this.hideAboutBlank) {
      this.hideAboutBlank = false;
      if (location == "about:blank")
        location = "";
    }

    // We should probably not do this if the value has changed since the user
    // searched
    document.getElementById("urlbar").value = location;

    UpdateBackForwardButtons();
  },

  onStatus : function(channel, status, msg)
  {
    this.setOverLink(msg);
    //this.setDefaultStatus(msg);
  }
}

function getBrowser()
{
  return document.getElementById("content");
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
    url = bundle.GetStringFromName("homePageDefault");

  return url;
}

function UpdateBackForwardButtons()
{
  var backBroadcaster = document.getElementById("canGoBack");
  var forwardBroadcaster = document.getElementById("canGoForward");
  var webNavigation = getWebNavigation();

  backBroadcaster.setAttribute("disabled", !webNavigation.canGoBack);
  forwardBroadcaster.setAttribute("disabled", !webNavigation.canGoForward);
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
  Observe: function(subject, topic, prefName)
  {
    // verify that we're changing a button pref
    if (topic != "nsPref:changed") return;
    if (prefName.substr(0, this.domain.length) != this.domain) return;
    
    var buttonName = prefName.substr(this.domain.length+1);
    var buttonId = buttonName + "-button";
    var button = document.getElementById(buttonId);
    
    var show = pref.GetBoolPref(prefName);
    if (show)
      button.removeAttribute("hidden");
    else
      button.setAttribute("hidden", "true");
  }
}

function Startup()
{
  window.XULBrowserWindow = new nsXULBrowserWindow();
  window.buttonPrefListener = new nsButtonPrefListener();


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

  _content.appCore = appCore;

  // Initialize browser instance..
  appCore.setWebShellWindow(window);

  // give urlbar focus so it'll be an active editor and d&d will work properly
  gURLBar = document.getElementById("urlbar");
  gURLBar.focus();

  // set the offline/online mode
  setOfflineStatus();

  // Add a capturing event listener to the content area
  // (rjc note: not the entire window, otherwise we'll get sidebar pane loads too!)
  //  so we'll be notified when onloads complete.
  var contentArea = document.getElementById("appcontent");
  contentArea.addEventListener("load", loadEventHandlers, true);
  contentArea.addEventListener("focus", contentAreaFrameFocus, true);

  // set default character set if provided
  if ("arguments" in window && window.arguments.length > 1) {
    if (window.arguments[1].indexOf("charset=") != -1) {
      var arrayArgComponents = window.arguments[1].split("=");
      if (arrayArgComponents) {
        //we should "inherit" the charset menu setting in a new window
        appCore.SetDocumentCharset(arrayArgComponents[1]);
      }
    }
  }

  try {
    var searchMode = pref.GetIntPref("browser.search.mode");
    setBrowserSearchMode(searchMode);
  } catch (ex) {
  }

  // set home button tooltip text
  var homePage = getHomePage();
  if (homePage)
    document.getElementById("home-button").setAttribute("tooltiptext", homePage);

  //initConsoleListener();

  // load appropriate initial page from commandline
  var isPageCycling = false;

  // page cycling for tinderbox tests
  if (!appCore.cmdLineURLUsed)
    isPageCycling = appCore.startPageCycler();

  // only load url passed in when we're not page cycling
  if (!isPageCycling) {
    var startPage;

    if (!appCore.cmdLineURLUsed) {
      var cmdLineService = Components.classes["@mozilla.org/appshell/commandLineService;1"]
                                     .getService(Components.interfaces.nsICmdLineService);
      startPage = cmdLineService.URLToLoad;
      appCore.cmdLineURLUsed = true;
    }

    if (!startPage) {
      // Check for window.arguments[0]. If present, use that for startPage.
      if ("arguments" in window && window.arguments.length > 0) {
        startPage = window.arguments[0];
      } else {
        var cmdLineHandler = Components.classes["@mozilla.org/commandlinehandler/general-startup;1?type=browser"]
                                       .getService(Components.interfaces.nsICmdLineHandler);
        startPage = cmdLineHandler.defaultArgs;
      }
    }

    if (startPage)
      loadURI(startPage);

    // Perform default browser checking.
    checkForDefaultBrowser();
  }
}

function Shutdown()
{
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
  if (index) {
    getWebNavigation().gotoIndex(index);
    return true;
  }
  return false;
}

function BrowserBack()
{
  getWebNavigation().goBack();
  UpdateBackForwardButtons();
}

function BrowserForward()
{
  getWebNavigation().goForward();
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
  getWebNavigation().stop();
}

function BrowserReallyReload(event)
{
  // Default is no flags.
  var reloadFlags = nsIWebNavigation.LOAD_FLAGS_NONE;
  // See if the event was a shift-click.
  if (event && event.shiftKey) {
    // Shift key means bypass proxy and cache.
    reloadFlags = nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY | nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
  }
  getWebNavigation().reload(reloadFlags);
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

function OpenSearch(tabName, forceDialogFlag, searchStr)
{
  var autoOpenSearchPanel = false;
  var defaultSearchURL = null;
  var fallbackDefaultSearchURL = bundle.GetStringFromName("fallbackDefaultSearchURL");
  // XXX This somehow causes a big leak, back to the old way
  //     till we figure out why. See bug 61886.
  // var url = getWebNavigation().currentURI.spec;
  var url = _content.location.href;

  try {
    autoOpenSearchPanel = pref.GetBoolPref("browser.search.opensidebarsearchpanel");
    defaultSearchURL = pref.getLocalizedUnicharPref("browser.search.defaulturl");
  } catch (ex) {
  }

  // Fallback to a default url (one that we can get sidebar search results for)
  if (!defaultSearchURL)
    defaultSearchURL = fallbackDefaultSearchURL;

  if (!searchStr || searchStr == url) {
    if (defaultSearchURL != fallbackDefaultSearchURL)
      loadURI(defaultSearchURL);
    else
      loadURI(bundle.GetStringFromName("otherSearchURL"));

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

  // should we try and open up the sidebar to show the "Search Results" panel?
  if (autoOpenSearchPanel)
    RevealSearchPanel();
}

function setBrowserSearchMode(searchMode)
{
  // set search mode preference
  try {
    pref.SetIntPref("browser.search.mode", searchMode);
  } catch (ex) {
  }

  // update search menu
  var simpleMenuItem = document.getElementById("simpleSearch");
  simpleMenuItem.setAttribute("checked", (searchMode == 0) ? "true" : "false");

  var advancedMenuItem = document.getElementById("advancedSearch");
  advancedMenuItem.setAttribute("checked", (searchMode == 1) ? "true" : "false");
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
  openDialog("chrome://navigator/content/openLocation.xul", "_blank", "chrome,modal,titlebar", window);
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
    fp.init(window, bundle.GetStringFromName("openFile"), nsIFilePicker.modeOpen);
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
  // this is currently a do-nothing on appCore which is going to die
  // ???.printPreview();
}

function BrowserPrint()
{
  // using _content.print() until printing becomes scriptable on docShell
  _content.print();
}

function BrowserSetDefaultCharacterSet(aCharset)
{
  appCore.SetDocumentCharset(aCharset);
  getWebNavigation().reload(nsIWebNavigation.LOAD_FLAGS_NONE);
}

function BrowserSetForcedCharacterSet(aCharset)
{
  appCore.SetForcedCharset(aCharset);
}

function BrowserSetForcedDetector()
{
  appCore.SetForcedDetector();
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

  function BrowserFind() {
    appCore.find();
  }

  function BrowserFindAgain() {
    appCore.findNext();
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
  loadShortcutOrURI(url);
}

function loadShortcutOrURI(url)
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
    // stifle any exceptions so we're sure to load the URL.
  }

  loadURI(url);
  _content.focus();
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

  var docCharset = "charset=" + focusedWindow.document.characterSet;

  //now try to open a view-source window while inheriting the charset (if any)
  openDialog("chrome://navigator/content/viewSource.xul",
             "_blank",
             "scrollbars,resizable,chrome,dialog=no",
             _content.location, docCharset);
}

function BrowserPageInfo()
{
  window.openDialog("chrome://navigator/content/pageInfo.xul",
                    "_blank",
                    "chrome,dialog=no");
}

function BrowserReload()
{
}

function hiddenWindowStartup()
{
  // Disable menus which are not appropriate
  var disabledItems = ['cmd_close', 'Browser:SendPage', 'Browser:EditPage', 'Browser:PrintSetup', 'Browser:PrintPreview',
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
      statusbarDisplay.value = bundle.GetStringFromName("jserror");
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
  if ("HTTPIndex" in _content &&
      typeof _content.HTTPIndex == "object" &&
      !_content.HTTPIndex.constructor) {
    // Give directory .xul/.js access to browser instance.
    // XXX The following line leaks (bug 61821), so the next line is a hack
    // to avoid the leak.
    // _content.defaultCharacterset = getBrowser().markupDocumentViewer.defaultCharacterSet;
    _content.defaultCharacterset = getBrowser().docShell.contentViewer.QueryInterface(Components.interfaces.nsIMarkupDocumentViewer).defaultCharacterSet;
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

function EnableBusyCursor(doEnable)
{
  if (doEnable) {
    // set the spinning cursor everywhere but mac, we have our own way to
    // do this thankyouverymuch.
    if (navigator.platform.indexOf("Mac") == -1) {
      setCursor("spinning");
      _content.setCursor("spinning");
    }
  } else {
    setCursor("auto");
    _content.setCursor("auto");
  }
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

      var lastWithSameTitle;
      if (currentStyleSheet.title in currentStyleSheets)
        lastWithSameTitle = currentStyleSheets[currentStyleSheet.title];

      if (!lastWithSameTitle) {
        var menuItem = document.createElement("menuitem");
        menuItem.setAttribute("value", currentStyleSheet.title);
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
  loadURI(brandBundle.GetStringFromName("getNewThemesURL"));
}

function URLBarLeftClickHandler(aEvent)
{
  if (pref.GetBoolPref("browser.urlbar.clickSelectsAll")) {
    var URLBar = aEvent.target;
    URLBar.setSelectionRange(0, URLBar.value.length);
  }
}

function URLBarBlurHandler(aEvent)
{
  if (pref.GetBoolPref("browser.urlbar.clickSelectsAll")) {
    var URLBar = aEvent.target;
    URLBar.setSelectionRange(0, 0);
  }
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

function formCapture()
{
  var walletService = Components.classes["@mozilla.org/wallet/wallet-service;1"].getService(Components.interfaces.nsIWalletService);
  walletService.WALLET_RequestToCapture(window._content);
}

function formPrefill()
{
  var walletService = Components.classes["@mozilla.org/wallet/wallet-service;1"].getService(Components.interfaces.nsIWalletService);
  walletService.WALLET_Prefill(false, window._content);

  window.openDialog("chrome://communicator/content/wallet/WalletPreview.xul",
                    "_blank", "chrome,modal=yes,dialog=yes,all, width=504, height=436");
}

function formShow()
{
  window.openDialog(
      "chrome://communicator/content/wallet/WalletViewer.xul",
      "WalletViewer",
      "chrome,titlebar,modal=yes,resizable=yes");
  // if a stored value changed, we might need to enable/disable the prefill-form button
  setFormToolbar(); // in case we need to change state of prefill-form button 
}

function setFormToolbar()
{

  var formToolbar = document.getElementById("FormToolbar");
  if (!formToolbar) {
    return;
  }

  // keep form toolbar hidden if checkbox in view menu so indicates

  var cmd_viewformToolbar = document.getElementById("cmd_viewformtoolbar");
  if (cmd_viewformToolbar) {
    var checkValue = cmd_viewformToolbar.getAttribute("checked");
    if (checkValue == "false") {
      formToolbar.setAttribute("hidden", "true");
      return;
    }
  }

  // hide form toolbar if there is no form on the current page

  if (!window._content.document) {
    formToolbar.setAttribute("hidden", "true");
    return;
  }
  var formsArray = window._content.document.forms;
  if (!formsArray || formsArray.length == 0) {
    formToolbar.setAttribute("hidden", "true");
    return;
  }
  formToolbar.removeAttribute("hidden");
  return; // skip disabling the prefill button for now -- it was causing leak and bloat

  // enable prefill button if there is at least one saved value for the form

  var formPrefill = document.getElementById("formPrefill");
  var walletService = Components.classes["@mozilla.org/wallet/wallet-service;1"].getService(Components.interfaces.nsIWalletService);
  var form;
  for (form=0; form<formsArray.length; form++) {
    var elementsArray = formsArray[form].elements;
    var element;
    for (element=0; element<elementsArray.length; element++) {
      var type = elementsArray[element].type;
      if (type=="" || type=="text" || type=="select-one") {
        var value = walletService.WALLET_PrefillOneElement
          (window._content, elementsArray[element]);
        if (value != "") {
          // element has a saved value, thus prefill button is to appear in toolbar
          formPrefill.setAttribute("disabled", "false");
          return;
        }
      }
    }
  }
  formPrefill.setAttribute("disabled", "true");
}

// Can't use generic goToggleToolbar (see utilityOverlay.js) for form menu because
// form toolbar could be hidden even when the checkbox in the view menu is checked
function goToggleFormToolbar( id, elementID )
{
  var toolbar = document.getElementById(id);
  var element = document.getElementById(elementID);
  if (element) {
    var checkValue = element.getAttribute("checked");
    if (checkValue == "false") {
      element.setAttribute("checked","true")
      if (toolbar) {
        setFormToolbar();
      }
    } else {
      element.setAttribute("checked","false")
      if (toolbar) {
        toolbar.setAttribute("hidden", true );
      }
    }
    document.persist(id, 'hidden');
    document.persist(elementID, 'checked');
  }
}


