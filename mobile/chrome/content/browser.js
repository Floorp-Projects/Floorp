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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;
let Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm")

XPCOMUtils.defineLazyServiceGetter(this, "URIFixup",
  "@mozilla.org/docshell/urifixup;1", "nsIURIFixup");

// TODO: Take into account ppi in these units?

// The ratio of velocity that is retained every ms.
const kPanDeceleration = 0.999;

// The number of ms to consider events over for a swipe gesture.
const kSwipeLength = 500;

// The number of pixels to move before we consider a drag to be more than
// just a click with jitter.
const kDragThreshold = 10;

// The number of pixels to move to break out of axis-lock
const kLockBreakThreshold = 100;

// Minimum speed to move during kinetic panning. 0.015 pixels/ms is roughly
// equivalent to a pixel every 4 frames at 60fps.
const kMinKineticSpeed = 0.015;

// Maximum kinetic panning speed. 9 pixels/ms is equivalent to 150 pixels per
// frame at 60fps.
const kMaxKineticSpeed = 9;

// The maximum magnitude of disparity allowed between axes acceleration. If
// it's larger than this, lock the slow-moving axis.
const kAxisLockRatio = 5;

function dump(a) {
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(a);
}

function sendMessageToJava(aMessage) {
  let bridge = Cc["@mozilla.org/android/bridge;1"].getService(Ci.nsIAndroidBridge);
  return bridge.handleGeckoMessage(JSON.stringify(aMessage));
}

function resolveGeckoURI(aURI) {
  if (aURI.indexOf("chrome://") == 0) {
    let registry = Cc['@mozilla.org/chrome/chrome-registry;1'].getService(Ci["nsIChromeRegistry"]);
    return registry.convertChromeURL(Services.io.newURI(aURI, null, null)).spec;
  } else if (aURI.indexOf("resource://") == 0) {
    let handler = Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler);
    return handler.resolveURI(Services.io.newURI(aURI, null, null));
  }
  return aURI;
}

/**
 * Cache of commonly used string bundles.
 */
var Strings = {};
[
  ["brand",      "chrome://branding/locale/brand.properties"],
  ["browser",    "chrome://browser/locale/browser.properties"]
].forEach(function (aStringBundle) {
  let [name, bundle] = aStringBundle;
  XPCOMUtils.defineLazyGetter(Strings, name, function() {
    return Services.strings.createBundle(bundle);
  });
});

var BrowserApp = {
  _tabs: [],
  _selectedTab: null,

  deck: null,

  startup: function startup() {
    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = new nsBrowserAccess();
    dump("zerdatime " + Date.now() + " - browser chrome startup finished.");

    this.deck = document.getElementById("browsers");
    BrowserEventHandler.init();

    Services.obs.addObserver(this, "Tab:Add", false);
    Services.obs.addObserver(this, "Tab:Load", false);
    Services.obs.addObserver(this, "Tab:Select", false);
    Services.obs.addObserver(this, "Tab:Close", false);
    Services.obs.addObserver(this, "session-back", false);
    Services.obs.addObserver(this, "session-reload", false);
    Services.obs.addObserver(this, "SaveAs:PDF", false);
    Services.obs.addObserver(this, "Preferences:Get", false);
    Services.obs.addObserver(this, "Preferences:Set", false);
    Services.obs.addObserver(this, "ScrollTo:FocusedInput", false);
    Services.obs.addObserver(this, "Sanitize:ClearAll", false);

    Services.obs.addObserver(XPInstallObserver, "addon-install-blocked", false);
    Services.obs.addObserver(XPInstallObserver, "addon-install-started", false);

    NativeWindow.init();
    Downloads.init();

    // Init LoginManager
    Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);

    let uri = "about:support";
    if ("arguments" in window && window.arguments[0])
      uri = window.arguments[0];

    // XXX maybe we don't do this if the launch was kicked off from external
    Services.io.offline = false;
    let newTab = this.addTab(uri);
    newTab.active = true;

    // Broadcast a UIReady message so add-ons know we are finished with startup
    let event = document.createEvent("Events");
    event.initEvent("UIReady", true, false);
    window.dispatchEvent(event);

    // notify java that gecko has loaded
    sendMessageToJava({
      gecko: {
        type: "Gecko:Ready"
      }
    });
  },

  shutdown: function shutdown() {
    NativeWindow.uninit();

    Services.obs.removeObserver(XPInstallObserver, "addon-install-blocked");
    Services.obs.removeObserver(XPInstallObserver, "addon-install-started");
  },

  get tabs() {
    return this._tabs;
  },

  get selectedTab() {
    return this._selectedTab;
  },

  set selectedTab(aTab) {
    this._selectedTab = aTab;
    if (!aTab)
      return;

    this.deck.selectedPanel = aTab.browser;
  },

  get selectedBrowser() {
    if (this._selectedTab)
      return this._selectedTab.browser;
    return null;
  },

  getTabForId: function getTabForId(aId) {
    let tabs = this._tabs;
    for (let i=0; i < tabs.length; i++) {
       if (tabs[i].id == aId)
         return tabs[i];
    }
    return null;
  },

  getTabForBrowser: function getTabForBrowser(aBrowser) {
    let tabs = this._tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser == aBrowser)
        return tabs[i];
    }
    return null;
  },

  getBrowserForWindow: function getBrowserForWindow(aWindow) {
    let tabs = this._tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser.contentWindow == aWindow)
        return tabs[i].browser;
    }
    return null;
  },

  getBrowserForDocument: function getBrowserForDocument(aDocument) {
    let tabs = this._tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser.contentDocument == aDocument)
        return tabs[i].browser;
    }
    return null;
  },

  loadURI: function loadURI(aURI, aParams) {
    let browser = this.selectedBrowser;
    if (!browser)
      return;

    let flags = aParams.flags || Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    let postData = ("postData" in aParams && aParams.postData) ? aParams.postData.value : null;
    let referrerURI = "referrerURI" in aParams ? aParams.referrerURI : null;
    let charset = "charset" in aParams ? aParams.charset : null;
    browser.loadURIWithFlags(aURI, flags, referrerURI, charset, postData);
  },

  addTab: function addTab(aURI) {
    let newTab = new Tab(aURI);
    this._tabs.push(newTab);
    return newTab;
  },

  closeTab: function closeTab(aTab) {
    if (aTab == this.selectedTab)
      this.selectedTab = null;

    aTab.destroy();
    this._tabs.splice(this._tabs.indexOf(aTab), 1);
  },

  selectTab: function selectTab(aTab) {
    if (aTab != null) {
      this.selectedTab = aTab;
      aTab.active = true;
      let message = {
        gecko: {
          type: "Tab:Selected",
          tabID: aTab.id
        }
      };

      sendMessageToJava(message);
    }
  },

  saveAsPDF: function saveAsPDF(aBrowser) {
    // Create the final destination file location
    let ContentAreaUtils = {};
    Services.scriptloader.loadSubScript("chrome://global/content/contentAreaUtils.js", ContentAreaUtils);
    let fileName = ContentAreaUtils.getDefaultFileName(aBrowser.contentTitle, aBrowser.documentURI, null, null);
    fileName = fileName.trim() + ".pdf";

    let dm = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
    let downloadsDir = dm.defaultDownloadsDirectory;

    let file = downloadsDir.clone();
    file.append(fileName);
    file.createUnique(file.NORMAL_FILE_TYPE, 0666);
    fileName = file.leafName;

    // We must manually add this to the download system
    let db = dm.DBConnection;

    let stmt = db.createStatement(
      "INSERT INTO moz_downloads (name, source, target, startTime, endTime, state, referrer) " +
      "VALUES (:name, :source, :target, :startTime, :endTime, :state, :referrer)"
    );

    let current = aBrowser.currentURI.spec;
    stmt.params.name = fileName;
    stmt.params.source = current;
    stmt.params.target = Services.io.newFileURI(file).spec;
    stmt.params.startTime = Date.now() * 1000;
    stmt.params.endTime = Date.now() * 1000;
    stmt.params.state = Ci.nsIDownloadManager.DOWNLOAD_NOTSTARTED;
    stmt.params.referrer = current;
    stmt.execute();
    stmt.finalize();

    let newItemId = db.lastInsertRowID;
    let download = dm.getDownload(newItemId);
    try {
      DownloadsView.downloadStarted(download);
    }
    catch(e) {}
    Services.obs.notifyObservers(download, "dl-start", null);

    let printSettings = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(Ci.nsIPrintSettingsService).newPrintSettings;
    printSettings.printSilent = true;
    printSettings.showPrintProgress = false;
    printSettings.printBGImages = true;
    printSettings.printBGColors = true;
    printSettings.printToFile = true;
    printSettings.toFileName = file.path;
    printSettings.printFrameType = Ci.nsIPrintSettings.kFramesAsIs;
    printSettings.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;

    //XXX we probably need a preference here, the header can be useful
    printSettings.footerStrCenter = "";
    printSettings.footerStrLeft   = "";
    printSettings.footerStrRight  = "";
    printSettings.headerStrCenter = "";
    printSettings.headerStrLeft   = "";
    printSettings.headerStrRight  = "";

    let listener = {
      onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {},
      onProgressChange : function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {},

      // stubs for the nsIWebProgressListener interfaces which nsIWebBrowserPrint doesn't use.
      onLocationChange : function() {},
      onStatusChange: function() {},
      onSecurityChange : function() {}
    };

    let webBrowserPrint = content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebBrowserPrint);
    webBrowserPrint.print(printSettings, listener);
  },

  getPreferences: function getPreferences(aPrefNames) {
    try {
      let json = JSON.parse(aPrefNames);
      let prefs = [];
      
      for each (let prefName in json) {
        let pref = {
          name: prefName
        };

        try {
          switch (Services.prefs.getPrefType(prefName)) {
            case Ci.nsIPrefBranch.PREF_BOOL:
              pref.type = "bool";
              pref.value = Services.prefs.getBoolPref(prefName);
              break;
            case Ci.nsIPrefBranch.PREF_INT:
              pref.type = "int";
              pref.value = Services.prefs.getIntPref(prefName);
              break;
            case Ci.nsIPrefBranch.PREF_STRING:
            default:
              pref.type = "string";
              pref.value = Services.prefs.getComplexValue(prefName, Ci.nsISupportsString).data;
              break;
          }
        } catch (e) {
            // preference does not exist; do not send it
            continue;
        }

        // some preferences use integers or strings instead of booleans for
        // indicating enabled/disabled. since the java ui uses the type to
        // determine which ui elements to show, we need to normalize these
        // preferences to be actual booleans.
        switch (prefName) {
          case "network.cookie.cookieBehavior":
            pref.type = "bool";
            pref.value = pref.value == 0;
            break;
          case "permissions.default.image":
            pref.type = "bool";
            pref.value = pref.value == 1;
            break;
          case "browser.menu.showCharacterEncoding":
            pref.type = "bool";
            pref.value = pref.value == "true";
            break;
        }

        prefs.push(pref);
      }

      sendMessageToJava({
        gecko: {
          type: "Preferences:Data",
          preferences: prefs
        }
      });
    } catch (e) {}
  },

  setPreferences: function setPreferences(aPref) {
    let json = JSON.parse(aPref);

    // when sending to java, we normalized special preferences that use
    // integers and strings to represent booleans.  here, we convert them back
    // to their actual types so we can store them.
    switch (json.name) {
      case "network.cookie.cookieBehavior":
        json.type = "int";
        json.value = (json.value ? 0 : 2);
        break;
      case "permissions.default.image":
        json.type = "int";
        json.value = (json.value ? 1 : 2);
        break;
      case "browser.menu.showCharacterEncoding":
        json.type = "string";
        json.value = (json.value ? "true" : "false");
        break;
    }

    if (json.type == "bool")
      Services.prefs.setBoolPref(json.name, json.value);
    else if (json.type == "int")
      Services.prefs.setIntPref(json.name, json.value);
    else {
      let pref = Cc["@mozilla.org/pref-localizedstring;1"].createInstance(Ci.nsIPrefLocalizedString);
      pref.data = json.value;
      Services.prefs.setComplexValue(json.name, Ci.nsISupportsString, pref);
    }
  },

  scrollToFocusedInput: function(aBrowser) {
    let doc = aBrowser.contentDocument;
    if (!doc)
      return;
    let focused = doc.activeElement;
    if ((focused instanceof HTMLInputElement && focused.mozIsTextField(false)) || (focused instanceof HTMLTextAreaElement))
      focused.scrollIntoView(false);
  },

  observe: function(aSubject, aTopic, aData) {
    let browser = this.selectedBrowser;
    if (!browser)
      return;

    if (aTopic == "session-back") {
      browser.goBack();
    } else if (aTopic == "session-reload") {
      browser.reload();
    } else if (aTopic == "Tab:Add") {
      let uri = URIFixup.createFixupURI(aData, Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP);
      let newTab = this.addTab(uri ? uri.spec : aData);
      newTab.active = true;
    } else if (aTopic == "Tab:Load") {
      let uri = URIFixup.createFixupURI(aData, Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP);
      browser.loadURI(uri ? uri.spec : aData);
    } else if (aTopic == "Tab:Select") {
      this.selectTab(this.getTabForId(parseInt(aData)));
    } else if (aTopic == "Tab:Close") {
      this.closeTab(this.getTabForId(parseInt(aData)));
    } else if (aTopic == "SaveAs:PDF") {
      this.saveAsPDF(browser);
    } else if (aTopic == "Preferences:Get") {
      this.getPreferences(aData);
    } else if (aTopic == "Preferences:Set") {
      this.setPreferences(aData);
    } else if (aTopic == "ScrollTo:FocusedInput") {
      this.scrollToFocusedInput(browser);
    } else if (aTopic == "Sanitize:ClearAll") {
      Sanitizer.sanitize();
    }
  }
}

var NativeWindow = {
  init: function() {
    Services.obs.addObserver(this, "Menu:Clicked", false);
    Services.obs.addObserver(this, "Doorhanger:Reply", false);
  },

  uninit: function() {
    Services.obs.removeObserver(this, "Menu:Clicked");
    Services.obs.removeObserver(this, "Doorhanger:Reply");
  },

  toast: {
    show: function(aMessage, aDuration) {
      sendMessageToJava({
        gecko: {
          type: "Toast:Show",
          message: aMessage,
          duration: aDuration
        }
      });
    }
  },

  menu: {
    _callbacks: [],
    _menuId: 0,
    add: function(aName, aIcon, aCallback) {
      sendMessageToJava({
        gecko: {
          type: "Menu:Add",
          name: aName,
          icon: aIcon,
          id: this._menuId
        }
      });
      this._callbacks[this._menuId] = aCallback;
      this._menuId++;
      return this._menuId - 1;
    },

    remove: function(aId) {
      sendMessageToJava({ gecko: {type: "Menu:Remove", id: aId }});
    }
  },

  doorhanger: {
    _callbacks: [],
    _callbacksId: 0,
    _promptId: 0,
    show: function(aMessage, aButtons, aTab) {
      // use the current tab if none is provided
      let tabID = aTab ? aTab : BrowserApp.selectedTab.id;
      aButtons.forEach((function(aButton) {
        this._callbacks[this._callbacksId] = { cb: aButton.callback, prompt: this._promptId };
        aButton.callback = this._callbacksId;
        this._callbacksId++;
      }).bind(this));

      this._promptId++;
      let json = {
        gecko: {
          type: "Doorhanger:Add",
          message: aMessage,
          severity: "PRIORITY_WARNING_MEDIUM",
          buttons: aButtons,
          tabID: tabID
        }
      };
      sendMessageToJava(json);
    }
  },
  
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "Menu:Clicked") {
      if (this.menu._callbacks[aData])
        this.menu._callbacks[aData]();
    } else if (aTopic == "Doorhanger:Reply") {
      let id = parseInt(aData);
      if (this.doorhanger._callbacks[id]) {
        let prompt = this.doorhanger._callbacks[id].prompt;
        this.doorhanger._callbacks[id].cb();
        for (let i = 0; i < this._callbacksId; i++) {
          if (this._callbacks[i] && this._callbacks[i].prompt == prompt)
            delete this._callbacks[i];
        }
      }
    }
  }
};


function nsBrowserAccess() {
}

nsBrowserAccess.prototype = {
  openURI: function browser_openURI(aURI, aOpener, aWhere, aContext) {
    let isExternal = (aContext == Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);

    dump("nsBrowserAccess::openURI");
    let browser = BrowserApp.selectedBrowser;
    if (!browser || isExternal) {
      let tab = BrowserApp.addTab("about:blank");
      BrowserApp.selectTab(tab);
      browser = tab.browser;
    }

    // Why does returning the browser.contentWindow not work here?
    Services.io.offline = false;
    browser.loadURI(aURI.spec, null, null);
    return null;
  },

  openURIInFrame: function browser_openURIInFrame(aURI, aOpener, aWhere, aContext) {
    dump("nsBrowserAccess::openURIInFrame");
    return null;
  },

  isTabContentWindow: function(aWindow) {
    return BrowserApp.getBrowserForWindow(aWindow) != null;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBrowserDOMWindow])
};


let gTabIDFactory = 0;

function Tab(aURL) {
  this.filter = {
    percentage: 0,
    requestsFinished: 0,
    requestsTotal: 0
  };

  this.browser = null;
  this.id = 0;
  this.create(aURL);
}

Tab.prototype = {
  create: function(aURL) {
    if (this.browser)
      return;

    this.browser = document.createElement("browser");
    this.browser.setAttribute("type", "content");
    BrowserApp.deck.appendChild(this.browser);
    this.browser.stop();

    let flags = Ci.nsIWebProgress.NOTIFY_STATE_ALL |
                Ci.nsIWebProgress.NOTIFY_LOCATION |
                Ci.nsIWebProgress.NOTIFY_PROGRESS;
    this.browser.addProgressListener(this, flags);
    this.browser.loadURI(aURL);

    this.id = ++gTabIDFactory;
    let message = {
      gecko: {
        type: "Tab:Added",
        tabID: this.id,
        uri: aURL
      }
    };

    sendMessageToJava(message);
  },

  destroy: function() {
    if (!this.browser)
      return;

    this.browser.removeProgressListener(this);
    BrowserApp.deck.removeChild(this.browser);
    this.browser = null;
    let message = {
      gecko: {
        type: "Tab:Closed",
        tabID: this.id
      }
    };

    sendMessageToJava(message);
  },

  set active(aActive) {
    if (!this.browser)
      return;

    if (aActive) {
      this.browser.setAttribute("type", "content-primary");
      this.browser.focus();
      BrowserApp.selectedTab = this;
    } else {
      this.browser.setAttribute("type", "content");
    }
  },

  get active() {
    if (!this.browser)
      return false;
    return this.browser.getAttribute("type") == "content-primary";
  },

  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
        // Reset filter members
        this.filter = {
          percentage: 0,
          requestsFinished: 0,
          requestsTotal: 0
        };
      }
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_REQUEST)
        // Filter optimization: If we have more than one request, show progress
        //based on requests completing, not on percent loaded of each request
        ++this.filter.requestsTotal;
    }
    else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_REQUEST) {
        // Filter optimization: Request has completed, so send a "progress change"
        // Note: aRequest is null
        ++this.filter.requestsFinished;
        this.onProgressChange(aWebProgress, null, 0, 0, this.filter.requestsFinished, this.filter.requestsTotal);
      }
    }

    if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT) {
      // Filter optimization: Only really send DOCUMENT state changes to Java listener
      let browser = BrowserApp.getBrowserForWindow(aWebProgress.DOMWindow);
      let uri = "";
      if (browser)
        uri = browser.currentURI.spec;
  
      let message = {
        gecko: {
          type: "onStateChange",
          tabID: this.id,
          uri: uri,
          state: aStateFlags
        }
      };
  
      sendMessageToJava(message);
    }
  },

  onLocationChange: function(aWebProgress, aRequest, aLocationURI) {
    let contentWin = aWebProgress.DOMWindow;
    if (contentWin != contentWin.top)
        return;

    let browser = BrowserApp.getBrowserForWindow(contentWin);
    let uri = browser.currentURI.spec;

    let message = {
      gecko: {
        type: "onLocationChange",
        tabID: this.id,
        uri: uri
      }
    };

    sendMessageToJava(message);
  },

  onSecurityChange: function(aBrowser, aWebProgress, aRequest, aState) {
  },

  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {
    // Filter optimization: Don't send garbage
    if (aCurTotalProgress > aMaxTotalProgress || aMaxTotalProgress <= 0)
      return;

    // Filter optimization: Are we sending "request completions" as "progress changes"
    if (this.filter.requestsTotal > 1 && aRequest)
      return;

    // Filter optimization: Only send non-trivial progress changes to Java listeners
    let percentage = (aCurTotalProgress * 100) / aMaxTotalProgress;
    if (percentage > this.filter.percentage + 3) {
      this.filter.percentage = percentage;

      let message = {
        gecko: {
          type: "onProgressChange",
          tabID: this.id,
          current: aCurTotalProgress,
          total: aMaxTotalProgress
        }
      };
  
      sendMessageToJava(message);
    }
  },

  onStatusChange: function(aBrowser, aWebProgress, aRequest, aStatus, aMessage) {
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener, Ci.nsISupportsWeakReference])
};


var BrowserEventHandler = {
  init: function init() {
    window.addEventListener("click", this, true);
    window.addEventListener("mousedown", this, true);
    window.addEventListener("mouseup", this, true);
    window.addEventListener("mousemove", this, true);

    BrowserApp.deck.addEventListener("MozMagnifyGestureStart", this, true);
    BrowserApp.deck.addEventListener("MozMagnifyGestureUpdate", this, true);
    BrowserApp.deck.addEventListener("DOMContentLoaded", this, true);
    BrowserApp.deck.addEventListener("DOMLinkAdded", this, true);
    BrowserApp.deck.addEventListener("DOMTitleChanged", this, true);
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "DOMContentLoaded": {
        let browser = BrowserApp.getBrowserForDocument(aEvent.target);
        browser.focus();

        let tabID = BrowserApp.getTabForBrowser(browser).id;

        sendMessageToJava({
          gecko: {
            type: "DOMContentLoaded",
            tabID: tabID,
            windowID: 0,
            uri: browser.currentURI.spec,
            title: browser.contentTitle
          }
        });

        // Attach a listener to watch for "click" events bubbling up from error
        // pages and other similar page. This lets us fix bugs like 401575 which
        // require error page UI to do privileged things, without letting error
        // pages have any privilege themselves.
        if (/^about:/.test(aEvent.originalTarget.documentURI)) {
          let browser = BrowserApp.getBrowserForDocument(aEvent.originalTarget);
          browser.addEventListener("click", ErrorPageEventHandler, false);
          browser.addEventListener("pagehide", function () {
            browser.removeEventListener("click", ErrorPageEventHandler, false);
            browser.removeEventListener("pagehide", arguments.callee, true);
          }, true);
        }
        break;
      }

      case "DOMLinkAdded": {
        let target = aEvent.originalTarget;
        if (!target.href || target.disabled)
          return;

        let browser = BrowserApp.getBrowserForDocument(target.ownerDocument); 
        let tabID = BrowserApp.getTabForBrowser(browser).id;

        let json = {
          type: "DOMLinkAdded",
          tabID: tabID,
          windowId: target.ownerDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
          href: resolveGeckoURI(target.href),
          charset: target.ownerDocument.characterSet,
          title: target.title,
          rel: target.rel
        };
        
        // rel=icon can also have a sizes attribute
        if (target.hasAttribute("sizes"))
          json.sizes = target.getAttribute("sizes");

        sendMessageToJava({ gecko: json });
        break;
      }

      case "DOMTitleChanged": {
        if (!aEvent.isTrusted)
          return;

        let contentWin = aEvent.target.defaultView;
        if (contentWin != contentWin.top)
          return;

        let browser = BrowserApp.getBrowserForDocument(aEvent.target);
        if (!browser)
          return;

        let tabID = BrowserApp.getTabForBrowser(browser).id;
        sendMessageToJava({
          gecko: {
            type: "DOMTitleChanged",
            tabID: tabID,
            title: aEvent.target.title
          }
        });
        break;
      }

      case "click":
        if (this.blockClick) {
          aEvent.stopPropagation();
          aEvent.preventDefault();
        }
        break;

      case "mousedown":
        this.startX = aEvent.clientX;
        this.startY = aEvent.clientY;
        this.blockClick = false;

        this.firstMovement = true;
        this.edx = 0;
        this.edy = 0;
        this.lockXaxis = false;
        this.lockYaxis = false;

        this.motionBuffer = [];
        this._updateLastPosition(aEvent.clientX, aEvent.clientY, 0, 0);
        this.panElement = this._findScrollableElement(aEvent.originalTarget,
                                                      true);

        if (this.panElement)
          this.panning = true;
        break;

      case "mousemove":
        aEvent.stopPropagation();
        aEvent.preventDefault();

        if (!this.panning)
          break;

        this.edx += aEvent.clientX - this.lastX;
        this.edy += aEvent.clientY - this.lastY;

        // If this is the first panning motion, check if we can
        // move in the given direction. If we can't, find an
        // ancestor that can.
        // We do this per-axis, as often the very first movement doesn't
        // reflect the direction of movement for both axes.
        if (this.firstMovement &&
            (Math.abs(this.edx) > kDragThreshold ||
             Math.abs(this.edy) > kDragThreshold)) {
          this.firstMovement = false;
          let originalElement = this.panElement;

          // Decide if we want to lock an axis while scrolling
          if (Math.abs(this.edx) > Math.abs(this.edy) * kAxisLockRatio)
            this.lockYaxis = true;
          else if (Math.abs(this.edy) > Math.abs(this.edx) * kAxisLockRatio)
            this.lockXaxis = true;

          // See if we've reached the extents of this element and if so,
          // find an element above it that can be scrolled.
          while (this.panElement &&
                 !this._elementCanScroll(this.panElement,
                                         -this.edx,
                                         -this.edy)) {
            this.panElement =
              this._findScrollableElement(this.panElement, false);
          }

          // If there are no scrollable elements above the element whose
          // extents we've reached, let that element be scrolled.
          if (!this.panElement)
            this.panElement = originalElement;
        }

        // Only scroll once we've moved past the drag threshold
        if (!this.firstMovement) {
          this._scrollElementBy(this.panElement,
                                this.lockXaxis ? 0 : -this.edx,
                                this.lockYaxis ? 0 : -this.edy);

          // Note, it's important that this happens after the scrollElementBy,
          // as this will modify the clientX/clientY to be relative to the
          // correct element.
          this._updateLastPosition(aEvent.clientX, aEvent.clientY,
                                   this.lockXaxis ? 0 : this.edx,
                                   this.lockYaxis ? 0 : this.edy);

          // Allow breaking out of axis lock if we move past a certain threshold
          if (this.lockXaxis) {
            if (Math.abs(this.edx) > kLockBreakThreshold)
              this.lockXaxis = false;
          } else {
            this.edx = 0;
          }

          if (this.lockYaxis) {
            if (Math.abs(this.edy) > kLockBreakThreshold)
              this.lockYaxis = false;
          } else {
            this.edy = 0;
          }
        }
        break;

      case "mouseup":
        if (!this.panning)
          break;

        this.panning = false;

        if (Math.abs(aEvent.clientX - this.startX) > kDragThreshold ||
            Math.abs(aEvent.clientY - this.startY) > kDragThreshold) {
          this.blockClick = true;
          aEvent.stopPropagation();
          aEvent.preventDefault();

          // Calculate a regression line for the last few motion events in
          // the same direction to estimate the velocity. This ought to do a
          // reasonable job of accounting for jitter/bad events.
          this.panLastTime = Date.now();

          // Variables required for calculating regression on each axis
          // 'p' will be sum of positions
          // 't' will be sum of times
          // 'tp' will be sum of times * positions
          // 'tt' will be sum of time^2's
          // 'n' is the number of data-points
          let xSums = { p: 0, t: 0, tp: 0, tt: 0, n: 0 };
          let ySums = { p: 0, t: 0, tp: 0, tt: 0, n: 0 };
          let lastDx = 0;
          let lastDy = 0;

          // Variables to find the absolute x,y (relative to the first event)
          let edx = 0; // Sum of x changes
          let edy = 0; // Sum of y changes

          // For convenience
          let mb = this.motionBuffer;

          // First collect the variables necessary to calculate the line
          for (let i = 0; i < mb.length; i++) {

            // Sum up total movement so far
            let dx = edx + mb[i].dx;
            let dy = edy + mb[i].dy;
            edx += mb[i].dx;
            edy += mb[i].dy;

            // Don't consider events before direction changes
            if ((xSums.n > 0) &&
                ((mb[i].dx < 0 && lastDx > 0) ||
                 (mb[i].dx > 0 && lastDx < 0))) {
              xSums = { p: 0, t: 0, tp: 0, tt: 0, n: 0 };
            }
            if ((ySums.n > 0) &&
                ((mb[i].dy < 0 && lastDy > 0) ||
                 (mb[i].dy > 0 && lastDy < 0))) {
              ySums = { p: 0, t: 0, tp: 0, tt: 0, n: 0 };
            }

            if (mb[i].dx != 0)
              lastDx = mb[i].dx;
            if (mb[i].dy != 0)
              lastDy = mb[i].dy;

            // Only consider events that happened in the last kSwipeLength ms
            let timeDelta = this.panLastTime - mb[i].time;
            if (timeDelta > kSwipeLength)
              continue;

            xSums.p += dx;
            xSums.t += timeDelta;
            xSums.tp += timeDelta * dx;
            xSums.tt += timeDelta * timeDelta;
            xSums.n ++;

            ySums.p += dy;
            ySums.t += timeDelta;
            ySums.tp += timeDelta * dy;
            ySums.tt += timeDelta * timeDelta;
            ySums.n ++;
          }

          // If we don't have enough usable motion events, bail out
          if (xSums.n < 2 && ySums.n < 2)
            break;

          // Calculate the slope of the regression line.
          // The intercept of the regression line is commented for reference.
          let sx = 0;
          if (xSums.n > 1)
            sx = ((xSums.n * xSums.tp) - (xSums.t * xSums.p)) /
                 ((xSums.n * xSums.tt) - (xSums.t * xSums.t));
          //let ix = (xSums.p - (sx * xSums.t)) / xSums.n;

          let sy = 0;
          if (ySums.n > 1)
            sy = ((ySums.n * ySums.tp) - (ySums.t * ySums.p)) /
                 ((ySums.n * ySums.tt) - (ySums.t * ySums.t));
          //let iy = (ySums.p - (sy * ySums.t)) / ySums.n;

          // The slope of the regression line is the projected acceleration
          this.panX = -sx;
          this.panY = -sy;

          if (Math.abs(this.panX) > kMaxKineticSpeed)
            this.panX = (this.panX > 0) ? kMaxKineticSpeed : -kMaxKineticSpeed;
          if (Math.abs(this.panY) > kMaxKineticSpeed)
            this.panY = (this.panY > 0) ? kMaxKineticSpeed : -kMaxKineticSpeed;

          // If we have (near) zero acceleration, bail out.
          if (Math.abs(this.panX) < kMinKineticSpeed &&
              Math.abs(this.panY) < kMinKineticSpeed)
            break;

          // Check if this element can scroll - if not, let's bail out.
          if (!this.panElement || !this._elementCanScroll(
                 this.panElement, -this.panX, -this.panY))
            break;

          // Fire off the first kinetic panning event
          this._scrollElementBy(this.panElement, -this.panX, -this.panY);

          // TODO: Use the kinetic scrolling prefs
          this.panAccumulatedDeltaX = 0;
          this.panAccumulatedDeltaY = 0;

          let self = this;
          let panElement = this.panElement;
          let callback = {
            onBeforePaint: function kineticPanCallback(timeStamp) {
              if (self.panning || self.panElement != panElement)
                return;

              let timeDelta = timeStamp - self.panLastTime;
              self.panLastTime = timeStamp;

              // Adjust deceleration
              self.panX *= Math.pow(kPanDeceleration, timeDelta);
              self.panY *= Math.pow(kPanDeceleration, timeDelta);

              // Calculate panning motion
              let dx = self.panX * timeDelta;
              let dy = self.panY * timeDelta;

              // We only want to set integer scroll coordinates
              self.panAccumulatedDeltaX += dx - Math.floor(dx);
              self.panAccumulatedDeltaY += dy - Math.floor(dy);

              dx = Math.floor(dx);
              dy = Math.floor(dy);

              if (Math.abs(self.panAccumulatedDeltaX) >= 1.0) {
                  let adx = Math.floor(self.panAccumulatedDeltaX);
                  dx += adx;
                  self.panAccumulatedDeltaX -= adx;
              }

              if (Math.abs(self.panAccumulatedDeltaY) >= 1.0) {
                  let ady = Math.floor(self.panAccumulatedDeltaY);
                  dy += ady;
                  self.panAccumulatedDeltaY -= ady;
              }

              self._scrollElementBy(panElement, -dx, -dy);

              if (Math.abs(self.panX) >= kMinKineticSpeed ||
                  Math.abs(self.panY) >= kMinKineticSpeed)
                window.mozRequestAnimationFrame(this);
              }
          };

          // If one axis is moving a lot slower than the other, lock it.
          if (Math.abs(this.panX) < Math.abs(this.panY) / kAxisLockRatio)
            this.panX = 0;
          else if (Math.abs(this.panY) < Math.abs(this.panX) / kAxisLockRatio)
            this.panY = 0;

          // Start the panning animation
          window.mozRequestAnimationFrame(callback);
        }
        break;

      case "MozMagnifyGestureStart":
        this._pinchDelta = 0;
        this.zoomCallbackFired = true;
        break;

      case "MozMagnifyGestureUpdate":
        if (!aEvent.delta)
          break;
  
        this._pinchDelta += aEvent.delta;

        if ((Math.abs(this._pinchDelta) >= 1) && this.zoomCallbackFired) {
          // pinchDelta is the difference in pixels since the last call, so can
          // be viewed as the number of extra/fewer pixels visible.
          //
          // We can work out the new zoom level by looking at the window width
          // and height, and the existing zoom level.
          let currentZoom = BrowserApp.selectedBrowser.markupDocumentViewer.fullZoom;
          let currentSize = Math.sqrt(Math.pow(window.innerWidth, 2) + Math.pow(window.innerHeight, 2));
          let newZoom = ((currentSize * currentZoom) + this._pinchDelta) / currentSize;

          let self = this;
          let callback = {
            onBeforePaint: function zoomCallback(timeStamp) {
              BrowserApp.selectedBrowser.markupDocumentViewer.fullZoom = newZoom;
              self.zoomCallbackFired = true;
            }
          };

          this._pinchDelta = 0;

          // Use mozRequestAnimationFrame to stop from flooding fullZoom
          this.zoomCallbackFired = false;
          window.mozRequestAnimationFrame(callback);
        }
        break;
    }
  },

  _updateLastPosition: function(x, y, dx, dy) {
    this.lastX = x;
    this.lastY = y;
    this.lastTime = Date.now();

    this.motionBuffer.push({ dx: dx, dy: dy, time: this.lastTime });
  },

  _findScrollableElement: function(elem, checkElem) {
    // Walk the DOM tree until we find a scrollable element
    let scrollable = false;
    while (elem) {
      /* Element is scrollable if its scroll-size exceeds its client size, and:
       * - It has overflow 'auto' or 'scroll'
       * - It's a textarea
       * - It's an HTML/BODY node
       */
      if (checkElem) {
        if (((elem.scrollHeight > elem.clientHeight) ||
             (elem.scrollWidth > elem.clientWidth)) &&
            (elem.style.overflow == 'auto' ||
             elem.style.overflow == 'scroll' ||
             elem.localName == 'textarea' ||
             elem.localName == 'html' ||
             elem.localName == 'body')) {
          scrollable = true;
          break;
        }
      } else {
        checkElem = true;
      }

      // Propagate up iFrames
      if (!elem.parentNode && elem.documentElement &&
          elem.documentElement.ownerDocument)
        elem = elem.documentElement.ownerDocument.defaultView.frameElement;
      else
        elem = elem.parentNode;
    }

    if (!scrollable)
      return null;

    return elem;
  },

  _scrollElementBy: function(elem, x, y) {
    elem.scrollTop = elem.scrollTop + y;
    elem.scrollLeft = elem.scrollLeft + x;
  },

  _elementCanScroll: function(elem, x, y) {
    let scrollX = true;
    let scrollY = true;

    if (x < 0) {
      if (elem.scrollLeft <= 0) {
        scrollX = false;
      }
    } else if (elem.scrollLeft >= (elem.scrollWidth - elem.clientWidth)) {
      scrollX = false;
    }

    if (y < 0) {
      if (elem.scrollTop <= 0) {
        scrollY = false;
      }
    } else if (elem.scrollTop >= (elem.scrollHeight - elem.clientHeight)) {
      scrollY = false;
    }

    return scrollX || scrollY;
  }
};


var ErrorPageEventHandler = {
  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "click": {
        // Don't trust synthetic events
        if (!aEvent.isTrusted)
          return;

        let target = aEvent.originalTarget;
        let errorDoc = target.ownerDocument;

        // If the event came from an ssl error page, it is probably either the "Add
        // Exception…" or "Get me out of here!" button
        if (/^about:certerror\?e=nssBadCert/.test(errorDoc.documentURI)) {
          let perm = errorDoc.getElementById("permanentExceptionButton");
          let temp = errorDoc.getElementById("temporaryExceptionButton");
          if (target == temp || target == perm) {
            // Handle setting an cert exception and reloading the page
            try {
              // Add a new SSL exception for this URL
              let uri = Services.io.newURI(errorDoc.location.href, null, null);
              let sslExceptions = new SSLExceptions();
      
              if (target == perm)
                sslExceptions.addPermanentException(uri);
              else
                sslExceptions.addTemporaryException(uri);
            } catch (e) {
              dump("Failed to set cert exception: " + e + "\n");
            }
            errorDoc.location.reload();
          } else if (target == errorDoc.getElementById("getMeOutOfHereButton")) {
            errorDoc.location = this.getFallbackSafeURL();
          }
        }
        break;
      }
    }
  },

  getFallbackSafeURL: function getFallbackSafeURL() {
    // Get the start page from the *default* pref branch, not the user's
    let prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefService).getDefaultBranch(null);
    let url = "about:home";
    try {
      url = prefs.getComplexValue("browser.startup.homepage", Ci.nsIPrefLocalizedString).data;
      // If url is a pipe-delimited set of pages, just take the first one.
      if (url.indexOf("|") != -1)
        url = url.split("|")[0];
    } catch(e) {
      Cu.reportError("Couldn't get homepage pref: " + e);
    }
    return url;
  }
};

var XPInstallObserver = {
  observe: function xpi_observer(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "addon-install-started":
        NativeWindow.toast.show(Strings.browser.GetStringFromName("alertAddonsDownloading"), "short");
        break;
      case "addon-install-blocked":
        dump("XPInstallObserver addon-install-blocked");
        let installInfo = aSubject.QueryInterface(Ci.amIWebInstallInfo);
        let host = installInfo.originatingURI.host;

        let brandShortName = Strings.brand.GetStringFromName("brandShortName");
        let notificationName, buttons, messageString;
        let strings = Strings.browser;
        let enabled = true;
        try {
          enabled = Services.prefs.getBoolPref("xpinstall.enabled");
        }
        catch (e) {}

        if (!enabled) {
          notificationName = "xpinstall-disabled";
          if (Services.prefs.prefIsLocked("xpinstall.enabled")) {
            messageString = strings.GetStringFromName("xpinstallDisabledMessageLocked");
            buttons = [];
          } else {
            messageString = strings.formatStringFromName("xpinstallDisabledMessage2", [brandShortName, host], 2);
            buttons = [{
              label: strings.GetStringFromName("xpinstallDisabledButton"),
              callback: function editPrefs() {
                Services.prefs.setBoolPref("xpinstall.enabled", true);
                return false;
              }
            }];
          }
        } else {
          notificationName = "xpinstall";
          messageString = strings.formatStringFromName("xpinstallPromptWarning2", [brandShortName, host], 2);

          buttons = [{
            label: strings.GetStringFromName("xpinstallPromptAllowButton"),
            callback: function() {
              // Kick off the install
              installInfo.install();
              return false;
            }
          }];
        }
        NativeWindow.doorhanger.show(messageString, buttons);
        break;
    }
  }
};
