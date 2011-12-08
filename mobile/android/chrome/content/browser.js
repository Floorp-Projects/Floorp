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
Cu.import("resource://gre/modules/Services.jsm")
Cu.import("resource://gre/modules/AddonManager.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "URIFixup",
  "@mozilla.org/docshell/urifixup;1", "nsIURIFixup");
XPCOMUtils.defineLazyServiceGetter(this, "DOMUtils",
  "@mozilla.org/inspector/dom-utils;1", "inIDOMUtils");
const kStateActive = 0x00000001; // :active pseudoclass for elements

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

const UA_MODE_MOBILE = "mobile";
const UA_MODE_DESKTOP = "desktop";

function dump(a) {
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(a);
}

function getBridge() {
  return Cc["@mozilla.org/android/bridge;1"].getService(Ci.nsIAndroidBridge);
}

function sendMessageToJava(aMessage) {
  return getBridge().handleGeckoMessage(JSON.stringify(aMessage));
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

  startup: function startup() {
    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = new nsBrowserAccess();
    dump("zerdatime " + Date.now() + " - browser chrome startup finished.");

    this.deck = document.getElementById("browsers");
    BrowserEventHandler.init();
    ViewportHandler.init();

    getBridge().setDrawMetadataProvider(this.getDrawMetadata.bind(this));

    Services.obs.addObserver(this, "Tab:Add", false);
    Services.obs.addObserver(this, "Tab:Load", false);
    Services.obs.addObserver(this, "Tab:Select", false);
    Services.obs.addObserver(this, "Tab:Close", false);
    Services.obs.addObserver(this, "Session:Back", false);
    Services.obs.addObserver(this, "Session:Forward", false);
    Services.obs.addObserver(this, "Session:Reload", false);
    Services.obs.addObserver(this, "Session:Stop", false);
    Services.obs.addObserver(this, "SaveAs:PDF", false);
    Services.obs.addObserver(this, "Browser:Quit", false);
    Services.obs.addObserver(this, "Preferences:Get", false);
    Services.obs.addObserver(this, "Preferences:Set", false);
    Services.obs.addObserver(this, "ScrollTo:FocusedInput", false);
    Services.obs.addObserver(this, "Sanitize:ClearAll", false);
    Services.obs.addObserver(this, "PanZoom:PanZoom", false);
    Services.obs.addObserver(this, "FullScreen:Exit", false);
    Services.obs.addObserver(this, "Viewport:Change", false);
    Services.obs.addObserver(this, "AgentMode:Change", false);
    Services.obs.addObserver(this, "SearchEngines:Get", false);

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
    XPInstallObserver.init();
    ConsoleAPI.init();

    // Init LoginManager
    Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);

    let uri = "about:home";
    if ("arguments" in window && window.arguments[0])
      uri = window.arguments[0];

    // XXX maybe we don't do this if the launch was kicked off from external
    Services.io.offline = false;
    let newTab = this.addTab(uri);

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
    ViewportHandler.uninit();
    XPInstallObserver.uninit();
    ConsoleAPI.uninit();
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

    aTab.updateViewport(false);
    this.deck.selectedPanel = aTab.vbox;
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
    aParams = aParams || { selected: true };
    let newTab = new Tab(aURI, aParams);
    this._tabs.push(newTab);
    if ("selected" in aParams && aParams.selected)
      newTab.active = true;

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
      Cu.reportError("got quit quit message");
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

    let webBrowserPrint = content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebBrowserPrint);

    let cancelable = {
      cancel: function (aReason) {
        webBrowserPrint.cancel();
      }
    }
    let download = dm.addDownload(Ci.nsIDownloadManager.DOWNLOAD_TYPE_DOWNLOAD,
                                  aBrowser.currentURI,
                                  Services.io.newFileURI(file), "", null,
                                  Date.now() * 1000, null, cancelable);

    webBrowserPrint.print(printSettings, download);
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

  getSearchEngines: function() {
    let engineData = Services.search.getEngines({});
    let searchEngines = engineData.map(function (engine) {
      return {
        name: engine.name,
        iconURI: engine.iconURI.spec
      };
    });

    sendMessageToJava({
      gecko: {
        type: "SearchEngines:Data",
        searchEngines: searchEngines
      }
    });
  },

  getSearchOrFixupURI: function(aData) {
    let args = JSON.parse(aData);
    let uri;
    if (args.engine) {
      let engine = Services.search.getEngineByName(args.engine);
      uri = engine.getSubmission(args.url).uri;
    } else
      uri = URIFixup.createFixupURI(args.url, Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP);
    return uri ? uri.spec : args.url;
  },

  scrollToFocusedInput: function(aBrowser) {
    let doc = aBrowser.contentDocument;
    if (!doc)
      return;
    let focused = doc.activeElement;
    if ((focused instanceof HTMLInputElement && focused.mozIsTextField(false)) || (focused instanceof HTMLTextAreaElement)) {
      focused.scrollIntoView(false);
      BrowserApp.getTabForBrowser(aBrowser).sendViewportUpdate();
    }
  },

  getDrawMetadata: function getDrawMetadata() {
    return JSON.stringify(this.selectedTab.viewport);
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
    } else if (aTopic == "Session:Stop") {
      browser.stop();
    } else if (aTopic == "Tab:Add") {
      let newTab = this.addTab(this.getSearchOrFixupURI(aData));
    } else if (aTopic == "Tab:Load") {
      browser.loadURI(this.getSearchOrFixupURI(aData));
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
    } else if (aTopic == "AgentMode:Change") {
      let args = JSON.parse(aData);
      let tab = this.getTabForId(args.tabId);
      tab.setAgentMode(args.agent);
      tab.browser.reload();
    } else if (aTopic == "ScrollTo:FocusedInput") {
      this.scrollToFocusedInput(browser);
    } else if (aTopic == "Sanitize:ClearAll") {
      Sanitizer.sanitize();
    } else if (aTopic == "FullScreen:Exit") {
      browser.contentDocument.mozCancelFullScreen();
    } else if (aTopic == "Viewport:Change") {
      this.selectedTab.viewport = JSON.parse(aData);
      ViewportHandler.onResize();
    } else if (aTopic == "SearchEngines:Get") {
      this.getSearchEngines();
    }
  },

  get defaultBrowserWidth() {
    delete this.defaultBrowserWidth;
    let width = Services.prefs.getIntPref("browser.viewport.desktopWidth");
    return this.defaultBrowserWidth = width;
   }
};

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
                 BrowserApp.addTab(url, { selected: false });
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
      BrowserEventHandler._cancelTapHighlight();
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

const kDefaultMetadata = { autoSize: false, allowZoom: true, autoScale: true };

// track the last known screen size so that new tabs
// get created with the right size rather than being 1x1
let gScreenWidth = 1;
let gScreenHeight = 1;

function Tab(aURL, aParams) {
  this.browser = null;
  this.vbox = null;
  this.id = 0;
  this.agentMode = UA_MODE_MOBILE;
  this.lastHost = null;
  this.create(aURL, aParams);
  this._metadata = null;
  this._viewport = { x: 0, y: 0, width: gScreenWidth, height: gScreenHeight, offsetX: 0, offsetY: 0,
                     pageWidth: 1, pageHeight: 1, zoom: 1.0 };
  this.viewportExcess = { x: 0, y: 0 };
  this.userScrollPos = { x: 0, y: 0 };
}

Tab.prototype = {
  create: function(aURL, aParams) {
    if (this.browser)
      return;

    this.vbox = document.createElement("vbox");
    this.vbox.align = "start";
    BrowserApp.deck.appendChild(this.vbox);

    this.browser = document.createElement("browser");
    this.browser.setAttribute("type", "content");
    this.setBrowserSize(980, 480);
    this.browser.style.MozTransformOrigin = "0 0";
    this.vbox.appendChild(this.browser);

    // Turn off clipping so we can buffer areas outside of the browser element.
    let frameLoader = this.browser.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader;
    frameLoader.clipSubdocument = false;

    this.browser.stop();

    this.id = ++gTabIDFactory;
    aParams = aParams || { selected: true };
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
    this.browser.addEventListener("DOMContentLoaded", this, true);
    this.browser.addEventListener("DOMLinkAdded", this, true);
    this.browser.addEventListener("DOMTitleChanged", this, true);
    this.browser.addEventListener("scroll", this, true);
    Services.obs.addObserver(this, "http-on-modify-request", false);
    this.browser.loadURI(aURL);
  },

  setAgentMode: function(aMode) {
    if (this.agentMode != aMode) {
      this.agentMode = aMode;
      sendMessageToJava({
        gecko: {
          type: "AgentMode:Changed",
          agentMode: aMode,
          tabId: this.id
        }
      });
    }
  },

  setHostFromURL: function(aURL) {
    let uri = Services.io.newURI(aURL, null, null);
    let host = uri.asciiHost;
    if (this.lastHost != host) {
      this.lastHost = host;
      // TODO: remember mobile/desktop selection for each host (bug 705840)
      this.setAgentMode(UA_MODE_MOBILE);
    }
  },

  destroy: function() {
    if (!this.browser)
      return;

    this.browser.removeProgressListener(this);
    this.browser.removeEventListener("DOMContentLoaded", this, true);
    this.browser.removeEventListener("DOMLinkAdded", this, true);
    this.browser.removeEventListener("DOMTitleChanged", this, true);
    this.browser.removeEventListener("scroll", this, true);
    BrowserApp.deck.removeChild(this.vbox);
    Services.obs.removeObserver(this, "http-on-modify-request", false);
    this.browser = null;
    this.vbox = null;
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

  set viewport(aViewport) {
    // Transform coordinates based on zoom
    aViewport.x /= aViewport.zoom;
    aViewport.y /= aViewport.zoom;

    // Set scroll position
    let win = this.browser.contentWindow;
    win.scrollTo(aViewport.x, aViewport.y);
    this.userScrollPos.x = win.scrollX;
    this.userScrollPos.y = win.scrollY;

    // If we've been asked to over-scroll, do it via the transformation
    // and store it separately to the viewport.
    let excessX = aViewport.x - win.scrollX;
    let excessY = aViewport.y - win.scrollY;

    this._viewport.width = gScreenWidth = aViewport.width;
    this._viewport.height = gScreenHeight = aViewport.height;

    let transformChanged = false;

    if ((aViewport.offsetX != this._viewport.offsetX) ||
        (excessX != this.viewportExcess.x)) {
      this._viewport.offsetX = aViewport.offsetX;
      this.viewportExcess.x = excessX;
      transformChanged = true;
    }
    if ((aViewport.offsetY != this._viewport.offsetY) ||
        (excessY != this.viewportExcess.y)) {
      this._viewport.offsetY = aViewport.offsetY;
      this.viewportExcess.y = excessY;
      transformChanged = true;
    }
    if (Math.abs(aViewport.zoom - this._viewport.zoom) >= 1e-6) {
      this._viewport.zoom = aViewport.zoom;
      transformChanged = true;
    }

    let hasZoom = (Math.abs(this._viewport.zoom - 1.0) >= 1e-6);

    if (transformChanged) {
      let x = this._viewport.offsetX + Math.round(-excessX * this._viewport.zoom);
      let y = this._viewport.offsetY + Math.round(-excessY * this._viewport.zoom);

      let transform =
        "translate(" + x + "px, " +
                       y + "px)";
      if (hasZoom)
        transform += " scale(" + this._viewport.zoom + ")";

      this.browser.style.MozTransform = transform;
    }
  },

  get viewport() {
    // Update the viewport to current dimensions
    this._viewport.x = this.browser.contentWindow.scrollX +
                       this.viewportExcess.x;
    this._viewport.y = this.browser.contentWindow.scrollY +
                       this.viewportExcess.y;

    let doc = this.browser.contentDocument;
    let pageWidth = this._viewport.width;
    let pageHeight = this._viewport.height;
    if (doc != null) {
      let body = doc.body || { scrollWidth: pageWidth, scrollHeight: pageHeight };
      let html = doc.documentElement || { scrollWidth: pageWidth, scrollHeight: pageHeight };
      pageWidth = Math.max(body.scrollWidth, html.scrollWidth);
      pageHeight = Math.max(body.scrollHeight, html.scrollHeight);
    }

    // Transform coordinates based on zoom
    this._viewport.x = Math.round(this._viewport.x * this._viewport.zoom);
    this._viewport.y = Math.round(this._viewport.y * this._viewport.zoom);
    this._viewport.pageWidth = Math.round(pageWidth * this._viewport.zoom);
    this._viewport.pageHeight = Math.round(pageHeight * this._viewport.zoom);

    return this._viewport;
  },

  updateViewport: function(aReset) {
    let win = this.browser.contentWindow;
    let zoom = (aReset ? this.getDefaultZoomLevel() : this._viewport.zoom);
    let xpos = (aReset ? win.scrollX * zoom : this._viewport.x);
    let ypos = (aReset ? win.scrollY * zoom : this._viewport.y);

    this.viewportExcess = { x: 0, y: 0 };
    this.viewport = { x: xpos, y: ypos,
                      offsetX: 0, offsetY: 0,
                      width: this._viewport.width, height: this._viewport.height,
                      pageWidth: 1, pageHeight: 1,
                      zoom: zoom };
    this.sendViewportUpdate();
  },

  sendViewportUpdate: function() {
    sendMessageToJava({
      gecko: {
        type: "Viewport:Update",
        viewport: JSON.stringify(this.viewport)
      }
    });
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "DOMContentLoaded": {
        let target = aEvent.originalTarget;

        // ignore on frames
        if (target.defaultView != this.browser.contentWindow)
          return;

        this.updateViewport(true);

        sendMessageToJava({
          gecko: {
            type: "DOMContentLoaded",
            tabID: this.id,
            windowID: 0,
            uri: this.browser.currentURI.spec,
            title: this.browser.contentTitle
          }
        });

        // Attach a listener to watch for "click" events bubbling up from error
        // pages and other similar page. This lets us fix bugs like 401575 which
        // require error page UI to do privileged things, without letting error
        // pages have any privilege themselves.
        if (/^about:/.test(target.documentURI)) {
          this.browser.addEventListener("click", ErrorPageEventHandler, false);
          this.browser.addEventListener("pagehide", function listener() {
            this.browser.removeEventListener("click", ErrorPageEventHandler, false);
            this.browser.removeEventListener("pagehide", listener, true);
          }, true);
        }

        break;
      }

      case "DOMLinkAdded": {
        let target = aEvent.originalTarget;
        if (!target.href || target.disabled)
          return;

        // ignore on frames
        if (target.defaultView != this.browser.contentWindow)
          return;

        let json = {
          type: "DOMLinkAdded",
          tabID: this.id,
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

        // ignore on frames
        if (aEvent.target.defaultView != this.browser.contentWindow)
          return;

        sendMessageToJava({
          gecko: {
            type: "DOMTitleChanged",
            tabID: this.id,
            title: aEvent.target.title
          }
        });
        break;
      }

      case "scroll": {
        let win = this.browser.contentWindow;
        if (this.userScrollPos.x != win.scrollX || this.userScrollPos.y != win.scrollY) {
          sendMessageToJava({
            gecko: {
              type: "Viewport:UpdateLater"
            }
          });
        }
        break;
      }
    }
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

  onLocationChange: function(aWebProgress, aRequest, aLocationURI, aFlags) {
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

    if ((aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) == 0) {
      this.updateViewport(true);
    } else {
      this.sendViewportUpdate();
    }
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

  get metadata() {
    return this._metadata || kDefaultMetadata;
  },

  /** Update viewport when the metadata changes. */
  updateViewportMetadata: function updateViewportMetadata(aMetadata) {
    if (aMetadata && aMetadata.autoScale) {
      let scaleRatio = aMetadata.scaleRatio = ViewportHandler.getScaleRatio();

      if ("defaultZoom" in aMetadata && aMetadata.defaultZoom > 0)
        aMetadata.defaultZoom *= scaleRatio;
      if ("minZoom" in aMetadata && aMetadata.minZoom > 0)
        aMetadata.minZoom *= scaleRatio;
      if ("maxZoom" in aMetadata && aMetadata.maxZoom > 0)
        aMetadata.maxZoom *= scaleRatio;
    }
    this._metadata = aMetadata;
    this.updateViewportSize();
    this.updateViewport(true);
  },

  /** Update viewport when the metadata or the window size changes. */
  updateViewportSize: function updateViewportSize() {
    let browser = this.browser;
    if (!browser)
      return;

    let screenW = this._viewport.width;
    let screenH = this._viewport.height;
    let viewportW, viewportH;

    let metadata = this.metadata;
    if (metadata.autoSize) {
      if ("scaleRatio" in metadata) {
        viewportW = screenW / metadata.scaleRatio;
        viewportH = screenH / metadata.scaleRatio;
      } else {
        viewportW = screenW;
        viewportH = screenH;
      }
    } else {
      viewportW = metadata.width;
      viewportH = metadata.height;

      // If (scale * width) < device-width, increase the width (bug 561413).
      let maxInitialZoom = metadata.defaultZoom || metadata.maxZoom;
      if (maxInitialZoom && viewportW)
        viewportW = Math.max(viewportW, screenW / maxInitialZoom);

      let validW = viewportW > 0;
      let validH = viewportH > 0;

      if (!validW)
        viewportW = validH ? (viewportH * (screenW / screenH)) : BrowserApp.defaultBrowserWidth;
      if (!validH)
        viewportH = viewportW * (screenH / screenW);
    }

    // Make sure the viewport height is not shorter than the window when
    // the page is zoomed out to show its full width.
    let minScale = this.getPageZoomLevel(screenW);
    viewportH = Math.max(viewportH, screenH / minScale);

    this.setBrowserSize(viewportW, viewportH);
  },

  getDefaultZoomLevel: function getDefaultZoomLevel() {
    let md = this.metadata;
    if ("defaultZoom" in md && md.defaultZoom)
      return md.defaultZoom;

    let browserWidth = parseInt(this.browser.style.width);
    return gScreenWidth / browserWidth;
  },

  getPageZoomLevel: function getPageZoomLevel() {
    // This may get called during a Viewport:Change message while the document
    // has not loaded yet.
    if (!this.browser.contentDocument || !this.browser.contentDocument.body)
      return 1.0;

    return this._viewport.width / this.browser.contentDocument.body.clientWidth;
  },

  setBrowserSize: function(aWidth, aHeight) {
    this.browser.style.width = aWidth + "px";
    this.browser.style.height = aHeight + "px";
  },

  getRequestLoadContext: function(aRequest) {
    if (aRequest && aRequest.notificationCallbacks) {
      try {
        return aRequest.notificationCallbacks.getInterface(Ci.nsILoadContext);
      } catch (ex) { }
    }

    if (aRequest && aRequest.loadGroup && aRequest.loadGroup.notificationCallbacks) {
      try {
        return aRequest.loadGroup.notificationCallbacks.getInterface(Ci.nsILoadContext);
      } catch (ex) { }
    }

    return null;
  },

  getWindowForRequest: function(aRequest) {
    let loadContext = this.getRequestLoadContext(aRequest);
    if (loadContext)
      return loadContext.associatedWindow;
    return null;
  },

  observe: function(aSubject, aTopic, aData) {
    if (!(aSubject instanceof Ci.nsIHttpChannel))
      return;

    let channel = aSubject.QueryInterface(Ci.nsIHttpChannel);
    if (!(channel.loadFlags & Ci.nsIChannel.LOAD_DOCUMENT_URI))
      return;

    let channelWindow = this.getWindowForRequest(channel);
    if (channelWindow == this.browser.contentWindow) {
      this.setHostFromURL(channel.URI.spec);
      if (this.agentMode == UA_MODE_DESKTOP)
        channel.setRequestHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:7.0.1) Gecko/20100101 Firefox/7.0.1", false);
    }
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIWebProgressListener,
    Ci.nsISHistoryListener,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference
  ])
};


var BrowserEventHandler = {
  init: function init() {
    Services.obs.addObserver(this, "Gesture:SingleTap", false);
    Services.obs.addObserver(this, "Gesture:ShowPress", false);
    Services.obs.addObserver(this, "Gesture:CancelTouch", false);
    Services.obs.addObserver(this, "Gesture:DoubleTap", false);
    Services.obs.addObserver(this, "Gesture:Scroll", false);

    BrowserApp.deck.addEventListener("DOMUpdatePageReport", PopupBlockerObserver.onUpdatePageReport, false);
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "Gesture:Scroll") {
      // If we've lost our scrollable element, return. Don't cancel the
      // override, as we probably don't want Java to handle panning until the
      // user releases their finger.
      if (this._scrollableElement == null)
        return;

      // If this is the first scroll event and we can't scroll in the direction
      // the user wanted, and neither can any non-root sub-frame, cancel the
      // override so that Java can handle panning the main document.
      let data = JSON.parse(aData);
      if (this._firstScrollEvent) {
        while (this._scrollableElement != null &&
               !this._elementCanScroll(this._scrollableElement, data.x, data.y))
          this._scrollableElement = this._findScrollableElement(this._scrollableElement, false);

        let doc = BrowserApp.selectedBrowser.contentDocument;
        if (this._scrollableElement == doc.body ||
            this._scrollableElement == doc.documentElement) {
          sendMessageToJava({ gecko: { type: "Panning:CancelOverride" } });
          return;
        }

        this._firstScrollEvent = false;
      }

      // Scroll the scrollable element
      this._scrollElementBy(this._scrollableElement, data.x, data.y);
      sendMessageToJava({ gecko: { type: "Gesture:ScrollAck" } });
    } else if (aTopic == "Gesture:CancelTouch") {
      this._cancelTapHighlight();
    } else if (aTopic == "Gesture:ShowPress") {
      let data = JSON.parse(aData);
      let closest = ElementTouchHelper.elementFromPoint(BrowserApp.selectedBrowser.contentWindow,
                                                        data.x, data.y);
      if (!closest)
        closest = ElementTouchHelper.anyElementFromPoint(BrowserApp.selectedBrowser.contentWindow,
                                                        data.x, data.y);
      if (closest) {
        this._doTapHighlight(closest);

        // If we've pressed a scrollable element, let Java know that we may
        // want to override the scroll behaviour (for document sub-frames)
        this._scrollableElement = this._findScrollableElement(closest, true);
        this._firstScrollEvent = true;

        if (this._scrollableElement != null) {
          // Discard if it's the top-level scrollable, we let Java handle this
          let doc = BrowserApp.selectedBrowser.contentDocument;
          if (this._scrollableElement != doc.body &&
              this._scrollableElement != doc.documentElement)
            sendMessageToJava({ gecko: { type: "Panning:Override" } });
        }
      }
    } else if (aTopic == "Gesture:SingleTap") {
      let element = this._highlightElement;
      if (element && !FormAssistant.handleClick(element)) {
        let data = JSON.parse(aData);
        [data.x, data.y] = ElementTouchHelper.toScreenCoords(element.ownerDocument.defaultView, data.x, data.y);

        this._sendMouseEvent("mousemove", element, data.x, data.y);
        this._sendMouseEvent("mousedown", element, data.x, data.y);
        this._sendMouseEvent("mouseup",   element, data.x, data.y);
      }
      this._cancelTapHighlight();
    } else if (aTopic == "Gesture:DoubleTap") {
      this._cancelTapHighlight();
      this.onDoubleTap(aData);
    }
  },
 
  _zoomOut: function() {
    this._zoomedToElement = null;
    // zoom out, try to keep the center in the center of the page
    setTimeout(function() {
      sendMessageToJava({ gecko: { type: "Browser:ZoomToPageWidth"} });
    }, 0);    
  },

  onDoubleTap: function(aData) {
    let data = JSON.parse(aData);

    let rect = {};
    let win = BrowserApp.selectedBrowser.contentWindow;
    
    let zoom = BrowserApp.selectedTab._viewport.zoom;
    let element = ElementTouchHelper.anyElementFromPoint(win, data.x, data.y);
    if (!element) {
      this._zoomOut();
      return;
    }

    win = element.ownerDocument.defaultView;
    while (element && win.getComputedStyle(element,null).display == "inline")
      element = element.parentNode;
    if (!element || element == this._zoomedToElement) {
      this._zoomOut();
    } else if (element) {
      const margin = 15;
      this._zoomedToElement = element;
      rect = ElementTouchHelper.getBoundingContentRect(element);

      let zoom = BrowserApp.selectedTab.viewport.zoom;
      rect.x *= zoom;
      rect.y *= zoom;
      rect.w *= zoom;
      rect.h *= zoom;

      setTimeout(function() {
        rect.type = "Browser:ZoomToRect";
        rect.x -= margin;
        rect.w += 2*margin;
        sendMessageToJava({ gecko: rect });
      }, 0);
    }
  },

  _firstScrollEvent: false,

  _scrollableElement: null,

  _highlightElement: null,

  _doTapHighlight: function _doTapHighlight(aElement) {
    DOMUtils.setContentState(aElement, kStateActive);
    this._highlightElement = aElement;
  },

  _cancelTapHighlight: function _cancelTapHighlight() {
    DOMUtils.setContentState(BrowserApp.selectedBrowser.contentWindow.document.documentElement, kStateActive);
    this._highlightElement = null;
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

    [aX, aY] = ElementTouchHelper.toBrowserCoords(aElement.ownerDocument.defaultView, aX, aY);
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

  _elementReceivesInput: function(aElement) {
    return aElement instanceof Element &&
        kElementsReceivingInput.hasOwnProperty(aElement.tagName.toLowerCase()) ||
        this._isEditable(aElement);
  },

  _isEditable: function(aElement) {
    let canEdit = false;

    if (aElement.isContentEditable || aElement.designMode == "on") {
      canEdit = true;
    } else if (aElement instanceof HTMLIFrameElement && (aElement.contentDocument.body.isContentEditable || aElement.contentDocument.designMode == "on")) {
      canEdit = true;
    } else {
      canEdit = aElement.ownerDocument && aElement.ownerDocument.designMode == "on";
    }

    return canEdit;
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

const kReferenceDpi = 240; // standard "pixel" size used in some preferences

const ElementTouchHelper = {
  toBrowserCoords: function(aWindow, aX, aY) {
    let tab = BrowserApp.selectedTab;
    if (aWindow) {
      let browser = BrowserApp.getBrowserForWindow(aWindow);
      tab = BrowserApp.getTabForBrowser(browser);
    }
    let viewport = tab.viewport;
    return [
        ((aX-tab.viewportExcess.x)*viewport.zoom + viewport.offsetX),
        ((aY-tab.viewportExcess.y)*viewport.zoom + viewport.offsetY)
    ];
  },

  toScreenCoords: function(aWindow, aX, aY) {
    let tab = BrowserApp.selectedTab;
    if (aWindow) {
      let browser = BrowserApp.getBrowserForWindow(aWindow);
      tab = BrowserApp.getTabForBrowser(browser);
    }
    let viewport = tab.viewport;
    return [
        (aX - viewport.offsetX)/viewport.zoom + tab.viewportExcess.x,
        (aY - viewport.offsetY)/viewport.zoom + tab.viewportExcess.y
    ];
  },

  anyElementFromPoint: function(aWindow, aX, aY) {
    [aX, aY] = this.toScreenCoords(aWindow, aX, aY);
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
    [aX, aY] = this.toScreenCoords(aWindow, aX, aY);
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

    // if this element is clickable we return quickly
    if (this._isElementClickable(target))
      return target;

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
    let offset = { x: 0, y: 0 };

    let nativeRects = aElement.getClientRects();
    // step out of iframes and frames, offsetting scroll values
    for (let frame = aElement.ownerDocument.defaultView; frame.frameElement; frame = frame.parent) {
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
  },
  getBoundingContentRect: function(aElement) {
    if (!aElement)
      return {x: 0, y: 0, w: 0, h: 0};
  
    let document = aElement.ownerDocument;
    while (document.defaultView.frameElement)
      document = document.defaultView.frameElement.ownerDocument;
  
    let cwu = document.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    let scrollX = {}, scrollY = {};
    cwu.getScrollXY(false, scrollX, scrollY);
  
    let r = aElement.getBoundingClientRect();
 
    // step out of iframes and frames, offsetting scroll values
    for (let frame = aElement.ownerDocument.defaultView; frame.frameElement && frame != content; frame = frame.parent) {
      // adjust client coordinates' origin to be top left of iframe viewport
      let rect = frame.frameElement.getBoundingClientRect();
      let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
      let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
      scrollX.value += rect.left + parseInt(left);
      scrollY.value += rect.top + parseInt(top);
    }

    var x = r.left + scrollX.value;
    var y = r.top + scrollY.value;
    var x2 = x + r.width;
    var y2 = y + r.height;
    return {x: x,
            y: y,
            w: x2 - x,
            h: y2 - y};
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

  handleClick: function(aTarget) {
    let target = aTarget;
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
  init: function xpi_init() {
    Services.obs.addObserver(XPInstallObserver, "addon-install-blocked", false);
    Services.obs.addObserver(XPInstallObserver, "addon-install-started", false);

    AddonManager.addInstallListener(XPInstallObserver);
  },

  uninit: function xpi_uninit() {
    Services.obs.removeObserver(XPInstallObserver, "addon-install-blocked");
    Services.obs.removeObserver(XPInstallObserver, "addon-install-started");

    AddonManager.removeInstallListener(XPInstallObserver);
  },

  observe: function xpi_observer(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "addon-install-started":
        NativeWindow.toast.show(Strings.browser.GetStringFromName("alertAddonsDownloading"), "short");
        break;
      case "addon-install-blocked":
        let installInfo = aSubject.QueryInterface(Ci.amIWebInstallInfo);
        let host = installInfo.originatingURI.host;

        let brandShortName = Strings.brand.GetStringFromName("brandShortName");
        let notificationName, buttons, message;
        let strings = Strings.browser;
        let enabled = true;
        try {
          enabled = Services.prefs.getBoolPref("xpinstall.enabled");
        }
        catch (e) {}

        if (!enabled) {
          notificationName = "xpinstall-disabled";
          if (Services.prefs.prefIsLocked("xpinstall.enabled")) {
            message = strings.GetStringFromName("xpinstallDisabledMessageLocked");
            buttons = [];
          } else {
            message = strings.formatStringFromName("xpinstallDisabledMessage2", [brandShortName, host], 2);
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
          message = strings.formatStringFromName("xpinstallPromptWarning2", [brandShortName, host], 2);

          buttons = [{
            label: strings.GetStringFromName("xpinstallPromptAllowButton"),
            callback: function() {
              // Kick off the install
              installInfo.install();
              return false;
            }
          }];
        }
        NativeWindow.doorhanger.show(message, aTopic, buttons);
        break;
    }
  },

  onInstallEnded: function(aInstall, aAddon) {
    let needsRestart = false;
    if (aInstall.existingAddon && (aInstall.existingAddon.pendingOperations & AddonManager.PENDING_UPGRADE))
      needsRestart = true;
    else if (aAddon.pendingOperations & AddonManager.PENDING_INSTALL)
      needsRestart = true;

    if (needsRestart) {
      buttons = [{
        label: Strings.browser.GetStringFromName("notificationRestart.button"),
        callback: function() {
          // Notify all windows that an application quit has been requested
          let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
          Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");

          // If nothing aborted, quit the app
          if (cancelQuit.data == false) {
            let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
            appStartup.quit(Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit);
          }
        }
      }];

      let message = Strings.browser.GetStringFromName("notificationRestart.normal");
      NativeWindow.doorhanger.show(message, "addon-app-restart", buttons, BrowserApp.selectedTab.id, { persistence: -1 });
    } else {
      let message = Strings.browser.GetStringFromName("alertAddonsInstalledNoRestart");
      NativeWindow.toast.show(message, "short");
    }
  },

  onInstallFailed: function(aInstall) {
    NativeWindow.toast.show(Strings.browser.GetStringFromName("alertAddonsFail"), "short");
  },

  onDownloadProgress: function xpidm_onDownloadProgress(aInstall) {},

  onDownloadFailed: function(aInstall) {
    this.onInstallFailed(aInstall);
  },

  onDownloadCancelled: function(aInstall) {}
};

// Blindly copied from Safari documentation for now.
const kViewportMinScale  = 0;
const kViewportMaxScale  = 10;
const kViewportMinWidth  = 200;
const kViewportMaxWidth  = 10000;
const kViewportMinHeight = 223;
const kViewportMaxHeight = 10000;

var ViewportHandler = {
  init: function init() {
    addEventListener("DOMWindowCreated", this, false);
    addEventListener("DOMMetaAdded", this, false);
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("pageshow", this, false);
    addEventListener("resize", this, false);
  },

  uninit: function uninit() {
    removeEventListener("DOMWindowCreated", this, false);
    removeEventListener("DOMMetaAdded", this, false);
    removeEventListener("DOMContentLoaded", this, false);
    removeEventListener("pageshow", this, false);
    removeEventListener("resize", this, false);
  },

  handleEvent: function handleEvent(aEvent) {
    let target = aEvent.originalTarget;
    let document = target.ownerDocument || target;
    let browser = BrowserApp.getBrowserForDocument(document);
    let tab = BrowserApp.getTabForBrowser(browser);
    if (!tab)
      return;

    switch (aEvent.type) {
      case "DOMWindowCreated":
        this.resetMetadata(tab);
        break;

      case "DOMMetaAdded":
        if (target.name == "viewport")
          this.updateMetadata(tab);
        break;

      case "DOMContentLoaded":
      case "pageshow":
        this.updateMetadata(tab);
        break;

      case "resize":
        this.onResize();
        break;
    }
  },

  resetMetadata: function resetMetadata(tab) {
    tab.updateViewportMetadata(null);
  },

  updateMetadata: function updateMetadata(tab) {
    let metadata = this.getViewportMetadata(tab.browser.contentWindow);
    tab.updateViewportMetadata(metadata);
  },

  /**
   * Returns an object with the page's preferred viewport properties:
   *   defaultZoom (optional float): The initial scale when the page is loaded.
   *   minZoom (optional float): The minimum zoom level.
   *   maxZoom (optional float): The maximum zoom level.
   *   width (optional int): The CSS viewport width in px.
   *   height (optional int): The CSS viewport height in px.
   *   autoSize (boolean): Resize the CSS viewport when the window resizes.
   *   allowZoom (boolean): Let the user zoom in or out.
   *   autoScale (boolean): Adjust the viewport properties to account for display density.
   */
  getViewportMetadata: function getViewportMetadata(aWindow) {
    let doctype = aWindow.document.doctype;
    if (doctype && /(WAP|WML|Mobile)/.test(doctype.publicId))
      return { defaultZoom: 1, autoSize: true, allowZoom: true, autoScale: true };

    let windowUtils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    let handheldFriendly = windowUtils.getDocumentMetadata("HandheldFriendly");
    if (handheldFriendly == "true")
      return { defaultZoom: 1, autoSize: true, allowZoom: true, autoScale: true };

    if (aWindow.document instanceof XULDocument)
      return { defaultZoom: 1, autoSize: true, allowZoom: false, autoScale: false };

    // viewport details found here
    // http://developer.apple.com/safari/library/documentation/AppleApplications/Reference/SafariHTMLRef/Articles/MetaTags.html
    // http://developer.apple.com/safari/library/documentation/AppleApplications/Reference/SafariWebContent/UsingtheViewport/UsingtheViewport.html

    // Note: These values will be NaN if parseFloat or parseInt doesn't find a number.
    // Remember that NaN is contagious: Math.max(1, NaN) == Math.min(1, NaN) == NaN.
    let scale = parseFloat(windowUtils.getDocumentMetadata("viewport-initial-scale"));
    let minScale = parseFloat(windowUtils.getDocumentMetadata("viewport-minimum-scale"));
    let maxScale = parseFloat(windowUtils.getDocumentMetadata("viewport-maximum-scale"));

    let widthStr = windowUtils.getDocumentMetadata("viewport-width");
    let heightStr = windowUtils.getDocumentMetadata("viewport-height");
    let width = this.clamp(parseInt(widthStr), kViewportMinWidth, kViewportMaxWidth);
    let height = this.clamp(parseInt(heightStr), kViewportMinHeight, kViewportMaxHeight);

    let allowZoomStr = windowUtils.getDocumentMetadata("viewport-user-scalable");
    let allowZoom = !/^(0|no|false)$/.test(allowZoomStr); // WebKit allows 0, "no", or "false"

    scale = this.clamp(scale, kViewportMinScale, kViewportMaxScale);
    minScale = this.clamp(minScale, kViewportMinScale, kViewportMaxScale);
    maxScale = this.clamp(maxScale, kViewportMinScale, kViewportMaxScale);

    // If initial scale is 1.0 and width is not set, assume width=device-width
    let autoSize = (widthStr == "device-width" ||
                    (!widthStr && (heightStr == "device-height" || scale == 1.0)));

    return {
      defaultZoom: scale,
      minZoom: minScale,
      maxZoom: maxScale,
      width: width,
      height: height,
      autoSize: autoSize,
      allowZoom: allowZoom,
      autoScale: true
    };
  },

  onResize: function onResize() {
    for (let i = 0; i < BrowserApp.tabs.length; i++)
      BrowserApp.tabs[i].updateViewportSize();
  },

  clamp: function(num, min, max) {
    return Math.max(min, Math.min(max, num));
  },

  // The device-pixel-to-CSS-px ratio used to adjust meta viewport values.
  // This is higher on higher-dpi displays, so pages stay about the same physical size.
  getScaleRatio: function getScaleRatio() {
    let prefValue = Services.prefs.getIntPref("browser.viewport.scaleRatio");
    if (prefValue > 0)
      return prefValue / 100;

    let dpi = this.displayDPI;
    if (dpi < 200) // Includes desktop displays, and LDPI and MDPI Android devices
      return 1;
    else if (dpi < 300) // Includes Nokia N900, and HDPI Android devices
      return 1.5;

    // For very high-density displays like the iPhone 4, calculate an integer ratio.
    return Math.floor(dpi / 150);
  },

  get displayDPI() {
    let utils = window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    delete this.displayDPI;
    return this.displayDPI = utils.displayDPI;
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
        let popupURIspec = pageReport[i].popupWindowURI.spec;

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

var ConsoleAPI = {
  init: function init() {
    Services.obs.addObserver(this, "console-api-log-event", false);
  },

  uninit: function uninit() {
    Services.obs.removeObserver(this, "console-api-log-event", false);
  },

  observe: function observe(aMessage, aTopic, aData) {
    aMessage = aMessage.wrappedJSObject;

    let mappedArguments = Array.map(aMessage.arguments, this.formatResult, this);
    let joinedArguments = Array.join(mappedArguments, " ");

    if (aMessage.level == "error" || aMessage.level == "warn") {
      let flag = (aMessage.level == "error" ? Ci.nsIScriptError.errorFlag : Ci.nsIScriptError.warningFlag);
      let consoleMsg = Cc["@mozilla.org/scripterror;1"].createInstance(Ci.nsIScriptError);
      consoleMsg.init(joinedArguments, null, null, 0, 0, flag, "content javascript");
      Services.console.logMessage(consoleMsg);
    } else if (aMessage.level == "trace") {
      let bundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
      let args = aMessage.arguments;
      let filename = this.abbreviateSourceURL(args[0].filename);
      let functionName = args[0].functionName || bundle.GetStringFromName("stacktrace.anonymousFunction");
      let lineNumber = args[0].lineNumber;

      let body = bundle.formatStringFromName("stacktrace.outputMessage", [filename, functionName, lineNumber], 3);
      body += "\n";
      args.forEach(function(aFrame) {
        let functionName = aFrame.functionName || bundle.GetStringFromName("stacktrace.anonymousFunction");
        body += "  " + aFrame.filename + " :: " + functionName + " :: " + aFrame.lineNumber + "\n";
      });

      Services.console.logStringMessage(body);
    } else if (aMessage.level == "time" && aMessage.arguments) {
      let bundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
      let body = bundle.formatStringFromName("timer.start", [aMessage.arguments.name], 1);
      Services.console.logStringMessage(body);
    } else if (aMessage.level == "timeEnd" && aMessage.arguments) {
      let bundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
      let body = bundle.formatStringFromName("timer.end", [aMessage.arguments.name, aMessage.arguments.duration], 2);
      Services.console.logStringMessage(body);
    } else if (["group", "groupCollapsed", "groupEnd"].indexOf(aMessage.level) != -1) {
      // Do nothing yet
    } else {
      Services.console.logStringMessage(joinedArguments);
    }
  },

  getResultType: function getResultType(aResult) {
    let type = aResult === null ? "null" : typeof aResult;
    if (type == "object" && aResult.constructor && aResult.constructor.name)
      type = aResult.constructor.name;
    return type.toLowerCase();
  },

  formatResult: function formatResult(aResult) {
    let output = "";
    let type = this.getResultType(aResult);
    switch (type) {
      case "string":
      case "boolean":
      case "date":
      case "error":
      case "number":
      case "regexp":
        output = aResult.toString();
        break;
      case "null":
      case "undefined":
        output = type;
        break;
      default:
        if (aResult.toSource) {
          try {
            output = aResult.toSource();
          } catch (ex) { }
        }
        if (!output || output == "({})") {
          output = aResult.toString();
        }
        break;
    }

    return output;
  },

  abbreviateSourceURL: function abbreviateSourceURL(aSourceURL) {
    // Remove any query parameters.
    let hookIndex = aSourceURL.indexOf("?");
    if (hookIndex > -1)
      aSourceURL = aSourceURL.substring(0, hookIndex);

    // Remove a trailing "/".
    if (aSourceURL[aSourceURL.length - 1] == "/")
      aSourceURL = aSourceURL.substring(0, aSourceURL.length - 1);

    // Remove all but the last path component.
    let slashIndex = aSourceURL.lastIndexOf("/");
    if (slashIndex > -1)
      aSourceURL = aSourceURL.substring(slashIndex + 1);

    return aSourceURL;
  }
};
