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
"use strict";

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

// The element tag names that are considered to receive input. Mouse-down
// events directed to one of these are allowed to go through.
const kElementsReceivingInput = {
    applet: true,
    audio: true,
    button: true,
    embed: true,
    input: true,
    map: true,
    select: true,
    textarea: true,
    video: true
};

function dump(a) {
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(a);
}

function sendMessageToJava(aMessage) {
  let bridge = Cc["@mozilla.org/android/bridge;1"].getService(Ci.nsIAndroidBridge);
  return bridge.handleGeckoMessage(JSON.stringify(aMessage));
}

#ifdef MOZ_CRASHREPORTER
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "CrashReporter",
  "@mozilla.org/xre/app-info;1", "nsICrashReporter");
#endif

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
  vertScroller: null,
  horizScroller: null,

  startup: function startup() {
    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = new nsBrowserAccess();
    dump("zerdatime " + Date.now() + " - browser chrome startup finished.");

    this.deck = document.getElementById("browsers");
    this.vertScroller = document.getElementById("vertical-scroller");
    this.horizScroller = document.getElementById("horizontal-scroller");
    BrowserEventHandler.init();

    Services.obs.addObserver(this, "Tab:Add", false);
    Services.obs.addObserver(this, "Tab:Load", false);
    Services.obs.addObserver(this, "Tab:Select", false);
    Services.obs.addObserver(this, "Tab:Close", false);
    Services.obs.addObserver(this, "Session:Back", false);
    Services.obs.addObserver(this, "Session:Forward", false);
    Services.obs.addObserver(this, "Session:Reload", false);
    Services.obs.addObserver(this, "SaveAs:PDF", false);
    Services.obs.addObserver(this, "Browser:Quit", false);
    Services.obs.addObserver(this, "Preferences:Get", false);
    Services.obs.addObserver(this, "Preferences:Set", false);
    Services.obs.addObserver(this, "ScrollTo:FocusedInput", false);
    Services.obs.addObserver(this, "Sanitize:ClearAll", false);
    Services.obs.addObserver(this, "PanZoom:PanZoom", false);
    Services.obs.addObserver(this, "FullScreen:Exit", false);

    Services.obs.addObserver(XPInstallObserver, "addon-install-blocked", false);
    Services.obs.addObserver(XPInstallObserver, "addon-install-started", false);

    function showFullScreenWarning() {
      NativeWindow.toast.show(Strings.browser.GetStringFromName("alertFullScreenToast"), "short");
    }

    window.addEventListener("fullscreen", function() {
      sendMessageToJava({
        gecko: {
          type: window.fullScreen ? "ToggleChrome:Show" : "ToggleChrome:Hide"
        }       
      });

      if (!window.fullScreen)
        showFullScreenWarning();
    }, false);

    // When a restricted key is pressed in DOM full-screen mode, we should display
    // the "Press ESC to exit" warning message.
    window.addEventListener("MozShowFullScreenWarning", showFullScreenWarning, true);

    NativeWindow.init();
    Downloads.init();
    OfflineApps.init();
    IndexedDB.init();

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

    let telemetryPrompted = false;
    try {
      telemetryPrompted = Services.prefs.getBoolPref("toolkit.telemetry.prompted");
    } catch (e) {
      // optional
    }

    if (!telemetryPrompted) {
      let buttons = [
        {
          label: Strings.browser.GetStringFromName("telemetry.optin.yes"),
          callback: function () { 
            Services.prefs.setBoolPref("toolkit.telemetry.prompted", true);
            Services.prefs.setBoolPref("toolkit.telemetry.enabled", true);
          }
        },
        {
          label: Strings.browser.GetStringFromName("telemetry.optin.no"),
          callback: function () { 
            Services.prefs.setBoolPref("toolkit.telemetry.prompted", true);
            Services.prefs.setBoolPref("toolkit.telemetry.enabled", false);
          }
        }
      ];
      let brandShortName = Strings.brand.GetStringFromName("brandShortName");
      let message = Strings.browser.formatStringFromName("telemetry.optin.message", [brandShortName], 1);
      NativeWindow.doorhanger.show(message, "telemetry-optin", buttons);
    }

  },

  shutdown: function shutdown() {
    NativeWindow.uninit();
    OfflineApps.uninit();
    IndexedDB.uninit();

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

  addTab: function addTab(aURI, aParams) {
    let newTab = new Tab(aURI, aParams);
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

  quit: function quit() {
      window.QueryInterface(Ci.nsIDOMChromeWindow).minimize();
      window.close();
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
    file.createUnique(file.NORMAL_FILE_TYPE, parseInt("666", 8));
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

  panZoom: function(aData) {
    let data = JSON.parse(aData);
    let browser = this.selectedBrowser;
    browser.contentWindow.scrollTo(data.x, data.y);

    /* TODO (bug 695449): Scale. */

    sendMessageToJava({ gecko: { type: "PanZoom:Ack", rect: data } });
  },

  updateScrollbarsFor: function(aElement) {
    // only draw the scrollbars if we're scrolling the root content element
    let doc = this.selectedBrowser.contentDocument;
    if (aElement != doc.documentElement && aElement != doc.body)
      return;

    // draw the vertical scrollbar as needed
    let scrollMax = aElement.scrollHeight;
    let viewSize = aElement.clientHeight;
    if (scrollMax > viewSize) {
      let scrollPos = aElement.scrollTop;
      let scrollerSize = this.selectedBrowser.clientHeight;
      // scrollerSize may not equal viewSize if the user has zoomed
      let barStart = Math.round(scrollerSize * scrollPos / scrollMax);
      let barEnd = Math.round(scrollerSize * (scrollPos + viewSize) / scrollMax);
      this.vertScroller.height = (barEnd - barStart);
      this.vertScroller.style.MozTransform = "translateY(" + barStart + "px)";
      this.vertScroller.setAttribute("panning", "true");
    }

    // draw the horizontal scrollbar as needed
    scrollMax = aElement.scrollWidth;
    viewSize = aElement.clientWidth;
    if (scrollMax > viewSize) {
      let scrollPos = aElement.scrollLeft;
      let scrollerSize = this.selectedBrowser.clientWidth;
      // scrollerSize may not equal viewSize if the user has zoomed
      let barStart = Math.round(scrollerSize * scrollPos / scrollMax);
      let barEnd = Math.round(scrollerSize * (scrollPos + viewSize) / scrollMax);
      this.horizScroller.width = (barEnd - barStart);
      this.horizScroller.style.MozTransform = "translateX(" + barStart + "px)";
      this.horizScroller.setAttribute("panning", "true");
    }
  },

  hideScrollbars: function() {
    this.vertScroller.setAttribute("panning", "");
    this.horizScroller.setAttribute("panning", "");
  },

  observe: function(aSubject, aTopic, aData) {
    let browser = this.selectedBrowser;
    if (!browser)
      return;

    if (aTopic == "Session:Back") {
      browser.goBack();
    } else if (aTopic == "Session:Forward") {
      browser.goForward();
    } else if (aTopic == "Session:Reload") {
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
    } else if (aTopic == "Browser:Quit") {
      this.quit();
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
    } else if (aTopic == "PanZoom:PanZoom") {
      this.panZoom(aData);
    } else if (aTopic == "FullScreen:Exit") {
      browser.contentDocument.mozCancelFullScreen();
    }
  }
}

var NativeWindow = {
  init: function() {
    Services.obs.addObserver(this, "Menu:Clicked", false);
    Services.obs.addObserver(this, "Doorhanger:Reply", false);
    this.contextmenus.init();
  },

  uninit: function() {
    Services.obs.removeObserver(this, "Menu:Clicked");
    Services.obs.removeObserver(this, "Doorhanger:Reply");
    this.contextmenus.uninit();
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
    _callbacks: {},
    _callbacksId: 0,
    _promptId: 0,

  /**
   * @param aOptions
   *        An options JavaScript object holding additional properties for the
   *        notification. The following properties are currently supported:
   *        persistence: An integer. The notification will not automatically
   *                     dismiss for this many page loads. If persistence is set
   *                     to -1, the doorhanger will never automatically dismiss.
   *        timeout:     A time in milliseconds. The notification will not
   *                     automatically dismiss before this time.
   */
    show: function(aMessage, aValue, aButtons, aTabID, aOptions) {
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
          value: aValue,
          buttons: aButtons,
          // use the current tab if none is provided
          tabID: aTabID || BrowserApp.selectedTab.id,
          options: aOptions || {}
        }
      };
      sendMessageToJava(json);
    },

    hide: function(aValue, aTabID) {
      sendMessageToJava({
        type: "Doorhanger:Remove",
        value: aValue,
        tabID: aTabID
      });
    }
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "Menu:Clicked") {
      if (this.menu._callbacks[aData])
        this.menu._callbacks[aData]();
    } else if (aTopic == "Doorhanger:Reply") {
      let reply_id = aData;
      if (this.doorhanger._callbacks[reply_id]) {
        let prompt = this.doorhanger._callbacks[reply_id].prompt;
        this.doorhanger._callbacks[reply_id].cb();
        for (let id in this.doorhanger._callbacks) {
          if (this.doorhanger._callbacks[id].prompt == prompt) {
            delete this.doorhanger._callbacks[id];
          }
        }
      }
    }
  },
  contextmenus: {
    items: {}, //  a list of context menu items that we may show
    textContext: null, // saved selector for text input areas
    linkContext: null, // saved selector for links
    _contextId: 0, // id to assign to new context menu items if they are added

    init: function() {
      this.textContext = this.SelectorContext("input[type='text'],input[type='password'],textarea");
      this.linkContext = this.SelectorContext("a:not([href='']),area:not([href='']),link");
      Services.obs.addObserver(this, "Gesture:LongPress", false);

      // TODO: These should eventually move into more appropriate classes
      this.add(Strings.browser.GetStringFromName("contextmenu.openInNewTab"),
               this.linkContext,
               function(aTarget) {
                 let url = NativeWindow.contextmenus._getLinkURL(aTarget);
                 BrowserApp.addTab(url, {selected: false});
               });
  
      this.add(Strings.browser.GetStringFromName("contextmenu.changeInputMethod"),
               this.textContext,
               function(aTarget) {
                 Cc["@mozilla.org/imepicker;1"].getService(Ci.nsIIMEPicker).show();
               });

      this.add(Strings.browser.GetStringFromName("contextmenu.fullScreen"),
               this.SelectorContext("video:not(:-moz-full-screen)"),
               function(aTarget) {
                 aTarget.mozRequestFullScreen();
               });
    },

    uninit: function() {
      Services.obs.removeObserver(this, "Gesture:LongPress");
    },

    add: function(aName, aSelector, aCallback) {
      if (!aName)
        throw "Menu items must have a name";
  
      let item = {
        name: aName,
        context: aSelector,
        callback: aCallback,
        matches: function(aElt) {
          return this.context.matches(aElt);
        },
        getValue: function() {
          return {
            label: this.name,
            id: this.id
          }
        }
      };
      item.id = this._contextId++;
      this.items[item.id] = item;
      return item.id;
    },
  
    remove: function(aId) {
      this.items[aId] = null;
    },
  
    SelectorContext: function(aSelector) {
      return {
        matches: function(aElt) {
          if (aElt.mozMatchesSelector)
            return aElt.mozMatchesSelector(aSelector);
          return false;
        }
      }
    },
  
    _sendToContent: function(aX, aY) {
      // initially we look for nearby clickable elements. If we don't find one we fall back to using whatever this click was on
      let rootElement = ElementTouchHelper.elementFromPoint(BrowserApp.selectedBrowser.contentWindow, aX, aY);
      if (!rootElement)
        rootElement = ElementTouchHelper.anyElementFromPoint(BrowserApp.selectedBrowser.contentWindow, aX, aY)
  
      this.menuitems = null;
      let element = rootElement;
      if (!element)
        return;
  
      while (element) {
        for each (let item in this.items) {
          // since we'll have to spin through this for each element, check that
          // it is not already in the list
          if ((!this.menuitems || !this.menuitems[item.id]) && item.matches(element)) {
            if (!this.menuitems)
              this.menuitems = {};
            this.menuitems[item.id] = item;
          }
        }

        if (this.linkContext.matches(element) || this.textContext.matches(element))
          break;
        element = element.parentNode;
      }

      // only send the contextmenu event to content if we are planning to show a context menu (i.e. not on every long tap)
      if (this.menuitems) {
        BrowserEventHandler.blockClick = true;
        let event = rootElement.ownerDocument.createEvent("MouseEvent");
        event.initMouseEvent("contextmenu", true, true, content,
                             0, aX, aY, aX, aY, false, false, false, false,
                             0, null);
        rootElement.ownerDocument.defaultView.addEventListener("contextmenu", this, false);
        rootElement.dispatchEvent(event);
      }
    },
  
    _show: function(aEvent) {
      if (aEvent.getPreventDefault())
        return;
  
      let popupNode = aEvent.originalTarget;
      let title = "";
      if ((popupNode instanceof Ci.nsIDOMHTMLAnchorElement && popupNode.href) ||
              (popupNode instanceof Ci.nsIDOMHTMLAreaElement && popupNode.href)) {
        title = this._getLinkURL(popupNode);
      } else if (popupNode instanceof Ci.nsIImageLoadingContent && popupNode.currentURI) {
        title = popupNode.currentURI.spec;
      } else if (popupNode instanceof Ci.nsIDOMHTMLMediaElement) {
        title = (popupNode.currentSrc || popupNode.src);
      }

      // convert this.menuitems object to an array for sending to native code
      let itemArray = [];
      for each (let item in this.menuitems) {
        itemArray.push(item.getValue());
      }

      let msg = {
        gecko: {
          type: "Prompt:Show",
          title: title,
          listitems: itemArray
        }
      };
      let data = JSON.parse(sendMessageToJava(msg));
      let selectedId = itemArray[data.button].id;
      let selectedItem = this.menuitems[selectedId];
  
      if (selectedItem && selectedItem.callback) {
        while (popupNode) {
          if (selectedItem.matches(popupNode)) {
            selectedItem.callback.call(selectedItem, popupNode);
            break;
          }
          popupNode = popupNode.parentNode;
        }
      }
      this.menuitems = null;
    },
  
    handleEvent: function(aEvent) {
      aEvent.target.ownerDocument.defaultView.removeEventListener("contextmenu", this, false);
      this._show(aEvent);
    },
  
    observe: function(aSubject, aTopic, aData) {
      let data = JSON.parse(aData);
      // content gets first crack at cancelling context menus
      this._sendToContent(data.x, data.y);
    },
  
    // XXX - These are stolen from Util.js, we should remove them if we bring it back
    makeURLAbsolute: function makeURLAbsolute(base, url) {
      // Note:  makeURI() will throw if url is not a valid URI
      return this.makeURI(url, null, this.makeURI(base)).spec;
    },
  
    makeURI: function makeURI(aURL, aOriginCharset, aBaseURI) {
      return Services.io.newURI(aURL, aOriginCharset, aBaseURI);
    },
  
    _getLinkURL: function ch_getLinkURL(aLink) {
      let href = aLink.href;
      if (href)
        return href;
  
      href = aLink.getAttributeNS(kXLinkNamespace, "href");
      if (!href || !href.match(/\S/)) {
        // Without this we try to save as the current doc,
        // for example, HTML case also throws if empty
        throw "Empty href";
      }
  
      return Util.makeURLAbsolute(aLink.baseURI, href);
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

function Tab(aURL, aParams) {
  this.browser = null;
  this.id = 0;
  this.create(aURL, aParams);
}

Tab.prototype = {
  create: function(aURL, aParams) {
    if (this.browser)
      return;

    this.browser = document.createElement("browser");
    this.browser.setAttribute("type", "content");
    this.browser.setAttribute("width", "980");
    this.browser.setAttribute("height", "480");
    BrowserApp.deck.appendChild(this.browser);

    let frameLoader = this.browser.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader;
    frameLoader.clipSubdocument = false;

    this.browser.stop();

    this.id = ++gTabIDFactory;
    let aParams = aParams || { selected: true };
    let message = {
      gecko: {
        type: "Tab:Added",
        tabID: this.id,
        uri: aURL,
        selected: ("selected" in aParams) ? aParams.selected : true
      }
    };
    sendMessageToJava(message);

    let flags = Ci.nsIWebProgress.NOTIFY_STATE_ALL |
                Ci.nsIWebProgress.NOTIFY_LOCATION |
                Ci.nsIWebProgress.NOTIFY_SECURITY;
    this.browser.addProgressListener(this, flags);
    this.browser.sessionHistory.addSHistoryListener(this);
    this.browser.loadURI(aURL);
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
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT) {
      // Filter optimization: Only really send DOCUMENT state changes to Java listener
      let browser = BrowserApp.getBrowserForWindow(aWebProgress.DOMWindow);
      let uri = "";
      if (browser)
        uri = browser.currentURI.spec;
  
      let message = {
        gecko: {
          type: "Content:StateChange",
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
        type: "Content:LocationChange",
        tabID: this.id,
        uri: uri
      }
    };

    sendMessageToJava(message);
  },

  onSecurityChange: function(aWebProgress, aRequest, aState) {
    let mode = "unknown";
    if (aState & Ci.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL)
      mode = "identified";
    else if (aState & Ci.nsIWebProgressListener.STATE_SECURE_HIGH)
      mode = "verified";
    else if (aState & Ci.nsIWebProgressListener.STATE_IS_BROKEN)
      mode = "mixed";
    else
      mode = "unknown";

   let message = {
      gecko: {
        type: "Content:SecurityChange",
        tabID: this.id,
        mode: mode
      }
    };

     sendMessageToJava(message);
  },

  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {
  },

  onStatusChange: function(aBrowser, aWebProgress, aRequest, aStatus, aMessage) {
  },

  _sendHistoryEvent: function(aMessage, aIndex, aUri) {
    let message = {
      gecko: {
        type: "SessionHistory:" + aMessage,
        tabID: this.id,
      }
    };
    if (aIndex != -1) {
      message.gecko.index = aIndex;
    }
    if (aUri != null) {
      message.gecko.uri = aUri;
    }
    sendMessageToJava(message);
  },

  OnHistoryNewEntry: function(aUri) {
    this._sendHistoryEvent("New", -1, aUri.spec);
  },

  OnHistoryGoBack: function(aUri) {
    this._sendHistoryEvent("Back", -1, null);
    return true;
  },

  OnHistoryGoForward: function(aUri) {
    this._sendHistoryEvent("Forward", -1, null);
    return true;
  },

  OnHistoryReload: function(aUri, aFlags) {
    // we don't do anything with this, so don't propagate it
    // for now anyway
    return true;
  },

  OnHistoryGotoIndex: function(aIndex, aUri) {
    this._sendHistoryEvent("Goto", aIndex, null);
    return true;
  },

  OnHistoryPurge: function(aNumEntries) {
    this._sendHistoryEvent("Purge", -1, null);
    return true;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener, Ci.nsISHistoryListener, Ci.nsISupportsWeakReference])
};


var BrowserEventHandler = {
  init: function init() {
    window.addEventListener("click", this, true);
    window.addEventListener("mousedown", this, true);
    window.addEventListener("mouseup", this, true);
    window.addEventListener("mousemove", this, true);

    BrowserApp.deck.addEventListener("DOMContentLoaded", this, true);
    BrowserApp.deck.addEventListener("DOMLinkAdded", this, true);
    BrowserApp.deck.addEventListener("DOMTitleChanged", this, true);
    BrowserApp.deck.addEventListener("DOMUpdatePageReport", PopupBlockerObserver.onUpdatePageReport, false);
    BrowserApp.deck.addEventListener("MozScrolledAreaChanged", this, true);
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "DOMContentLoaded": {
        let browser = BrowserApp.getBrowserForDocument(aEvent.target);
        if (!browser)
          return;
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
          browser.addEventListener("pagehide", function listener() {
            browser.removeEventListener("click", ErrorPageEventHandler, false);
            browser.removeEventListener("pagehide", listener, true);
          }, true);
        }

        break;
      }

      case "DOMLinkAdded": {
        let target = aEvent.originalTarget;
        if (!target.href || target.disabled)
          return;

        let browser = BrowserApp.getBrowserForDocument(target.ownerDocument);
        if (!browser)
          return;
        let tabID = BrowserApp.getTabForBrowser(browser).id;

        let json = {
          type: "DOMLinkAdded",
          tabID: tabID,
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
        } else {
          let closest = ElementTouchHelper.elementFromPoint(BrowserApp.selectedBrowser.contentWindow, aEvent.clientX, aEvent.clientY);
          if (closest) {
            aEvent.stopPropagation();
            aEvent.preventDefault();
            this._sendMouseEvent("mousedown", closest, aEvent.clientX, aEvent.clientY);
            this._sendMouseEvent("mouseup", closest, aEvent.clientX, aEvent.clientY);
          } else {
            FormAssistant.handleClick(aEvent);
          }
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
        if (!this._elementReceivesInput(aEvent.target))
          aEvent.preventDefault();  // Stops selection.
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
        this.panning = false;

        // hide the scrollbars in case we're done scrolling. if the
        // kinetic scrolling kicks in, it will re-enable the scrollbars
        // anyway by calling _scrollElementBy below
        BrowserApp.hideScrollbars();

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

              if (!self._elementCanScroll(panElement, -dx, -dy)) {
                BrowserApp.hideScrollbars();
                return;
              }

              self._scrollElementBy(panElement, -dx, -dy);

              if (Math.abs(self.panX) >= kMinKineticSpeed ||
                  Math.abs(self.panY) >= kMinKineticSpeed)
                window.mozRequestAnimationFrame(this);
              else
                BrowserApp.hideScrollbars();
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

      case "MozScrolledAreaChanged":
        let browser = BrowserApp.getBrowserForDocument(aEvent.target);
        if (browser != BrowserApp.selectedBrowser)
          return;

        sendMessageToJava({
          gecko: {
            type: "PanZoom:Resize",
            size: { width: aEvent.width, height: aEvent.height }
          }
        });
        break;
    }
  },

  _updateLastPosition: function(x, y, dx, dy) {
    this.lastX = x;
    this.lastY = y;
    this.lastTime = Date.now();

    this.motionBuffer.push({ dx: dx, dy: dy, time: this.lastTime });
  },

  _sendMouseEvent: function _sendMouseEvent(aName, aElement, aX, aY, aButton) {
    // the element can be out of the aX/aY point because of the touch radius
    // if outside, we gracefully move the touch point to the center of the element
    if (!(aElement instanceof HTMLHtmlElement)) {
      let isTouchClick = true;
      let rects = ElementTouchHelper.getContentClientRects(aElement);
      for (let i = 0; i < rects.length; i++) {
        let rect = rects[i];
        // We might be able to deal with fractional pixels, but mouse events won't.
        // Deflate the bounds in by 1 pixel to deal with any fractional scroll offset issues.
        let inBounds = 
          (aX > rect.left + 1 && aX < (rect.left + rect.width - 1)) &&
          (aY > rect.top + 1 && aY < (rect.top + rect.height - 1));
        if (inBounds) {
          isTouchClick = false;
          break;
        }
      }

      if (isTouchClick) {
        let rect = {x: rects[0].left, y: rects[0].top, w: rects[0].width, h: rects[0].height};
        if (rect.w == 0 && rect.h == 0)
          return;

        let point = { x: rect.x + rect.w/2, y: rect.y + rect.h/2 };
        aX = point.x;
        aY = point.y;
      }
    }

    let cwu = aElement.ownerDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    aButton = aButton || 0;
    cwu.sendMouseEventToWindow(aName, Math.round(aX), Math.round(aY), aButton, 1, 0, true);
  },

  _findScrollableElement: function(elem, checkElem) {
    // Walk the DOM tree until we find a scrollable element
    let scrollable = false;
    while (elem) {
      /* Element is scrollable if its scroll-size exceeds its client size, and:
       * - It has overflow 'auto' or 'scroll'
       * - It's a textarea
       * We don't consider HTML/BODY nodes here, since Java pans those.
       */
      if (checkElem) {
        if (((elem.scrollHeight > elem.clientHeight) ||
             (elem.scrollWidth > elem.clientWidth)) &&
            (elem.style.overflow == 'auto' ||
             elem.style.overflow == 'scroll' ||
             elem.localName == 'textarea')) {
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

  _elementReceivesInput: function(aElement) {
    return aElement instanceof Element &&
        (kElementsReceivingInput.hasOwnProperty(aElement.tagName.toLowerCase()) ||
        aElement.contentEditable === "true" || aElement.contentEditable === "");
  },

  _scrollElementBy: function(elem, x, y) {
    elem.scrollTop = elem.scrollTop + y;
    elem.scrollLeft = elem.scrollLeft + x;
    BrowserApp.updateScrollbarsFor(elem);
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

const kReferenceDpi = 240; // standard "pixel" size used in some preferences

const ElementTouchHelper = {
  anyElementFromPoint: function(aWindow, aX, aY) {
    let cwu = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    let elem = cwu.elementFromPoint(aX, aY, false, true);
  
    while (elem && (elem instanceof HTMLIFrameElement || elem instanceof HTMLFrameElement)) {
      let rect = elem.getBoundingClientRect();
      aX -= rect.left;
      aY -= rect.top;
      cwu = elem.contentDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      elem = cwu.elementFromPoint(aX, aY, false, true);
    }
  
    return elem;
  },

  elementFromPoint: function(aWindow, aX, aY) {
    // browser's elementFromPoint expect browser-relative client coordinates.
    // subtract browser's scroll values to adjust
    let cwu = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    let elem = this.getClosest(cwu, aX, aY);

    // step through layers of IFRAMEs and FRAMES to find innermost element
    while (elem && (elem instanceof HTMLIFrameElement || elem instanceof HTMLFrameElement)) {
      // adjust client coordinates' origin to be top left of iframe viewport
      let rect = elem.getBoundingClientRect();
      aX -= rect.left;
      aY -= rect.top;
      cwu = elem.contentDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      elem = ElementTouchHelper.getClosest(cwu, aX, aY);
    }
  
    return elem;
  },

  get radius() {
    let prefs = Services.prefs;
    delete this.radius;
    return this.radius = { "top": prefs.getIntPref("browser.ui.touch.top"),
                           "right": prefs.getIntPref("browser.ui.touch.right"),
                           "bottom": prefs.getIntPref("browser.ui.touch.bottom"),
                           "left": prefs.getIntPref("browser.ui.touch.left")
                         };
  },

  get weight() {
    delete this.weight;
    return this.weight = { "visited": Services.prefs.getIntPref("browser.ui.touch.weight.visited") };
  },

  /* Retrieve the closest element to a point by looking at borders position */
  getClosest: function getClosest(aWindowUtils, aX, aY) {
    if (!this.dpiRatio)
      this.dpiRatio = aWindowUtils.displayDPI / kReferenceDpi;

    let dpiRatio = this.dpiRatio;

    let target = aWindowUtils.elementFromPoint(aX, aY,
                                               true,   /* ignore root scroll frame*/
                                               false); /* don't flush layout */

    // return null if the click is over a clickable element
    if (this._isElementClickable(target))
      return null;
    let target = null;
    let nodes = aWindowUtils.nodesFromRect(aX, aY, this.radius.top * dpiRatio,
                                                   this.radius.right * dpiRatio,
                                                   this.radius.bottom * dpiRatio,
                                                   this.radius.left * dpiRatio, true, false);

    let threshold = Number.POSITIVE_INFINITY;
    for (let i = 0; i < nodes.length; i++) {
      let current = nodes[i];
      if (!current.mozMatchesSelector || !this._isElementClickable(current))
        continue;

      let rect = current.getBoundingClientRect();
      let distance = this._computeDistanceFromRect(aX, aY, rect);

      // increase a little bit the weight for already visited items
      if (current && current.mozMatchesSelector("*:visited"))
        distance *= (this.weight.visited / 100);

      if (distance < threshold) {
        target = current;
        threshold = distance;
      }
    }

    return target;
  },

  _isElementClickable: function _isElementClickable(aElement) {
    const selector = "a,:link,:visited,[role=button],button,input,select,textarea,label";
    for (let elem = aElement; elem; elem = elem.parentNode) {
      if (this._hasMouseListener(elem))
        return true;
      if (elem.mozMatchesSelector && elem.mozMatchesSelector(selector))
        return true;
    }
    return false;
  },

  _computeDistanceFromRect: function _computeDistanceFromRect(aX, aY, aRect) {
    let x = 0, y = 0;
    let xmost = aRect.left + aRect.width;
    let ymost = aRect.top + aRect.height;

    // compute horizontal distance from left/right border depending if X is
    // before/inside/after the element's rectangle
    if (aRect.left < aX && aX < xmost)
      x = Math.min(xmost - aX, aX - aRect.left);
    else if (aX < aRect.left)
      x = aRect.left - aX;
    else if (aX > xmost)
      x = aX - xmost;

    // compute vertical distance from top/bottom border depending if Y is
    // above/inside/below the element's rectangle
    if (aRect.top < aY && aY < ymost)
      y = Math.min(ymost - aY, aY - aRect.top);
    else if (aY < aRect.top)
      y = aRect.top - aY;
    if (aY > ymost)
      y = aY - ymost;

    return Math.sqrt(Math.pow(x, 2) + Math.pow(y, 2));
  },

  _els: Cc["@mozilla.org/eventlistenerservice;1"].getService(Ci.nsIEventListenerService),
  _clickableEvents: ["mousedown", "mouseup", "click"],
  _hasMouseListener: function _hasMouseListener(aElement) {
    let els = this._els;
    let listeners = els.getListenerInfoFor(aElement, {});
    for (let i = 0; i < listeners.length; i++) {
      if (this._clickableEvents.indexOf(listeners[i].type) != -1)
        return true;
    }
    return false;
  },
  getContentClientRects: function(aElement) {
    let offset = {x: 0, y: 0};
  
    let nativeRects = aElement.getClientRects();
    // step out of iframes and frames, offsetting scroll values
    for (let frame = aElement.ownerDocument.defaultView; frame != content; frame = frame.parent) {
      // adjust client coordinates' origin to be top left of iframe viewport
      let rect = frame.frameElement.getBoundingClientRect();
      let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
      let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
      offset.x += rect.left + parseInt(left);
      offset.y += rect.top + parseInt(top);
    }
  
    let result = [];
    for (let i = nativeRects.length - 1; i >= 0; i--) {
      let r = nativeRects[i];
      result.push({ left: r.left + offset.x,
                    top: r.top + offset.y,
                    width: r.width,
                    height: r.height
                  });
    }
    return result;
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


var FormAssistant = {
  show: function(aList, aElement) {
    let data = JSON.parse(sendMessageToJava({ gecko: aList }));
    let selected = data.button;
    if (!(selected instanceof Array)) {
      let temp = [];
      for (let i = 0;  i < aList.listitems.length; i++) {
        temp[i] = (i == selected);
      }
      selected = temp;
    }
    this.forOptions(aElement, function(aNode, aIndex) {
      aNode.selected = selected[aIndex];
    });
    this.fireOnChange(aElement);
  },

  handleClick: function(aEvent) {
    let target = aEvent.target;
    while (target) {
      if (this._isSelectElement(target)) {
        let list = this.getListForElement(target);
        this.show(list, target);
        target = null;
        return true;
      }
      if (target)
        target = target.parentNode;
    }
    return false;
  },

  fireOnChange: function(aElement) {
    let evt = aElement.ownerDocument.createEvent("Events");
    evt.initEvent("change", true, true, aElement.defaultView, 0,
                  false, false,
                  false, false, null);
    setTimeout(function() {
      aElement.dispatchEvent(evt);
    }, 0);
  },

  _isSelectElement: function(aElement) {
    return (aElement instanceof HTMLSelectElement);
  },

  _isOptionElement: function(aElement) {
    return aElement instanceof HTMLOptionElement;
  },

  _isOptionGroupElement: function(aElement) {
    return aElement instanceof HTMLOptGroupElement;
  },

  getListForElement: function(aElement) {
    let result = {
      type: "Prompt:Show",
      multiple: aElement.multiple,
      selected: [],
      listitems: []
    };

    if (aElement.multiple) {
      result.buttons = [
        { label: Strings.browser.GetStringFromName("selectHelper.closeMultipleSelectDialog") },
      ];
    }
  
    this.forOptions(aElement, function(aNode, aIndex) {
      result.listitems[aIndex] = {
        label: aNode.text || aNode.label,
        isGroup: this._isOptionGroupElement(aNode),
        inGroup: this._isOptionGroupElement(aNode.parentNode),
        disabled: aNode.disabled,
        id: aIndex
      }
      result.selected[aIndex] = aNode.selected;
    });
    return result;
  },

  forOptions: function(aElement, aFunction) {
    let optionIndex = 0;
    let children = aElement.children;
    // if there are no children in this select, we add a dummy row so that at least something appears
    if (children.length == 0)
      aFunction.call(this, {label:""}, optionIndex);
    for (let i = 0; i < children.length; i++) {
      let child = children[i];
      if (this._isOptionGroupElement(child)) {
        aFunction.call(this, child, optionIndex);
        optionIndex++;

        let subchildren = child.children;
        for (let j = 0; j < subchildren.length; j++) {
          let subchild = subchildren[j];
          aFunction.call(this, subchild, optionIndex);
          optionIndex++;
        }

      } else if (this._isOptionElement(child)) {
        // This is a regular choice under no group.
        aFunction.call(this, child, optionIndex);
        optionIndex++;
      }
    }
  }
}

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
        NativeWindow.doorhanger.show(messageString, aTopic, buttons);
        break;
    }
  }
};

/**
 * Handler for blocked popups, triggered by DOMUpdatePageReport events in browser.xml
 */
var PopupBlockerObserver = {
  onUpdatePageReport: function onUpdatePageReport(aEvent) {
    let browser = BrowserApp.selectedBrowser;
    if (aEvent.originalTarget != browser)
      return;

    if (!browser.pageReport)
      return;

    let result = Services.perms.testExactPermission(BrowserApp.selectedBrowser.currentURI, "popup");
    if (result == Ci.nsIPermissionManager.DENY_ACTION)
      return;

    // Only show the notification again if we've not already shown it. Since
    // notifications are per-browser, we don't need to worry about re-adding
    // it.
    if (!browser.pageReport.reported) {
      if (Services.prefs.getBoolPref("privacy.popups.showBrowserMessage")) {
        let brandShortName = Strings.brand.GetStringFromName("brandShortName");
        let message;
        let popupCount = browser.pageReport.length;

        let strings = Strings.browser;
        if (popupCount > 1)
          message = strings.formatStringFromName("popupWarningMultiple", [brandShortName, popupCount], 2);
        else
          message = strings.formatStringFromName("popupWarning", [brandShortName], 1);

        let buttons = [
          {
            label: strings.GetStringFromName("popupButtonAllowOnce"),
            callback: function() { PopupBlockerObserver.showPopupsForSite(); }
          },
          {
            label: strings.GetStringFromName("popupButtonAlwaysAllow2"),
            callback: function() { PopupBlockerObserver.allowPopupsForSite(true); }
          },
          {
            label: strings.GetStringFromName("popupButtonNeverWarn2"),
            callback: function() { PopupBlockerObserver.allowPopupsForSite(false); }
          }
        ];

        NativeWindow.doorhanger.show(message, "popup-blocked", buttons);
      }
      // Record the fact that we've reported this blocked popup, so we don't
      // show it again.
      browser.pageReport.reported = true;
    }
  },

  allowPopupsForSite: function allowPopupsForSite(aAllow) {
    let currentURI = BrowserApp.selectedBrowser.currentURI;
    Services.perms.add(currentURI, "popup", aAllow
                       ?  Ci.nsIPermissionManager.ALLOW_ACTION
                       :  Ci.nsIPermissionManager.DENY_ACTION);
    dump("Allowing popups for: " + currentURI);
  },

  showPopupsForSite: function showPopupsForSite() {
    let uri = BrowserApp.selectedBrowser.currentURI;
    let pageReport = BrowserApp.selectedBrowser.pageReport;
    if (pageReport) {
      for (let i = 0; i < pageReport.length; ++i) {
        var popupURIspec = pageReport[i].popupWindowURI.spec;

        // Sometimes the popup URI that we get back from the pageReport
        // isn't useful (for instance, netscape.com's popup URI ends up
        // being "http://www.netscape.com", which isn't really the URI of
        // the popup they're trying to show).  This isn't going to be
        // useful to the user, so we won't create a menu item for it.
        if (popupURIspec == "" || popupURIspec == "about:blank" || popupURIspec == uri.spec)
          continue;

        let popupFeatures = pageReport[i].popupWindowFeatures;
        let popupName = pageReport[i].popupWindowName;

        BrowserApp.addTab(popupURIspec);
      }
    }
  }
};


var OfflineApps = {
  init: function() {
    BrowserApp.deck.addEventListener("MozApplicationManifest", this, false);
  },

  uninit: function() {
    BrowserApp.deck.removeEventListener("MozApplicationManifest", this, false);
  },

  handleEvent: function(aEvent) {
    if (aEvent.type == "MozApplicationManifest")
      this.offlineAppRequested(aEvent.originalTarget.defaultView);
  },

  offlineAppRequested: function(aContentWindow) {
    if (!Services.prefs.getBoolPref("browser.offline-apps.notify"))
      return;

    let browser = BrowserApp.getBrowserForWindow(aContentWindow);
    let tab = BrowserApp.getTabForBrowser(browser);
    let currentURI = aContentWindow.document.documentURIObject;

    // Don't bother showing UI if the user has already made a decision
    if (Services.perms.testExactPermission(currentURI, "offline-app") != Services.perms.UNKNOWN_ACTION)
      return;

    try {
      if (Services.prefs.getBoolPref("offline-apps.allow_by_default")) {
        // All pages can use offline capabilities, no need to ask the user
        return;
      }
    } catch(e) {
      // This pref isn't set by default, ignore failures
    }

    let host = currentURI.asciiHost;
    let notificationID = "offline-app-requested-" + host;

    let strings = Strings.browser;
    let buttons = [{
      label: strings.GetStringFromName("offlineApps.allow"),
      callback: function() {
        OfflineApps.allowSite(aContentWindow.document);
      }
    },
    {
      label: strings.GetStringFromName("offlineApps.never"),
      callback: function() {
        OfflineApps.disallowSite(aContentWindow.document);
      }
    },
    {
      label: strings.GetStringFromName("offlineApps.notNow"),
      callback: function() { /* noop */ }
    }];

    let message = strings.formatStringFromName("offlineApps.available2", [host], 1);
    NativeWindow.doorhanger.show(message, notificationID, buttons, tab.id);
  },

  allowSite: function(aDocument) {
    Services.perms.add(aDocument.documentURIObject, "offline-app", Services.perms.ALLOW_ACTION);

    // When a site is enabled while loading, manifest resources will
    // start fetching immediately.  This one time we need to do it
    // ourselves.
    this._startFetching(aDocument);
  },

  disallowSite: function(aDocument) {
    Services.perms.add(aDocument.documentURIObject, "offline-app", Services.perms.DENY_ACTION);
  },

  _startFetching: function(aDocument) {
    if (!aDocument.documentElement)
      return;

    let manifest = aDocument.documentElement.getAttribute("manifest");
    if (!manifest)
      return;

    let manifestURI = Services.io.newURI(manifest, aDocument.characterSet, aDocument.documentURIObject);
    let updateService = Cc["@mozilla.org/offlinecacheupdate-service;1"].getService(Ci.nsIOfflineCacheUpdateService);
    updateService.scheduleUpdate(manifestURI, aDocument.documentURIObject, window);
  }
};

var IndexedDB = {
  _permissionsPrompt: "indexedDB-permissions-prompt",
  _permissionsResponse: "indexedDB-permissions-response",

  _quotaPrompt: "indexedDB-quota-prompt",
  _quotaResponse: "indexedDB-quota-response",
  _quotaCancel: "indexedDB-quota-cancel",

  init: function IndexedDB_init() {
    Services.obs.addObserver(this, this._permissionsPrompt, false);
    Services.obs.addObserver(this, this._quotaPrompt, false);
    Services.obs.addObserver(this, this._quotaCancel, false);
  },

  uninit: function IndexedDB_uninit() {
    Services.obs.removeObserver(this, this._permissionsPrompt, false);
    Services.obs.removeObserver(this, this._quotaPrompt, false);
    Services.obs.removeObserver(this, this._quotaCancel, false);
  },

  observe: function IndexedDB_observe(subject, topic, data) {
    if (topic != this._permissionsPrompt &&
        topic != this._quotaPrompt &&
        topic != this._quotaCancel) {
      throw new Error("Unexpected topic!");
    }

    let requestor = subject.QueryInterface(Ci.nsIInterfaceRequestor);

    let contentWindow = requestor.getInterface(Ci.nsIDOMWindow);
    let contentDocument = contentWindow.document;
    let browser = BrowserApp.getBrowserForWindow(contentWindow);
    if (!browser)
      return;

    let host = contentDocument.documentURIObject.asciiHost;

    let strings = Strings.browser;

    let message, responseTopic;
    if (topic == this._permissionsPrompt) {
      message = strings.formatStringFromName("offlineApps.available2", [host], 1);
      responseTopic = this._permissionsResponse;
    } else if (topic == this._quotaPrompt) {
      message = strings.formatStringFromName("indexedDBQuota.wantsTo", [ host, data ], 2);
      responseTopic = this._quotaResponse;
    } else if (topic == this._quotaCancel) {
      responseTopic = this._quotaResponse;
    }

    let notificationID = responseTopic + host;
    let tab = BrowserApp.getTabForBrowser(browser);
    let observer = requestor.getInterface(Ci.nsIObserver);

    if (topic == this._quotaCancel) {
      NativeWindow.doorhanger.hide(notificationID, tab.id);
      observer.observe(null, responseTopic, Ci.nsIPermissionManager.UNKNOWN_ACTION);
      return;
    }

    let buttons = [{
      label: strings.GetStringFromName("offlineApps.allow"),
      callback: function() {
        observer.observe(null, responseTopic, Ci.nsIPermissionManager.ALLOW_ACTION);
      }
    },
    {
      label: strings.GetStringFromName("offlineApps.never"),
      callback: function() {
        observer.observe(null, responseTopic, Ci.nsIPermissionManager.DENY_ACTION);
      }
    },
    {
      label: strings.GetStringFromName("offlineApps.notNow"),
      callback: function() {
        observer.observe(null, responseTopic, Ci.nsIPermissionManager.UNKNOWN_ACTION);
      }
    }];

    NativeWindow.doorhanger.show(message, notificationID, buttons, tab.id);
  }
};
