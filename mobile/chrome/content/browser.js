// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/*
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Lassey <blassey@mozilla.com>
 *   Mark Finkle <mfinkle@mozilla.com>
 *   Aleks Totic <a@totic.org>
 *   Johnathan Nightingale <johnath@mozilla.com>
 *   Stuart Parmenter <stuart@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const FINDSTATE_FIND = 0;
const FINDSTATE_FIND_AGAIN = 1;
const FINDSTATE_FIND_PREVIOUS = 2;

Cu.import("resource://gre/modules/SpatialNavigation.js");

// create a lazy-initialized handle for the pref service on the global object
// in the style of bug 385809
__defineGetter__("gPrefService", function () {
  delete gPrefService;
  var gPrefService;
  return gPrefService = Components.classes["@mozilla.org/preferences-service;1"]
                                  .getService(Components.interfaces.nsIPrefBranch2);
});

function getBrowser() {
  return Browser.currentBrowser;
}

var ws = null;
var ih = null;

var Browser = {
  _content : null,
  _tabs : [],
  _currentTab : null,

  startup: function() {
    var self = this;

    // initalize the CanvasBrowser
    this._content = new CanvasBrowser(document.getElementById("canvas"));

    // initialize the WidgetStack
    ws = new WidgetStack(document.getElementById("browser-container"));
    ws.setViewportBounds({ top: 0, left: 0, right: 800, bottom: 480 });

    // XXX this should live elsewhere
    window.gSidebarVisible = false;
    function panHandler(vr) {
      var visibleNow = ws.isWidgetVisible("browser-controls") || ws.isWidgetVisible("tabs-container");

      // XXX add code here to snap side panels fully out if they start to appear,
      // or snap them back if they only showed up for a little bit

      if (visibleNow && !gSidebarVisible) {
        ws.freeze("toolbar-main");
        ws.moveFrozenTo("toolbar-main", 0, 0);
      }
      else if (!visibleNow && gSidebarVisible) {
        ws.unfreeze("toolbar-main");
      }
      gSidebarVisible = visibleNow;

      // deal with checkerboard
      /*
      let stack = document.getElementById("browser-container");
      stack.style.backgroundPosition =  -vr.left + "px " + -vr.top + "px";
      */

      // this is really only necessary for maemo, where we don't
      // always repaint fast enough
      window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).processUpdates();
    }

    ws.setPanHandler(panHandler);

    function resizeHandler() { ws.updateSize(); }
    window.addEventListener("resize", resizeHandler, false);

    setTimeout(resizeHandler, 0);

    function viewportHandler(b, ob) { self._content.viewportHandler(b, ob); }
    ws.setViewportHandler(viewportHandler);

    // initialize input handling
    ih = new InputHandler();

    // Create the first tab
    this.newTab(true);

    window.controllers.appendController(this);
    window.controllers.appendController(BrowserUI);

    var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
    var styleSheets = Cc["@mozilla.org/content/style-sheet-service;1"].getService(Ci.nsIStyleSheetService);

    // Should we hide the cursors
    var hideCursor = gPrefService.getBoolPref("browser.ui.cursor") == false;
    if (hideCursor) {
      window.QueryInterface(Ci.nsIDOMChromeWindow).setCursor("none");

      var styleURI = ios.newURI("chrome://browser/content/content.css", null, null);
      styleSheets.loadAndRegisterSheet(styleURI, styleSheets.AGENT_SHEET);
    }

    // load styles for scrollbars
    var styleURI = ios.newURI("chrome://browser/content/scrollbars.css", null, null);
    styleSheets.loadAndRegisterSheet(styleURI, styleSheets.AGENT_SHEET);

    var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
    os.addObserver(gXPInstallObserver, "xpinstall-install-blocked", false);
    os.addObserver(gXPInstallObserver, "xpinstall-download-started", false);

    // XXX hook up memory-pressure notification to clear out tab browsers
    //os.addObserver(function(subject, topic, data) self.destroyEarliestBrowser(), "memory-pressure", false);

    BrowserUI.init();

    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = new nsBrowserAccess();

    let browsers = document.getElementById("browsers");
    browsers.addEventListener("command", this._handleContentCommand, false);
    browsers.addEventListener("DOMUpdatePageReport", gPopupBlockerObserver.onUpdatePageReport, false);

    /* Initialize Spatial Navigation */
    /*
    var deckbrowser = content;
    function panCallback(aElement) {
      // SpatialNav calls commandDispatcher.advanceFocus/rewindFocus, which
      // can mess the scroll state up. Reset it.
      deckbrowser.browser.contentWindow.scrollTo(0, 0);

      if (!aElement)
        return;

      deckbrowser.ensureElementIsVisible(aElement);
    }
    SpatialNavigation.init(content, panCallback);
    */

    /* Initialize Geolocation */
    this.setupGeolocationPrompt();


    /* Login Manager */
    Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);


    /* Command line arguments/initial homepage */
    // If this is an intial window launch (was a nsICommandLine passed via window params)
    // we execute some logic to load the initial launch page
    if (window.arguments && window.arguments[0]) {
      var whereURI = null;

      try {
        // Try to access the commandline
        var cmdLine = window.arguments[0].QueryInterface(Ci.nsICommandLine);

        try {
          // Check for and use a default homepage
          whereURI = gPrefService.getCharPref("browser.startup.homepage");
        } catch (e) {}

        // Check for and use a single commandline parameter
        if (cmdLine.length == 1) {
          // Assume the first arg is a URI if it is not a flag
          var uri = cmdLine.getArgument(0);
          if (uri != "" && uri[0] != '-') {
            whereURI = cmdLine.resolveURI(uri);
            if (whereURI)
              whereURI = whereURI.spec;
          }
        }

        // Check for the "url" flag
        var uriFlag = cmdLine.handleFlagWithParam("url", false);
        if (uriFlag) {
          whereURI = cmdLine.resolveURI(uriFlag);
          if (whereURI)
            whereURI = whereURI.spec;
        }
      } catch (e) {}

      if (whereURI) {
        var self = this;
        setTimeout(function() { self.currentBrowser.loadURI(whereURI, null, null, false); }, 0);
      }
    }

    var disablePlugins = true;
    try { disablePlugins = gPrefService.getBoolPref("temporary.disablePlugins"); } catch (ex) { }
    if (disablePlugins)
    {
      document.getElementById("plugins.enabled").pref.value = false;
      this.setPluginState(false);
    }
    gPrefService.setBoolPref("temporary.disablePlugins", false);
  },

  updateViewportSize: function() {
    // XXX make sure this is right, and then add a better function for it.
    var [w,h] = this._content._contentAreaDimensions;
    w = Math.ceil(this._content._pageToScreen(w));
    h = Math.ceil(this._content._pageToScreen(h));

    if (!this._currentViewportBounds || w != this._currentViewportBounds.width || h != this._currentViewportBounds.height) {
      this._currentViewportBounds = {width: w, height: h};
      let bounds = { top: 0, left: 0, right: Math.max(800, w), bottom: Math.max(480, h) }
      //dump("setViewportBounds: " + bounds.toSource() + "\n");
      ws.setViewportBounds(bounds);
    }
  },

  setPluginState: function(enabled)
  {
    var phs = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    var plugins = phs.getPluginTags({ });
    for (var i = 0; i < plugins.length; ++i)
      plugins[i].disabled = !enabled;
  },

  setupGeolocationPrompt: function() {
    try {
      var geolocationService = Cc["@mozilla.org/geolocation/service;1"].getService(Ci.nsIGeolocationService);
    }
    catch (ex) {
      return;
    }

    geolocationService.prompt = function(request) {
      var notificationBox = Browser.getNotificationBox();
      var notification = notificationBox.getNotificationWithValue("geolocation");

      if (!notification) {
        var bundle_browser = document.getElementById("bundle_browser");
        var buttons = [{
            label: bundle_browser.getString("geolocation.exactLocation"),
            subLabel: bundle_browser.getString("geolocation.exactLocation.subLabel"),
            accessKey: bundle_browser.getString("geolocation.exactLocationKey"),
            callback: function(){request.allow()}
          },
          {
            label: bundle_browser.getString("geolocation.neighborhoodLocation"),
            subLabel: bundle_browser.getString("geolocation.neighborhoodLocation.subLabel"),
            accessKey: bundle_browser.getString("geolocation.neighborhoodLocationKey"),
            callback: function(){request.allowButFuzz()}
          },
          {
            label: bundle_browser.getString("geolocation.nothingLocation"),
            subLabel: "",
            accessKey: bundle_browser.getString("geolocation.nothingLocationKey"),
            callback: function(){request.cancel()}
          }];

        var message = bundle_browser.getFormattedString("geolocation.requestMessage", [request.requestingURI.spec]);
        var notification = notificationBox.appendNotification(message,
                             "geolocation", null, // todo "chrome://browser/skin/Info.png",
                             notificationBox.PRIORITY_INFO_HIGH, buttons);
        var children = notification.childNodes;
        for (var b = 0; b < children.length; b++)
          children[b].setAttribute("sublabel", children[b].buttonInfo.subLabel);
        return 1;
      }
    }
  },

  get content() {
    return this._content;
  },

  /**
   * Return the currently active <browser> object, since a <deckbrowser> may
   * have more than one
   */
  get currentBrowser() {
    return this._currentTab.browser;
  },

  get currentTab() {
    return this._currentTab;
  },

  getTabAtIndex: function(index) {
    if (index > this._tabs.length || index < 0)
      return null;
    return this._tabs[index];
  },

  getTabFromContent: function(content) {
    for (var t = 0; t < this._tabs.length; t++) {
      if (this._tabs[t].content == content)
        return this._tabs[t];
    }
    return null;
  },

  newTab: function(bringFront) {
    let newTab = new Tab();
    newTab.create();
    this._tabs.push(newTab);

    let event = document.createEvent("Events");
    event.initEvent("TabOpen", true, false);
    newTab.content.dispatchEvent(event);

    if (bringFront)
      this.selectTab(newTab);

    return newTab;
  },

  closeTab: function(tab) {
    if (tab instanceof XULElement)
      tab = this.getTabFromContent(tab);

    if (!tab)
      return;

    let tabIndex = this._tabs.indexOf(tab);

    let nextTab = this._currentTab;
    if (this._currentTab == tab) {
      nextTab = this.getTabAtIndex(tabIndex + 1) || this.getTabAtIndex(tabIndex - 1);
      if (!nextTab)
        return;
    }

    let event = document.createEvent("Events");
    event.initEvent("TabClose", true, false);
    tab.content.dispatchEvent(event);

    tab.destroy();
    this._tabs.splice(tabIndex, 1);

    // redraw the tabs
    for (let t = tabIndex; t < this._tabs.length; t++)
      this._tabs[t].updateThumbnail();

    this.selectTab(nextTab);
  },

  selectTab: function(tab) {
    if (tab instanceof XULElement)
      tab = this.getTabFromContent(tab);

    if (!tab || this._currentTab == tab)
      return;

    this._currentTab = tab;
    this._content.setCurrentBrowser(this.currentBrowser);
    document.getElementById("tabs").selectedItem = tab.content;

    ws.panTo(0,0, true);

    let event = document.createEvent("Events");
    event.initEvent("TabSelect", true, false);
    tab.content.dispatchEvent(event);
  },

  supportsCommand: function(cmd) {
    var isSupported = false;
    switch (cmd) {
      case "cmd_fullscreen":
        isSupported = true;
        break;
      default:
        isSupported = false;
        break;
    }
    return isSupported;
  },

  isCommandEnabled: function(cmd) {
    return true;
  },

  doCommand: function(cmd) {
    var browser = this.currentBrowser;

    switch (cmd) {
      case "cmd_fullscreen":
        window.fullScreen = !window.fullScreen;
        break;
    }
  },

  getNotificationBox: function() {
    return document.getElementById("notifications");
  },

  findState: FINDSTATE_FIND,
  openFind: function(aState) {
    this.findState = aState;

    var findbar = document.getElementById("findbar");
    var browser = findbar.browser;
    if (!browser) {
      browser = this.currentBrowser;
      findbar.browser = browser;
    }

    var panel = document.getElementById("findpanel");
    if (panel.state == "open")
      this.doFind(null);
    else
      panel.openPopup(document.getElementById("findpanel-placeholder"), "before_start");
  },

  doFind: function (aEvent) {
    var findbar = document.getElementById("findbar");
    if (Browser.findState == FINDSTATE_FIND)
      findbar.onFindCommand();
    else
      findbar.onFindAgainCommand(Browser.findState == FINDSTATE_FIND_PREVIOUS);
  },

  translatePhoneNumbers: function() {
    let doc = getBrowser().contentDocument;
    // jonas black magic (only match text nodes that contain a sequence of 4 numbers)
    let textnodes = doc.evaluate('//text()[contains(translate(., "0123456789", "^^^^^^^^^^"), "^^^^")]',
                                 doc,
                                 null,
                                 XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE,
                                 null);
    let s, node, lastLastIndex;
    let re = /(\+?1? ?-?\(?\d{3}\)?[ +-\.]\d{3}[ +-\.]\d{4})/
    for (var i = 0; i < textnodes.snapshotLength; i++) {
      node = textnodes.snapshotItem(i);
      s = node.data;
      if (s.match(re)) {
        s = s.replace(re, "<a href='tel:$1'> $1 </a>");
        try {
          let replacement = doc.createElement("span");
          replacement.innerHTML = s;
          node.parentNode.insertBefore(replacement, node);
          node.parentNode.removeChild(node);
        } catch(e) {
          //do nothing, but continue
        }
      }
    }
  },

  /**
   * Handle command event bubbling up from content.  This allows us to do chrome-
   * privileged things based on buttons in, e.g., unprivileged error pages.
   * Obviously, care should be taken not to trust events that web pages could have
   * synthesized.
   */
  _handleContentCommand: function (aEvent) {
    // Don't trust synthetic events
    if (!aEvent.isTrusted)
      return;

    var ot = aEvent.originalTarget;
    var errorDoc = ot.ownerDocument;

    // If the event came from an ssl error page, it is probably either the "Add
    // Exception…" or "Get me out of here!" button
    if (/^about:neterror\?e=nssBadCert/.test(errorDoc.documentURI)) {
      if (ot == errorDoc.getElementById('exceptionDialogButton')) {
        var params = { exceptionAdded : false };

        try {
          switch (gPrefService.getIntPref("browser.ssl_override_behavior")) {
            case 2 : // Pre-fetch & pre-populate
              params.prefetchCert = true;
            case 1 : // Pre-populate
              params.location = errorDoc.location.href;
          }
        } catch (e) {
          Components.utils.reportError("Couldn't get ssl_override pref: " + e);
        }

        window.openDialog('chrome://pippki/content/exceptionDialog.xul',
                          '','chrome,centerscreen,modal', params);

        // If the user added the exception cert, attempt to reload the page
        if (params.exceptionAdded)
          errorDoc.location.reload();
      }
      else if (ot == errorDoc.getElementById('getMeOutOfHereButton')) {
        // Get the start page from the *default* pref branch, not the user's
        var defaultPrefs = Cc["@mozilla.org/preferences-service;1"]
                          .getService(Ci.nsIPrefService).getDefaultBranch(null);
        var url = "about:blank";
        try {
          url = defaultPrefs.getCharPref("browser.startup.homepage");
          // If url is a pipe-delimited set of pages, just take the first one.
          if (url.indexOf("|") != -1)
            url = url.split("|")[0];
        } catch (e) { /* Fall back on about blank */ }

        Browser.currentBrowser.loadURI(url, null, null, false);
      }
    }
  }
};

function nsBrowserAccess()
{
}

nsBrowserAccess.prototype =
{
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIBrowserDOMWindow) || aIID.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

  openURI: function(aURI, aOpener, aWhere, aContext) {
    var isExternal = (aContext == Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);

    if (isExternal && aURI && aURI.schemeIs("chrome")) {
      dump("use -chrome command-line option to load external chrome urls\n");
      return null;
    }

    var loadflags = isExternal ?
                       Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL :
                       Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    var location;
    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW) {
      switch (aContext) {
        case Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL :
          aWhere = gPrefService.getIntPref("browser.link.open_external");
          break;
        default : // OPEN_NEW or an illegal value
          aWhere = gPrefService.getIntPref("browser.link.open_newwindow");
      }
    }

    var newWindow;
    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW) {
      var url = aURI ? aURI.spec : "about:blank";
      newWindow = openDialog("chrome://browser/content/browser.xul", "_blank",
                             "all,dialog=no", url, null, null, null);
    }
    else {
      if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_NEWTAB)
        newWindow = Browser.newTab(true).browser.contentWindow;
      else
        newWindow = aOpener ? aOpener.top : browser.contentWindow;
    }

    try {
      var referrer;
      if (aURI) {
        if (aOpener) {
          location = aOpener.location;
          referrer = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService)
                                                            .newURI(location, null, null);
        }
        newWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .loadURI(aURI.spec, loadflags, referrer, null, null);
      }
      newWindow.focus();
    } catch(e) { }

    return newWindow;
  },

  isTabContentWindow: function(aWindow) {
    return Browser._content.browsers.some(function (browser) browser.contentWindow == aWindow);
  }
}

/**
 * Utility class to handle manipulations of the identity indicators in the UI
 */
function IdentityHandler() {
  this._stringBundle = document.getElementById("bundle_browser");
  this._staticStrings = {};
  this._staticStrings[this.IDENTITY_MODE_DOMAIN_VERIFIED] = {
    encryption_label: this._stringBundle.getString("identity.encrypted")
  };
  this._staticStrings[this.IDENTITY_MODE_IDENTIFIED] = {
    encryption_label: this._stringBundle.getString("identity.encrypted")
  };
  this._staticStrings[this.IDENTITY_MODE_UNKNOWN] = {
    encryption_label: this._stringBundle.getString("identity.unencrypted")
  };

  this._cacheElements();
}

IdentityHandler.prototype = {

  // Mode strings used to control CSS display
  IDENTITY_MODE_IDENTIFIED       : "verifiedIdentity", // High-quality identity information
  IDENTITY_MODE_DOMAIN_VERIFIED  : "verifiedDomain",   // Minimal SSL CA-signed domain verification
  IDENTITY_MODE_UNKNOWN          : "unknownIdentity",  // No trusted identity information

  // Cache the most recent SSLStatus and Location seen in checkIdentity
  _lastStatus : null,
  _lastLocation : null,

  /**
   * Build out a cache of the elements that we need frequently.
   */
  _cacheElements: function() {
    this._identityPopup = document.getElementById("identity-popup");
    this._identityBox = document.getElementById("identity-box");
    this._identityPopupContentBox = document.getElementById("identity-popup-content-box");
    this._identityPopupContentHost = document.getElementById("identity-popup-content-host");
    this._identityPopupContentOwner = document.getElementById("identity-popup-content-owner");
    this._identityPopupContentSupp = document.getElementById("identity-popup-content-supplemental");
    this._identityPopupContentVerif = document.getElementById("identity-popup-content-verifier");
    this._identityPopupEncLabel = document.getElementById("identity-popup-encryption-label");
  },

  /**
   * Handler for mouseclicks on the "More Information" button in the
   * "identity-popup" panel.
   */
  handleMoreInfoClick: function(event) {
    displaySecurityInfo();
    event.stopPropagation();
  },

  /**
   * Helper to parse out the important parts of _lastStatus (of the SSL cert in
   * particular) for use in constructing identity UI strings
  */
  getIdentityData: function() {
    var result = {};
    var status = this._lastStatus.QueryInterface(Components.interfaces.nsISSLStatus);
    var cert = status.serverCert;

    // Human readable name of Subject
    result.subjectOrg = cert.organization;

    // SubjectName fields, broken up for individual access
    if (cert.subjectName) {
      result.subjectNameFields = {};
      cert.subjectName.split(",").forEach(function(v) {
        var field = v.split("=");
        this[field[0]] = field[1];
      }, result.subjectNameFields);

      // Call out city, state, and country specifically
      result.city = result.subjectNameFields.L;
      result.state = result.subjectNameFields.ST;
      result.country = result.subjectNameFields.C;
    }

    // Human readable name of Certificate Authority
    result.caOrg =  cert.issuerOrganization || cert.issuerCommonName;
    result.cert = cert;

    return result;
  },

  /**
   * Determine the identity of the page being displayed by examining its SSL cert
   * (if available) and, if necessary, update the UI to reflect this.  Intended to
   * be called by onSecurityChange
   *
   * @param PRUint32 state
   * @param JS Object location that mirrors an nsLocation (i.e. has .host and
   *                           .hostname and .port)
   */
  checkIdentity: function(state, location) {
    var currentStatus = getBrowser().securityUI
                                .QueryInterface(Components.interfaces.nsISSLStatusProvider)
                                .SSLStatus;
    this._lastStatus = currentStatus;
    this._lastLocation = location;

    if (state & Components.interfaces.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL)
      this.setMode(this.IDENTITY_MODE_IDENTIFIED);
    else if (state & Components.interfaces.nsIWebProgressListener.STATE_SECURE_HIGH)
      this.setMode(this.IDENTITY_MODE_DOMAIN_VERIFIED);
    else
      this.setMode(this.IDENTITY_MODE_UNKNOWN);
  },

  /**
   * Return the eTLD+1 version of the current hostname
   */
  getEffectiveHost: function() {
    // Cache the eTLDService if this is our first time through
    if (!this._eTLDService)
      this._eTLDService = Cc["@mozilla.org/network/effective-tld-service;1"]
                         .getService(Ci.nsIEffectiveTLDService);
    try {
      return this._eTLDService.getBaseDomainFromHost(this._lastLocation.hostname);
    } catch (e) {
      // If something goes wrong (e.g. hostname is an IP address) just fail back
      // to the full domain.
      return this._lastLocation.hostname;
    }
  },

  /**
   * Update the UI to reflect the specified mode, which should be one of the
   * IDENTITY_MODE_* constants.
   */
  setMode: function(newMode) {
    if (!this._identityBox) {
      // No identity box means the identity box is not visible, in which
      // case there's nothing to do.
      return;
    }

    this._identityBox.className = newMode;
    this.setIdentityMessages(newMode);

    // Update the popup too, if it's open
    if (this._identityPopup.state == "open")
      this.setPopupMessages(newMode);
  },

  /**
   * Set up the messages for the primary identity UI based on the specified mode,
   * and the details of the SSL cert, where applicable
   *
   * @param newMode The newly set identity mode.  Should be one of the IDENTITY_MODE_* constants.
   */
  setIdentityMessages: function(newMode) {
    if (newMode == this.IDENTITY_MODE_DOMAIN_VERIFIED) {
      var iData = this.getIdentityData();

      // We need a port number for all lookups.  If one hasn't been specified, use
      // the https default
      var lookupHost = this._lastLocation.host;
      if (lookupHost.indexOf(':') < 0)
        lookupHost += ":443";

      // Cache the override service the first time we need to check it
      if (!this._overrideService)
        this._overrideService = Components.classes["@mozilla.org/security/certoverride;1"]
                                          .getService(Components.interfaces.nsICertOverrideService);

      // Verifier is either the CA Org, for a normal cert, or a special string
      // for certs that are trusted because of a security exception.
      var tooltip = this._stringBundle.getFormattedString("identity.identified.verifier",
                                                          [iData.caOrg]);

      // Check whether this site is a security exception. XPConnect does the right
      // thing here in terms of converting _lastLocation.port from string to int, but
      // the overrideService doesn't like undefined ports, so make sure we have
      // something in the default case (bug 432241).
      if (this._overrideService.hasMatchingOverride(this._lastLocation.hostname,
                                                    (this._lastLocation.port || 443),
                                                    iData.cert, {}, {}))
        tooltip = this._stringBundle.getString("identity.identified.verified_by_you");
    }
    else if (newMode == this.IDENTITY_MODE_IDENTIFIED) {
      // If it's identified, then we can populate the dialog with credentials
      iData = this.getIdentityData();
      tooltip = this._stringBundle.getFormattedString("identity.identified.verifier",
                                                      [iData.caOrg]);
    }
    else {
      tooltip = this._stringBundle.getString("identity.unknown.tooltip");
    }

    // Push the appropriate strings out to the UI
    this._identityBox.tooltipText = tooltip;
  },

  /**
   * Set up the title and content messages for the identity message popup,
   * based on the specified mode, and the details of the SSL cert, where
   * applicable
   *
   * @param newMode The newly set identity mode.  Should be one of the IDENTITY_MODE_* constants.
   */
  setPopupMessages: function(newMode) {

    this._identityPopup.className = newMode;
    this._identityPopupContentBox.className = newMode;

    // Set the static strings up front
    this._identityPopupEncLabel.textContent = this._staticStrings[newMode].encryption_label;

    // Initialize the optional strings to empty values
    var supplemental = "";
    var verifier = "";

    if (newMode == this.IDENTITY_MODE_DOMAIN_VERIFIED) {
      var iData = this.getIdentityData();
      var host = this.getEffectiveHost();
      var owner = this._stringBundle.getString("identity.ownerUnknown2");
      verifier = this._identityBox.tooltipText;
      supplemental = "";
    }
    else if (newMode == this.IDENTITY_MODE_IDENTIFIED) {
      // If it's identified, then we can populate the dialog with credentials
      iData = this.getIdentityData();
      host = this.getEffectiveHost();
      owner = iData.subjectOrg;
      verifier = this._identityBox.tooltipText;

      // Build an appropriate supplemental block out of whatever location data we have
      if (iData.city)
        supplemental += iData.city + "\n";
      if (iData.state && iData.country)
        supplemental += this._stringBundle.getFormattedString("identity.identified.state_and_country",
                                                              [iData.state, iData.country]);
      else if (iData.state) // State only
        supplemental += iData.state;
      else if (iData.country) // Country only
        supplemental += iData.country;
    }
    else {
      // These strings will be hidden in CSS anyhow
      host = "";
      owner = "";
    }

    // Push the appropriate strings out to the UI
    this._identityPopupContentHost.textContent = host;
    this._identityPopupContentOwner.textContent = owner;
    this._identityPopupContentSupp.textContent = supplemental;
    this._identityPopupContentVerif.textContent = verifier;
  },

  hideIdentityPopup: function() {
    this._identityPopup.hidePopup();
  },

  /**
   * Click handler for the identity-box element in primary chrome.
   */
  handleIdentityButtonEvent: function(event) {

    event.stopPropagation();

    if ((event.type == "click" && event.button != 0) ||
        (event.type == "keypress" && event.charCode != KeyEvent.DOM_VK_SPACE &&
         event.keyCode != KeyEvent.DOM_VK_RETURN))
      return; // Left click, space or enter only

    // Make sure that the display:none style we set in xul is removed now that
    // the popup is actually needed
    this._identityPopup.hidden = false;

    // Tell the popup to consume dismiss clicks, to avoid bug 395314
    this._identityPopup.popupBoxObject
        .setConsumeRollupEvent(Ci.nsIPopupBoxObject.ROLLUP_CONSUME);

    // Update the popup strings
    this.setPopupMessages(this._identityBox.className);

    // Now open the popup, anchored off the primary chrome element
    this._identityPopup.openPopup(this._identityBox, 'after_start');
  }
};

var gIdentityHandler;

/**
 * Returns the singleton instance of the identity handler class.  Should always be
 * used instead of referencing the global variable directly or creating new instances
 */
function getIdentityHandler() {
  if (!gIdentityHandler)
    gIdentityHandler = new IdentityHandler();
  return gIdentityHandler;
}


/**
 * Handler for blocked popups, triggered by DOMUpdatePageReport events in browser.xml
 */
const gPopupBlockerObserver = {
  _kIPM: Components.interfaces.nsIPermissionManager,

  onUpdatePageReport: function (aEvent)
  {
    var cBrowser = Browser.currentBrowser;
    if (aEvent.originalTarget != cBrowser)
      return;

    if (!cBrowser.pageReport)
      return;

    // Only show the notification again if we've not already shown it. Since
    // notifications are per-browser, we don't need to worry about re-adding
    // it.
    if (!cBrowser.pageReport.reported) {
      if(gPrefService.getBoolPref("privacy.popups.showBrowserMessage")) {
        var bundle_browser = document.getElementById("bundle_browser");
        var brandBundle = document.getElementById("bundle_brand");
        var brandShortName = brandBundle.getString("brandShortName");
        var message;
        var popupCount = cBrowser.pageReport.length;

        if (popupCount > 1)
          message = bundle_browser.getFormattedString("popupWarningMultiple", [brandShortName, popupCount]);
        else
          message = bundle_browser.getFormattedString("popupWarning", [brandShortName]);

        var notificationBox = Browser.getNotificationBox();
        var notification = notificationBox.getNotificationWithValue("popup-blocked");
        if (notification) {
          notification.label = message;
        }
        else {
          var buttons = [
            {
              label: bundle_browser.getString("popupButtonAlwaysAllow"),
              accessKey: bundle_browser.getString("popupButtonAlwaysAllow.accesskey"),
              callback: function() { gPopupBlockerObserver.toggleAllowPopupsForSite(); }
            },
            {
              label: bundle_browser.getString("popupButtonNeverWarn"),
              accessKey: bundle_browser.getString("popupButtonNeverWarn.accesskey"),
              callback: function() { gPopupBlockerObserver.dontShowMessage(); }
            }
          ];

          const priority = notificationBox.PRIORITY_WARNING_MEDIUM;
          notificationBox.appendNotification(message, "popup-blocked",
                                             "",
                                             priority, buttons);
        }
      }
      // Record the fact that we've reported this blocked popup, so we don't
      // show it again.
      cBrowser.pageReport.reported = true;
    }
  },

  toggleAllowPopupsForSite: function (aEvent)
  {
    var currentURI = Browser.currentBrowser.webNavigation.currentURI;
    var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                       .getService(this._kIPM);
    pm.add(currentURI, "popup", this._kIPM.ALLOW_ACTION);

    Browser.getNotificationBox().removeCurrentNotification();
  },

  dontShowMessage: function ()
  {
    var showMessage = gPrefService.getBoolPref("privacy.popups.showBrowserMessage");
    gPrefService.setBoolPref("privacy.popups.showBrowserMessage", !showMessage);
    Browser.getNotificationBox().removeCurrentNotification();
  }
};

const gXPInstallObserver = {
  observe: function (aSubject, aTopic, aData)
  {
    var brandBundle = document.getElementById("bundle_brand");
    var browserBundle = document.getElementById("bundle_browser");
    switch (aTopic) {
      case "xpinstall-install-blocked":
        var installInfo = aSubject.QueryInterface(Components.interfaces.nsIXPIInstallInfo);
        var host = installInfo.originatingURI.host;
        var brandShortName = brandBundle.getString("brandShortName");
        var notificationName, messageString, buttons;
        if (!gPrefService.getBoolPref("xpinstall.enabled")) {
          notificationName = "xpinstall-disabled"
          if (gPrefService.prefIsLocked("xpinstall.enabled")) {
            messageString = browserBundle.getString("xpinstallDisabledMessageLocked");
            buttons = [];
          }
          else {
            messageString = browserBundle.getFormattedString("xpinstallDisabledMessage",
                                                             [brandShortName, host]);
            buttons = [{
              label: browserBundle.getString("xpinstallDisabledButton"),
              accessKey: browserBundle.getString("xpinstallDisabledButton.accesskey"),
              popup: null,
              callback: function editPrefs() {
                gPrefService.setBoolPref("xpinstall.enabled", true);
                return false;
              }
            }];
          }
        }
        else {
          notificationName = "xpinstall"
          messageString = browserBundle.getFormattedString("xpinstallPromptWarning",
                                                           [brandShortName, host]);

          buttons = [{
            label: browserBundle.getString("xpinstallPromptAllowButton"),
            accessKey: browserBundle.getString("xpinstallPromptAllowButton.accesskey"),
            popup: null,
            callback: function() {
              // Force the addon manager panel to appear
              CommandUpdater.doCommand("cmd_addons");

              var mgr = Cc["@mozilla.org/xpinstall/install-manager;1"].createInstance(Ci.nsIXPInstallManager);
              mgr.initManagerWithInstallInfo(installInfo);
              return false;
            }
          }];
        }

        var nBox = Browser.getNotificationBox();
        if (!nBox.getNotificationWithValue(notificationName)) {
          const priority = nBox.PRIORITY_WARNING_MEDIUM;
          const iconURL = "chrome://mozapps/skin/update/update.png";
          nBox.appendNotification(messageString, notificationName, iconURL, priority, buttons);
        }
        break;
    }
  }
};

function getNotificationBox(aWindow) {
  return Browser.getNotificationBox();
}


function ProgressController(tab) {
  this._tab = tab;
}

ProgressController.prototype = {
  get browser() {
    return this._tab.browser;
  },

  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    // ignore notification that aren't about the main document (iframes, etc)
    if (aWebProgress.DOMWindow != this._tab.browser.contentWindow)
      return;

    if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_START)
        this._networkStart();
      else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP)
        this._networkStop();
    } else if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        this._documentStop();
      }
    }
  },

  // This method is called to indicate progress changes for the currently
  // loading page.
  onProgressChange: function(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
  },

  // This method is called to indicate a change to the current location.
  onLocationChange: function(aWebProgress, aRequest, aLocationURI) {
    // XXX this code is not multiple-tab friendly.

    var location = aLocationURI ? aLocationURI.spec : "";
    this._hostChanged = true;

    // This code here does not compare uris exactly when determining
    // whether or not the message(s) should be hidden since the message
    // may be prematurely hidden when an install is invoked by a click
    // on a link that looks like this:
    //
    // <a href="#" onclick="return install();">Install Foo</a>
    //
    // - which fires a onLocationChange message to uri + '#'...
    currentBrowser = Browser.currentBrowser;
    if (currentBrowser.lastURI) {
      var oldSpec = currentBrowser.lastURI.spec;
      var oldIndexOfHash = oldSpec.indexOf("#");
      if (oldIndexOfHash != -1)
        oldSpec = oldSpec.substr(0, oldIndexOfHash);
      var newSpec = location;
      var newIndexOfHash = newSpec.indexOf("#");
      if (newIndexOfHash != -1)
        newSpec = newSpec.substr(0, newSpec.indexOf("#"));
      if (newSpec != oldSpec) {
        // Remove all the notifications, except for those which want to
        // persist across the first location change.

	// XXX
        // var nBox = Browser.getNotificationBox();
        // nBox.removeTransientNotifications();
      }
    }
    currentBrowser.lastURI = aLocationURI;
    if (aWebProgress.DOMWindow == Browser.currentBrowser.contentWindow) {
      BrowserUI.setURI();
    }
  },

  // This method is called to indicate a status changes for the currently
  // loading page.  The message is already formatted for display.
  onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage) {
  },

  _networkStart: function() {
    this._tab.setLoading(true);
    //dump("started loading network\n");

    if (Browser.currentBrowser == this.browser) {
      Browser.content.startLoading();
      BrowserUI.update(TOOLBARSTATE_LOADING);
    }
  },

  _networkStop: function() {
    this._tab.setLoading(false);

    if (Browser.currentBrowser == this.browser) {
      BrowserUI.update(TOOLBARSTATE_LOADED);
    }
  },

  _documentStop: function() {
    //dump("stop, hammer time\n");

    // translate any phone numbers
    Browser.translatePhoneNumbers();

    if (Browser.currentBrowser == this.browser) {
      // focus the dom window
      this.browser.contentWindow.focus();

      Browser.content.endLoading();
    }
    this._tab.updateThumbnail();
  },

 // Properties used to cache security state used to update the UI
  _state: null,
  _host: undefined,
  _hostChanged: false, // onLocationChange will flip this bit

  // This method is called when the security state of the browser changes.
  onSecurityChange: function(aWebProgress, aRequest, aState) {
    // Don't need to do anything if the data we use to update the UI hasn't
    // changed
    if (this._state == aState &&
        !this._hostChanged) {
      return;
    }
    this._state = aState;

    try {
      this._host = getBrowser().contentWindow.location.host;
    }
    catch(ex) {
      this._host = null;
    }

    this._hostChanged = false;

    // Don't pass in the actual location object, since it can cause us to
    // hold on to the window object too long.  Just pass in the fields we
    // care about. (bug 424829)
    var location = getBrowser().contentWindow.location;
    var locationObj = {};
    try {
      locationObj.host = location.host;
      locationObj.hostname = location.hostname;
      locationObj.port = location.port;
    }
    catch (ex) {
      // Can sometimes throw if the URL being visited has no host/hostname,
      // e.g. about:blank. The _state for these pages means we won't need these
      // properties anyways, though.
    }
    getIdentityHandler().checkIdentity(this._state, locationObj);

  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
        aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};


function Tab() {
}

Tab.prototype = {
  _id: null,
  _browser: null,
  _state: null,
  _listener: null,
  _loading: false,
  _content: null,

  get browser() {
    return this._browser;
  },

  get content() {
    return this._content;
  },

  isLoading: function() {
    return this._loading;
  },

  setLoading: function(b) {
    this._loading = b;
  },

  create: function() {
    this._content = document.createElement("richlistitem");
    this._content.setAttribute("type", "documenttab");
    document.getElementById("tabs").addTab(this._content);

    this._createBrowser();
  },

  destroy: function() {
    this._destroyBrowser();
    document.getElementById("tabs").removeTab(this._content);
  },

  _createBrowser: function() {
    if (this._browser)
      throw "Browser already exists";

    let browser = this._browser = document.createElement("browser");
    browser.className = "deckbrowser-browser";
    browser.setAttribute("style", "overflow: hidden; visibility: hidden; width: 1024px; height: 800px;");
    browser.setAttribute("contextmenu", document.getElementById("canvas").getAttribute("contextmenu"));
    browser.setAttribute("autocompletepopup", document.getElementById("canvas").getAttribute("autocompletepopup"));
    browser.setAttribute("type", "content");

    document.getElementById("browsers").appendChild(browser);

    this._listener = new ProgressController(this);
    browser.addProgressListener(this._listener);
  },

  _destroyBrowser: function() {
    document.getElementById("browsers").removeChild(this._browser);
  },

  saveState: function() {
    let state = { };

    this._url = browser.contentWindow.location.toString();
    var browser = this.getBrowserForDisplay(display);
    var doc = browser.contentDocument;
    if (doc instanceof HTMLDocument) {
      var tags = ["input", "textarea", "select"];

      for (var t = 0; t < tags.length; t++) {
        var elements = doc.getElementsByTagName(tags[t]);
        for (var e = 0; e < elements.length; e++) {
          var element = elements[e];
          var id;
          if (element.id)
            id = "#" + element.id;
          else if (element.name)
            id = "$" + element.name;

          if (id)
            state[id] = element.value;
        }
      }
    }

    state._scrollX = browser.contentWindow.scrollX;
    state._scrollY = browser.contentWindow.scrollY;

    this._state = state;
  },

  restoreState: function() {
    let state = this._state;
    if (!state)
      return;

    let doc = this._browser.contentDocument;
    for (item in state) {
      var elem = null;
      if (item.charAt(0) == "#") {
        elem = doc.getElementById(item.substring(1));
      }
      else if (item.charAt(0) == "$") {
        var list = doc.getElementsByName(item.substring(1));
        if (list.length)
          elem = list[0];
      }

      if (elem)
        elem.value = state[item];
    }

    this._browser.contentWindow.scrollTo(state._scrollX, state._scrollY);
  },

  updateThumbnail: function() {
    if (!this._browser)
      return;

    let srcCanvas = (Browser.currentBrowser == this._browser) ? document.getElementById("canvas") : null;
    this._content.updateThumbnail(this._browser, srcCanvas);
  }
}
