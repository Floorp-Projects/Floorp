// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;
let Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
#ifdef ACCESSIBILITY
Cu.import("resource://gre/modules/accessibility/AccessFu.jsm");
#endif

XPCOMUtils.defineLazyGetter(this, "PluralForm", function() {
  Cu.import("resource://gre/modules/PluralForm.jsm");
  return PluralForm;
});

XPCOMUtils.defineLazyGetter(this, "DebuggerServer", function() {
  Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
  return DebuggerServer;
});

// Lazily-loaded browser scripts:
[
  ["HelperApps", "chrome://browser/content/HelperApps.js"],
  ["SelectHelper", "chrome://browser/content/SelectHelper.js"],
  ["Readability", "chrome://browser/content/Readability.js"],
  ["WebAppRT", "chrome://browser/content/WebAppRT.js"],
].forEach(function (aScript) {
  let [name, script] = aScript;
  XPCOMUtils.defineLazyGetter(window, name, function() {
    let sandbox = {};
    Services.scriptloader.loadSubScript(script, sandbox);
    return sandbox[name];
  });
});

XPCOMUtils.defineLazyServiceGetter(this, "Haptic",
  "@mozilla.org/widget/hapticfeedback;1", "nsIHapticFeedback");

XPCOMUtils.defineLazyServiceGetter(this, "DOMUtils",
  "@mozilla.org/inspector/dom-utils;1", "inIDOMUtils");

XPCOMUtils.defineLazyServiceGetter(window, "URIFixup",
  "@mozilla.org/docshell/urifixup;1", "nsIURIFixup");

const kStateActive = 0x00000001; // :active pseudoclass for elements

const kXLinkNamespace = "http://www.w3.org/1999/xlink";

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

const kDefaultCSSViewportWidth = 980;
const kDefaultCSSViewportHeight = 480;

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

XPCOMUtils.defineLazyGetter(this, "ContentAreaUtils", function() {
  let ContentAreaUtils = {};
  Services.scriptloader.loadSubScript("chrome://global/content/contentAreaUtils.js", ContentAreaUtils);
  return ContentAreaUtils;
});

XPCOMUtils.defineLazyGetter(this, "Rect", function() {
  Cu.import("resource://gre/modules/Geometry.jsm");
  return Rect;
});

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
  ["browser",    "chrome://browser/locale/browser.properties"],
  ["charset",    "chrome://global/locale/charsetTitles.properties"]
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

    getBridge().browserApp = this;

    Services.obs.addObserver(this, "Tab:Add", false);
    Services.obs.addObserver(this, "Tab:Load", false);
    Services.obs.addObserver(this, "Tab:Selected", false);
    Services.obs.addObserver(this, "Tab:Closed", false);
    Services.obs.addObserver(this, "Session:Back", false);
    Services.obs.addObserver(this, "Session:Forward", false);
    Services.obs.addObserver(this, "Session:Reload", false);
    Services.obs.addObserver(this, "Session:Stop", false);
    Services.obs.addObserver(this, "SaveAs:PDF", false);
    Services.obs.addObserver(this, "Browser:Quit", false);
    Services.obs.addObserver(this, "Preferences:Get", false);
    Services.obs.addObserver(this, "Preferences:Set", false);
    Services.obs.addObserver(this, "ScrollTo:FocusedInput", false);
    Services.obs.addObserver(this, "Sanitize:ClearData", false);
    Services.obs.addObserver(this, "PanZoom:PanZoom", false);
    Services.obs.addObserver(this, "FullScreen:Exit", false);
    Services.obs.addObserver(this, "Viewport:Change", false);
    Services.obs.addObserver(this, "Viewport:Flush", false);
    Services.obs.addObserver(this, "Passwords:Init", false);
    Services.obs.addObserver(this, "FormHistory:Init", false);
    Services.obs.addObserver(this, "ToggleProfiling", false);

    Services.obs.addObserver(this, "sessionstore-state-purge-complete", false);

    function showFullScreenWarning() {
      NativeWindow.toast.show(Strings.browser.GetStringFromName("alertFullScreenToast"), "short");
    }

    window.addEventListener("fullscreen", function() {
      sendMessageToJava({
        gecko: {
          type: window.fullScreen ? "ToggleChrome:Show" : "ToggleChrome:Hide"
        }
      });
    }, false);

    window.addEventListener("mozfullscreenchange", function() {
      sendMessageToJava({
        gecko: {
          type: document.mozFullScreen ? "DOMFullScreen:Start" : "DOMFullScreen:Stop"
        }
      });

      if (document.mozFullScreen)
        showFullScreenWarning();
    }, false);

    // When a restricted key is pressed in DOM full-screen mode, we should display
    // the "Press ESC to exit" warning message.
    window.addEventListener("MozShowFullScreenWarning", showFullScreenWarning, true);

    NativeWindow.init();
    SelectionHandler.init();
    Downloads.init();
    FindHelper.init();
    FormAssistant.init();
    OfflineApps.init();
    IndexedDB.init();
    XPInstallObserver.init();
    ConsoleAPI.init();
    ClipboardHelper.init();
    PermissionsHelper.init();
    CharacterEncoding.init();
    ActivityObserver.init();
    WebappsUI.init();
    RemoteDebugger.init();
    Reader.init();
    UserAgent.init();
    ExternalApps.init();
#ifdef MOZ_TELEMETRY_REPORTING
    Telemetry.init();
#endif
#ifdef ACCESSIBILITY
    AccessFu.attach(window);
#endif

    // Init LoginManager
    Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
    // Init FormHistory
    Cc["@mozilla.org/satchel/form-history;1"].getService(Ci.nsIFormHistory2);

    let loadParams = {};
    let url = "about:home";
    let restoreMode = 0;
    let pinned = false;
    if ("arguments" in window) {
      if (window.arguments[0])
        url = window.arguments[0];
      if (window.arguments[1])
        restoreMode = window.arguments[1];
      if (window.arguments[2])
        gScreenWidth = window.arguments[2];
      if (window.arguments[3])
        gScreenHeight = window.arguments[3];
      if (window.arguments[4])
        pinned = window.arguments[4];
    }

    let updated = this.isAppUpdated();
    if (pinned) {
      WebAppRT.init(updated);
    } else {
      SearchEngines.init();
      this.initContextMenu();
    }

    if (url == "about:empty")
      loadParams.flags = Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY;

    // XXX maybe we don't do this if the launch was kicked off from external
    Services.io.offline = false;

    // Broadcast a UIReady message so add-ons know we are finished with startup
    let event = document.createEvent("Events");
    event.initEvent("UIReady", true, false);
    window.dispatchEvent(event);

    // Restore the previous session
    // restoreMode = 0 means no restore
    // restoreMode = 1 means force restore (after an OOM kill)
    // restoreMode = 2 means restore only if we haven't crashed multiple times
    let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
    if (restoreMode || ss.shouldRestore()) {
      // A restored tab should not be active if we are loading a URL
      let restoreToFront = false;

      sendMessageToJava({
        gecko: {
          type: "Session:RestoreBegin"
        }
      });

      // Open any commandline URLs, except the homepage
      if (url && url != "about:home") {
        loadParams.pinned = pinned;
        this.addTab(url, loadParams);
      } else {
        // Let the session make a restored tab active
        restoreToFront = true;
      }

      // Be ready to handle any restore failures by making sure we have a valid tab opened
      let restoreCleanup = {
        observe: function(aSubject, aTopic, aData) {
          Services.obs.removeObserver(restoreCleanup, "sessionstore-windows-restored");
          if (aData == "fail") {
            BrowserApp.addTab("about:home", {
              showProgress: false,
              selected: restoreToFront
            });
          }

          sendMessageToJava({
            gecko: {
              type: "Session:RestoreEnd"
            }
          });
        }
      };
      Services.obs.addObserver(restoreCleanup, "sessionstore-windows-restored", false);

      // Start the restore
      ss.restoreLastSession(restoreToFront, restoreMode == 1);
    } else {
      loadParams.showProgress = (url != "about:home");
      loadParams.pinned = pinned;
      this.addTab(url, loadParams);

      // show telemetry door hanger if we aren't restoring a session
#ifdef MOZ_TELEMETRY_REPORTING
      Telemetry.prompt();
#endif
    }

    if (updated)
      this.onAppUpdated();

    // notify java that gecko has loaded
    sendMessageToJava({
      gecko: {
        type: "Gecko:Ready"
      }
    });

    // after gecko has loaded, set the checkerboarding pref once at startup (for testing only)
    sendMessageToJava({
      gecko: {
        "type": "Checkerboard:Toggle",
        "value": Services.prefs.getBoolPref("gfx.show_checkerboard_pattern")
      }
    });
  },

  isAppUpdated: function() {
    let savedmstone = null;
    try {
      savedmstone = Services.prefs.getCharPref("browser.startup.homepage_override.mstone");
    } catch (e) {
    }
#expand    let ourmstone = "__MOZ_APP_VERSION__";
    if (ourmstone != savedmstone) {
      Services.prefs.setCharPref("browser.startup.homepage_override.mstone", ourmstone);
      return savedmstone ? "upgrade" : "new";
    }
    return "";
  },

  initContextMenu: function ba_initContextMenu() {
    // TODO: These should eventually move into more appropriate classes
    NativeWindow.contextmenus.add(Strings.browser.GetStringFromName("contextmenu.openInNewTab"),
      NativeWindow.contextmenus.linkOpenableContext,
      function(aTarget) {
        let url = NativeWindow.contextmenus._getLinkURL(aTarget);
        BrowserApp.addTab(url, { selected: false, parentId: BrowserApp.selectedTab.id });

        let newtabStrings = Strings.browser.GetStringFromName("newtabpopup.opened");
        let label = PluralForm.get(1, newtabStrings).replace("#1", 1);
        NativeWindow.toast.show(label, "short");
      });

    NativeWindow.contextmenus.add(Strings.browser.GetStringFromName("contextmenu.copyLink"),
      NativeWindow.contextmenus.linkCopyableContext,
      function(aTarget) {
        let url = NativeWindow.contextmenus._getLinkURL(aTarget);
        NativeWindow.contextmenus._copyStringToDefaultClipboard(url);
      });

    NativeWindow.contextmenus.add(Strings.browser.GetStringFromName("contextmenu.copyEmailAddress"),
      NativeWindow.contextmenus.emailLinkCopyableContext,
      function(aTarget) {
        let url = NativeWindow.contextmenus._getLinkURL(aTarget);
        let emailAddr = NativeWindow.contextmenus._stripScheme(url);
        NativeWindow.contextmenus._copyStringToDefaultClipboard(emailAddr);
      });

    NativeWindow.contextmenus.add(Strings.browser.GetStringFromName("contextmenu.copyPhoneNumber"),
      NativeWindow.contextmenus.phoneNumberLinkCopyableContext,
      function(aTarget) {
        let url = NativeWindow.contextmenus._getLinkURL(aTarget);
        let phoneNumber = NativeWindow.contextmenus._stripScheme(url);
        NativeWindow.contextmenus._copyStringToDefaultClipboard(phoneNumber);
    });

    NativeWindow.contextmenus.add(Strings.browser.GetStringFromName("contextmenu.shareLink"),
      NativeWindow.contextmenus.linkShareableContext,
      function(aTarget) {
        let url = NativeWindow.contextmenus._getLinkURL(aTarget);
        let title = aTarget.textContent || aTarget.title;
        let sharing = Cc["@mozilla.org/uriloader/external-sharing-app-service;1"].getService(Ci.nsIExternalSharingAppService);
        sharing.shareWithDefault(url, "text/plain", title);
      });

    NativeWindow.contextmenus.add(Strings.browser.GetStringFromName("contextmenu.bookmarkLink"),
      NativeWindow.contextmenus.linkBookmarkableContext,
      function(aTarget) {
        let url = NativeWindow.contextmenus._getLinkURL(aTarget);
        let title = aTarget.textContent || aTarget.title || url;
        sendMessageToJava({
          gecko: {
            type: "Bookmark:Insert",
            url: url,
            title: title
          }
        });
      });

    NativeWindow.contextmenus.add(Strings.browser.GetStringFromName("contextmenu.fullScreen"),
      NativeWindow.contextmenus.SelectorContext("video:not(:-moz-full-screen)"),
      function(aTarget) {
        aTarget.mozRequestFullScreen();
      });

    NativeWindow.contextmenus.add(Strings.browser.GetStringFromName("contextmenu.shareImage"),
      NativeWindow.contextmenus.imageSaveableContext,
      function(aTarget) {
        let imageCache = Cc["@mozilla.org/image/cache;1"].getService(Ci.imgICache);
        let props = imageCache.findEntryProperties(aTarget.currentURI, aTarget.ownerDocument.characterSet);
        let src = aTarget.src;
        let type = "";
        try {
           type = String(props.get("type", Ci.nsISupportsCString));
        } catch(ex) {
           type = "";
        }
        sendMessageToJava({
          gecko: {
            type: "Share:Image",
            url: src,
            mime: type,
          }
        });
      });

    NativeWindow.contextmenus.add(Strings.browser.GetStringFromName("contextmenu.saveImage"),
      NativeWindow.contextmenus.imageSaveableContext,
      function(aTarget) {
        let imageCache = Cc["@mozilla.org/image/cache;1"].getService(Ci.imgICache);
        let props = imageCache.findEntryProperties(aTarget.currentURI, aTarget.ownerDocument.characterSet);
        let contentDisposition = "";
        let type = "";
        try {
           contentDisposition = String(props.get("content-disposition", Ci.nsISupportsCString));
           type = String(props.get("type", Ci.nsISupportsCString));
        } catch(ex) {
           contentDisposition = "";
           type = "";
        }
        ContentAreaUtils.internalSave(aTarget.currentURI.spec, null, null, contentDisposition, type, false, "SaveImageTitle", null, aTarget.ownerDocument.documentURIObject, true, null);
      });
  },

  onAppUpdated: function() {
    // initialize the form history and passwords databases on upgrades
    Services.obs.notifyObservers(null, "FormHistory:Init", "");
    Services.obs.notifyObservers(null, "Passwords:Init", "");
  },

  shutdown: function shutdown() {
    NativeWindow.uninit();
    SelectionHandler.uninit();
    FormAssistant.uninit();
    FindHelper.uninit();
    OfflineApps.uninit();
    IndexedDB.uninit();
    ViewportHandler.uninit();
    XPInstallObserver.uninit();
    ConsoleAPI.uninit();
    CharacterEncoding.uninit();
    SearchEngines.uninit();
    WebappsUI.uninit();
    RemoteDebugger.uninit();
    Reader.uninit();
    UserAgent.uninit();
    ExternalApps.uninit();
#ifdef MOZ_TELEMETRY_REPORTING
    Telemetry.uninit();
#endif
  },

  // This function returns false during periods where the browser displayed document is
  // different from the browser content document, so user actions and some kinds of viewport
  // updates should be ignored. This period starts when we start loading a new page or
  // switch tabs, and ends when the new browser content document has been drawn and handed
  // off to the compositor.
  isBrowserContentDocumentDisplayed: function() {
    if (window.top.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).isFirstPaint)
      return false;
    let tab = this.selectedTab;
    if (!tab)
      return true;
    return tab.contentDocumentIsDisplayed;
  },

  displayedDocumentChanged: function() {
    window.top.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).isFirstPaint = true;
  },

  get tabs() {
    return this._tabs;
  },

  get selectedTab() {
    return this._selectedTab;
  },

  set selectedTab(aTab) {
    if (this._selectedTab == aTab)
      return;

    if (this._selectedTab)
      this._selectedTab.setActive(false);

    this._selectedTab = aTab;
    if (!aTab)
      return;

    aTab.setActive(true);
    aTab.setResolution(aTab._zoom, true);
    this.displayedDocumentChanged();
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

  getTabForWindow: function getTabForWindow(aWindow) {
    let tabs = this._tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser.contentWindow == aWindow)
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

  loadURI: function loadURI(aURI, aBrowser, aParams) {
    aBrowser = aBrowser || this.selectedBrowser;
    if (!aBrowser)
      return;

    aParams = aParams || {};

    let flags = "flags" in aParams ? aParams.flags : Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    let postData = ("postData" in aParams && aParams.postData) ? aParams.postData : null;
    let referrerURI = "referrerURI" in aParams ? aParams.referrerURI : null;
    let charset = "charset" in aParams ? aParams.charset : null;

    if ("showProgress" in aParams) {
      let tab = this.getTabForBrowser(aBrowser);
      if (tab)
        tab.showProgress = aParams.showProgress;
    }

    try {
      aBrowser.loadURIWithFlags(aURI, flags, referrerURI, charset, postData);
    } catch(e) {
      let tab = this.getTabForBrowser(aBrowser);
      if (tab) {
        let message = {
          gecko: {
            type: "Content:LoadError",
            tabID: tab.id,
            uri: aBrowser.currentURI.spec,
            title: aBrowser.contentTitle
          }
        };
        sendMessageToJava(message);
        dump("Handled load error: " + e)
      }
    }
  },

  addTab: function addTab(aURI, aParams) {
    aParams = aParams || {};

    let newTab = new Tab(aURI, aParams);
    this._tabs.push(newTab);

    let selected = "selected" in aParams ? aParams.selected : true;
    if (selected)
      this.selectedTab = newTab;

    let pinned = "pinned" in aParams ? aParams.pinned : false;
    if (pinned) {
      let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
      ss.setTabValue(newTab, "appOrigin", aURI);
    }

    let evt = document.createEvent("UIEvents");
    evt.initUIEvent("TabOpen", true, false, window, null);
    newTab.browser.dispatchEvent(evt);

    return newTab;
  },

  // Use this method to close a tab from JS. This method sends a message
  // to Java to close the tab in the Java UI (we'll get a Tab:Closed message
  // back from Java when that happens).
  closeTab: function closeTab(aTab) {
    if (!aTab) {
      Cu.reportError("Error trying to close tab (tab doesn't exist)");
      return;
    }

    let message = {
      gecko: {
        type: "Tab:Close",
        tabID: aTab.id
      }
    };
    sendMessageToJava(message);
  },

  // Calling this will update the state in BrowserApp after a tab has been
  // closed in the Java UI.
  _handleTabClosed: function _handleTabClosed(aTab) {
    if (aTab == this.selectedTab)
      this.selectedTab = null;

    let evt = document.createEvent("UIEvents");
    evt.initUIEvent("TabClose", true, false, window, null);
    aTab.browser.dispatchEvent(evt);

    aTab.destroy();
    this._tabs.splice(this._tabs.indexOf(aTab), 1);
  },

  // Use this method to select a tab from JS. This method sends a message
  // to Java to select the tab in the Java UI (we'll get a Tab:Selected message
  // back from Java when that happens).
  selectTab: function selectTab(aTab) {
    if (!aTab) {
      Cu.reportError("Error trying to select tab (tab doesn't exist)");
      return;
    }

    // There's nothing to do if the tab is already selected
    if (aTab == this.selectedTab)
      return;

    let message = {
      gecko: {
        type: "Tab:Select",
        tabID: aTab.id
      }
    };
    sendMessageToJava(message);
  },

  // This method updates the state in BrowserApp after a tab has been selected
  // in the Java UI.
  _handleTabSelected: function _handleTabSelected(aTab) {
    this.selectedTab = aTab;

    let evt = document.createEvent("UIEvents");
    evt.initUIEvent("TabSelect", true, false, window, null);
    aTab.browser.dispatchEvent(evt);
  },

  quit: function quit() {
    // Figure out if there's at least one other browser window around.
    let lastBrowser = true;
    let e = Services.wm.getEnumerator("navigator:browser");
    while (e.hasMoreElements() && lastBrowser) {
      let win = e.getNext();
      if (win != window)
        lastBrowser = false;
    }

    if (lastBrowser) {
      // Let everyone know we are closing the last browser window
      let closingCanceled = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
      Services.obs.notifyObservers(closingCanceled, "browser-lastwindow-close-requested", null);
      if (closingCanceled.data)
        return;

      Services.obs.notifyObservers(null, "browser-lastwindow-close-granted", null);
    }

    window.QueryInterface(Ci.nsIDOMChromeWindow).minimize();
    window.close();
  },

  saveAsPDF: function saveAsPDF(aBrowser) {
    // Create the final destination file location
    let fileName = ContentAreaUtils.getDefaultFileName(aBrowser.contentTitle, aBrowser.currentURI, null, null);
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

    // Create a valid mimeInfo for the PDF
    let ms = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
    let mimeInfo = ms.getFromTypeAndExtension("application/pdf", "pdf");

    let webBrowserPrint = aBrowser.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                                .getInterface(Ci.nsIWebBrowserPrint);

    let cancelable = {
      cancel: function (aReason) {
        webBrowserPrint.cancel();
      }
    }
    let download = dm.addDownload(Ci.nsIDownloadManager.DOWNLOAD_TYPE_DOWNLOAD,
                                  aBrowser.currentURI,
                                  Services.io.newFileURI(file), "", mimeInfo,
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

        // The plugin pref is actually two separate prefs, so
        // we need to handle it differently
        if (prefName == "plugin.enable") {
          // Use a string type for java's ListPreference
          pref.type = "string";
          pref.value = PluginHelper.getPluginPreference();
          prefs.push(pref);
          continue;
        } else if (prefName == MasterPassword.pref) {
          // Master password is not a "real" pref
          pref.type = "bool";
          pref.value = MasterPassword.enabled;
          prefs.push(pref);
          continue;
        }

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
              try {
                // Try in case it's a localized string (will throw an exception if not)
                pref.value = Services.prefs.getComplexValue(prefName, Ci.nsIPrefLocalizedString).data;
              } catch (e) {
                pref.value = Services.prefs.getCharPref(prefName);
              }
              break;
          }
        } catch (e) {
          dump("Error reading pref [" + prefName + "]: " + e);
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
          case "font.size.inflation.minTwips":
            pref.type = "string";
            pref.value = pref.value.toString();
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

    if (json.name == "plugin.enable") {
      // The plugin pref is actually two separate prefs, so
      // we need to handle it differently
      PluginHelper.setPluginPreference(json.value);
      return;
    } else if (json.name == MasterPassword.pref) {
      // MasterPassword pref is not real, we just need take action and leave
      if (MasterPassword.enabled)
        MasterPassword.removePassword(json.value);
      else
        MasterPassword.setPassword(json.value);
      return;
    }

    // when sending to java, we normalized special preferences that use
    // integers and strings to represent booleans.  here, we convert them back
    // to their actual types so we can store them.
    switch (json.name) {
      case "network.cookie.cookieBehavior":
        json.type = "int";
        json.value = (json.value ? 0 : 2);
        break;
      case "font.size.inflation.minTwips":
        json.type = "int";
        json.value = parseInt(json.value);
        break;
    }

    if (json.type == "bool") {
      Services.prefs.setBoolPref(json.name, json.value);
    } else if (json.type == "int") {
      Services.prefs.setIntPref(json.name, json.value);
    } else {
      let pref = Cc["@mozilla.org/pref-localizedstring;1"].createInstance(Ci.nsIPrefLocalizedString);
      pref.data = json.value;
      Services.prefs.setComplexValue(json.name, Ci.nsISupportsString, pref);
    }
  },

  sanitize: function (aItems) {
    let sanitizer = new Sanitizer();
    let json = JSON.parse(aItems);
    let success = true;

    for (let key in json) {
      if (!json[key])
        continue;

      try {
        switch (key) {
          case "history_downloads":
            sanitizer.clearItem("history");
            sanitizer.clearItem("downloads");
            break;
          case "cookies_sessions":
            sanitizer.clearItem("cookies");
            sanitizer.clearItem("sessions");
            break;
          default:
            sanitizer.clearItem(key);
        }
      } catch (e) {
        dump("sanitize error: " + e);
        success = false;
      }
    }

    sendMessageToJava({
      gecko: {
        type: "Sanitize:Finished",
        success: success
      }
    });
  },

  scrollToFocusedInput: function(aBrowser) {
    let doc = aBrowser.contentDocument;
    if (!doc)
      return;

    let focused = doc.activeElement;
    if ((focused instanceof HTMLInputElement && focused.mozIsTextField(false)) || (focused instanceof HTMLTextAreaElement)) {
      let tab = BrowserApp.getTabForBrowser(aBrowser);
      let win = aBrowser.contentWindow;

      // tell gecko to scroll the field into view. this will scroll any nested scrollable elements
      // as well as the browser's content window, and modify the scrollX and scrollY on the content window.
      focused.scrollIntoView(false);

      // As Gecko isn't aware of the zoom level we're drawing with, the element may not entirely be in view
      // yet. Check for that, and scroll some extra to compensate, if necessary.
      let focusedRect = focused.getBoundingClientRect();
      let visibleContentWidth = gScreenWidth / tab._zoom;
      let visibleContentHeight = gScreenHeight / tab._zoom;

      let positionChanged = false;
      let scrollX = win.scrollX;
      let scrollY = win.scrollY;

      if (focusedRect.right >= visibleContentWidth && focusedRect.left > 0) {
        // the element is too far off the right side, so we need to scroll to the right more
        scrollX += Math.min(focusedRect.left, focusedRect.right - visibleContentWidth);
        positionChanged = true;
      } else if (focusedRect.left < 0) {
        // the element is too far off the left side, so we need to scroll to the left more
        scrollX += focusedRect.left;
        positionChanged = true;
      }
      if (focusedRect.bottom >= visibleContentHeight && focusedRect.top > 0) {
        // the element is too far down, so we need to scroll down more
        scrollY += Math.min(focusedRect.top, focusedRect.bottom - visibleContentHeight);
        positionChanged = true;
      } else if (focusedRect.top < 0) {
        // the element is too far up, so we need to scroll up more
        scrollY += focusedRect.top;
        positionChanged = true;
      }

      if (positionChanged)
        win.scrollTo(scrollX, scrollY);

      // update userScrollPos so that we don't send a duplicate viewport update by triggering
      // our scroll listener
      tab.userScrollPos.x = win.scrollX;
      tab.userScrollPos.y = win.scrollY;

      // finally, let java know where we ended up
      tab.sendViewportUpdate();
    }
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
    } else if (aTopic == "Tab:Add" || aTopic == "Tab:Load") {
      let data = JSON.parse(aData);

      // Pass LOAD_FLAGS_DISALLOW_INHERIT_OWNER to prevent any loads from
      // inheriting the currently loaded document's principal.
      let flags = Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
      if (data.userEntered)
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISALLOW_INHERIT_OWNER;

      let params = {
        selected: true,
        parentId: ("parentId" in data) ? data.parentId : -1,
        flags: flags
      };

      let url = data.url;
      if (data.engine) {
        let engine = Services.search.getEngineByName(data.engine);
        if (engine) {
          let submission = engine.getSubmission(url);
          url = submission.uri.spec;
          params.postData = submission.postData;
        }
      }

      // Don't show progress throbber for about:home
      if (url == "about:home")
        params.showProgress = false;

      if (aTopic == "Tab:Add")
        this.addTab(url, params);
      else
        this.loadURI(url, browser, params);
    } else if (aTopic == "Tab:Selected") {
      this._handleTabSelected(this.getTabForId(parseInt(aData)));
    } else if (aTopic == "Tab:Closed") {
      this._handleTabClosed(this.getTabForId(parseInt(aData)));
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
    } else if (aTopic == "Sanitize:ClearData") {
      this.sanitize(aData);
    } else if (aTopic == "FullScreen:Exit") {
      browser.contentDocument.mozCancelFullScreen();
    } else if (aTopic == "Viewport:Change") {
      if (this.isBrowserContentDocumentDisplayed())
        this.selectedTab.setViewport(JSON.parse(aData));
    } else if (aTopic == "Viewport:Flush") {
      this.displayedDocumentChanged();
    } else if (aTopic == "Passwords:Init") {
      let storage = Components.classes["@mozilla.org/login-manager/storage/mozStorage;1"].
        getService(Components.interfaces.nsILoginManagerStorage);
      storage.init();

      sendMessageToJava({gecko: { type: "Passwords:Init:Return" }});
      Services.obs.removeObserver(this, "Passwords:Init", false);
    } else if (aTopic == "FormHistory:Init") {
      let fh = Cc["@mozilla.org/satchel/form-history;1"].getService(Ci.nsIFormHistory2);
      // Force creation/upgrade of formhistory.sqlite
      let db = fh.DBConnection;
      sendMessageToJava({gecko: { type: "FormHistory:Init:Return" }});
      Services.obs.removeObserver(this, "FormHistory:Init", false);
    } else if (aTopic == "sessionstore-state-purge-complete") {
      sendMessageToJava({ gecko: { type: "Session:StatePurged" }});
    } else if (aTopic == "ToggleProfiling") {
      let profiler = Cc["@mozilla.org/tools/profiler;1"].
                       getService(Ci.nsIProfiler);
      if (profiler.IsActive()) {
        profiler.StopProfiler();
      } else {
        profiler.StartProfiler(100000, 25, ["stackwalk"], 1);
      }
    }
  },

  get defaultBrowserWidth() {
    delete this.defaultBrowserWidth;
    let width = Services.prefs.getIntPref("browser.viewport.desktopWidth");
    return this.defaultBrowserWidth = width;
  },

  // nsIAndroidBrowserApp
  getBrowserTab: function(tabId) {
    return this.getTabForId(tabId);
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
   *        persistWhileVisible:
   *                     A boolean. If true, a visible notification will always
   *                     persist across location changes.
   *        timeout:     A time in milliseconds. The notification will not
   *                     automatically dismiss before this time.
   *        checkbox:    A string to appear next to a checkbox under the notification
   *                     message. The button callback functions will be called with
   *                     the checked state as an argument.                   
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
      sendMessageToJava({ gecko: {
        type: "Doorhanger:Remove",
        value: aValue,
        tabID: aTabID
      }});
    }
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "Menu:Clicked") {
      if (this.menu._callbacks[aData])
        this.menu._callbacks[aData]();
    } else if (aTopic == "Doorhanger:Reply") {
      let data = JSON.parse(aData);
      let reply_id = data["callback"];

      if (this.doorhanger._callbacks[reply_id]) {
        // Pass the value of the optional checkbox to the callback
        let checked = data["checked"];
        this.doorhanger._callbacks[reply_id].cb(checked);

        let prompt = this.doorhanger._callbacks[reply_id].prompt;
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
    _contextId: 0, // id to assign to new context menu items if they are added

    init: function() {
      Services.obs.addObserver(this, "Gesture:LongPress", false);
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
        getValue: function(aElt) {
          return {
            label: (typeof this.name == "function") ? this.name(aElt) : this.name,
            id: this.id
          }
        }
      };
      item.id = this._contextId++;
      this.items[item.id] = item;
      return item.id;
    },

    remove: function(aId) {
      delete this.items[aId];
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

    linkOpenableContext: {
      matches: function linkOpenableContextMatches(aElement) {
        let uri = NativeWindow.contextmenus._getLink(aElement);
        if (uri) {
          let scheme = uri.scheme;
          let dontOpen = /^(mailto|javascript|news|snews)$/;
          return (scheme && !dontOpen.test(scheme));
        }
        return false;
      }
    },

    linkCopyableContext: {
      matches: function linkCopyableContextMatches(aElement) {
        let uri = NativeWindow.contextmenus._getLink(aElement);
        if (uri) {
          let scheme = uri.scheme;
          let dontCopy = /^(mailto|tel)$/;
          return (scheme && !dontCopy.test(scheme));
        }
        return false;
      }
    },

    emailLinkCopyableContext: {
      matches: function emailLinkCopyableContextMatches(aElement) {
        let uri = NativeWindow.contextmenus._getLink(aElement);
        if (uri) {
          return uri.schemeIs("mailto");
        }
        return false;
      }
    },

    phoneNumberLinkCopyableContext: {
      matches: function phoneNumberLinkCopyableContextMatches(aElement) {
        let uri = NativeWindow.contextmenus._getLink(aElement);
        if (uri) {
          return uri.schemeIs("tel");
        }
        return false;
      }
    },

    linkShareableContext: {
      matches: function linkShareableContextMatches(aElement) {
        let uri = NativeWindow.contextmenus._getLink(aElement);
        if (uri) {
          let scheme = uri.scheme;
          let dontShare = /^(chrome|about|file|javascript|resource)$/;
          return (scheme && !dontShare.test(scheme));
        }
        return false;
      }
    },

    linkBookmarkableContext: {
      matches: function linkBookmarkableContextMatches(aElement) {
        let uri = NativeWindow.contextmenus._getLink(aElement);
        if (uri) {
          let scheme = uri.scheme;
          let dontBookmark = /^(mailto)$/;
          return (scheme && !dontBookmark.test(scheme));
        }
        return false;
      }
    },

    textContext: {
      matches: function textContext(aElement) {
        return ((aElement instanceof Ci.nsIDOMHTMLInputElement && aElement.mozIsTextField(false))
                || aElement instanceof Ci.nsIDOMHTMLTextAreaElement);
      }
    },

    imageSaveableContext: {
      matches: function imageSaveableContextMatches(aElement) {
        if (aElement instanceof Ci.nsIImageLoadingContent && aElement.currentURI) {
          // The image must be loaded to allow saving
          let request = aElement.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
          return (request && (request.imageStatus & request.STATUS_SIZE_AVAILABLE));
        }
        return false;
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

        if (this.linkOpenableContext.matches(element) || this.textContext.matches(element))
          break;
        element = element.parentNode;
      }

      // only send the contextmenu event to content if we are planning to show a context menu (i.e. not on every long tap)
      if (this.menuitems) {
        let event = rootElement.ownerDocument.createEvent("MouseEvent");
        event.initMouseEvent("contextmenu", true, true, content,
                             0, aX, aY, aX, aY, false, false, false, false,
                             0, null);
        rootElement.ownerDocument.defaultView.addEventListener("contextmenu", this, false);
        rootElement.dispatchEvent(event);
      } else {
        // Otherwise, let the selection handler take over
        SelectionHandler.startSelection(rootElement, aX, aY);
      }
    },

    _show: function(aEvent) {
      if (aEvent.defaultPrevented)
        return;

      Haptic.performSimpleAction(Haptic.LongPress);

      let popupNode = aEvent.originalTarget;
      let title = "";
      if (popupNode.hasAttribute("title")) {
        title = popupNode.getAttribute("title")
      } else if ((popupNode instanceof Ci.nsIDOMHTMLAnchorElement && popupNode.href) ||
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
        itemArray.push(item.getValue(popupNode));
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

    _getLink: function(aElement) {
      if (aElement.nodeType == Ci.nsIDOMNode.ELEMENT_NODE &&
          ((aElement instanceof Ci.nsIDOMHTMLAnchorElement && aElement.href) ||
          (aElement instanceof Ci.nsIDOMHTMLAreaElement && aElement.href) ||
          aElement instanceof Ci.nsIDOMHTMLLinkElement ||
          aElement.getAttributeNS(kXLinkNamespace, "type") == "simple")) {
        try {
          let url = NativeWindow.contextmenus._getLinkURL(aElement);
          return Services.io.newURI(url, null, null);
        } catch (e) {}
      }
      return null;
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

      return this.makeURLAbsolute(aLink.baseURI, href);
    },

    _copyStringToDefaultClipboard: function(aString) {
      let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
      clipboard.copyString(aString);
    },

    _stripScheme: function(aString) {
      return aString.slice(aString.indexOf(":") + 1);
    }
  }
};

var SelectionHandler = {
  HANDLE_TYPE_START: "START",
  HANDLE_TYPE_END: "END",

  // Keeps track of data about the dimensions of the selection. Coordinates
  // stored here are relative to the _view window.
  cache: null,
  _active: false,

  // The window that holds the selection (can be a sub-frame)
  get _view() {
    if (this._viewRef)
      return this._viewRef.get();
    return null;
  },

  set _view(aView) {
    this._viewRef = Cu.getWeakReference(aView);
  },

  get _cwu() {
    return BrowserApp.selectedBrowser.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).
                                                    getInterface(Ci.nsIDOMWindowUtils);
  },

  _isRTL: false,

  init: function sh_init() {
    Services.obs.addObserver(this, "Gesture:SingleTap", false);
    Services.obs.addObserver(this, "Window:Resize", false);
    Services.obs.addObserver(this, "Tab:Selected", false);
    Services.obs.addObserver(this, "after-viewport-change", false);
    Services.obs.addObserver(this, "TextSelection:Move", false);
    Services.obs.addObserver(this, "TextSelection:Position", false);
  },

  uninit: function sh_uninit() {
    Services.obs.removeObserver(this, "Gesture:SingleTap");
    Services.obs.removeObserver(this, "Window:Resize");
    Services.obs.removeObserver(this, "Tab:Selected");
    Services.obs.removeObserver(this, "after-viewport-change");
    Services.obs.removeObserver(this, "TextSelection:Move");
    Services.obs.removeObserver(this, "TextSelection:Position");
  },

  observe: function sh_observe(aSubject, aTopic, aData) {
    if (!this._active)
      return;

    switch (aTopic) {
      case "Gesture:SingleTap": {
        let data = JSON.parse(aData);
        this.endSelection(data.x, data.y);
        break;
      }
      case "Tab:Selected":
      case "Window:Resize": {
        // Knowing when the page is done drawing is hard, so let's just cancel
        // the selection when the window changes. We should fix this later.
        this.endSelection();
        break;
      }
      case "after-viewport-change": {
        // Update the cache and reposition the handles after the viewport
        // changes (e.g. panning, zooming).
        this.updateCacheForSelection();
        this.positionHandles();
        break;
      }
      case "TextSelection:Move": {
        let data = JSON.parse(aData);
        this.moveSelection(data.handleType == this.HANDLE_TYPE_START, data.x, data.y);
        break;
      }
      case "TextSelection:Position": {
        let data = JSON.parse(aData);

        // Reverse the handles if necessary.
        let selectionReversed = this.updateCacheForSelection(data.handleType == this.HANDLE_TYPE_START);
        if (selectionReversed) {
          // Re-send mouse events to update the selection corresponding to the new handles.
          if (this._isRTL) {
            this._sendMouseEvents(this.cache.end.x, this.cache.end.y, false);
            this._sendMouseEvents(this.cache.start.x, this.cache.start.y, true);
          } else {
            this._sendMouseEvents(this.cache.start.x, this.cache.start.y, false);
            this._sendMouseEvents(this.cache.end.x, this.cache.end.y, true);
          }
        }

        // Position the handles to align with the edges of the selection.
        this.positionHandles();
        break;
      }
    }
  },

  handleEvent: function sh_handleEvent(aEvent) {
    if (!this._active)
      return;

    switch (aEvent.type) {
      case "pagehide":
        this.endSelection();
        break;
    }
  },

  notifySelectionChanged: function sh_notifySelectionChanged(aDoc, aSel, aReason) {
    // If the selection was removed, call endSelection() to clean up
    if (aSel == "" && aReason == Ci.nsISelectionListener.NO_REASON)
      this.endSelection();
  },

  // aX/aY are in top-level window browser coordinates
  startSelection: function sh_startSelection(aElement, aX, aY) {
    if (this._active) {
      // If the user long tapped on the selection, show a context menu
      if (this._pointInSelection(aX, aY)) {
        this.showContextMenu(aX, aY);
        return;
      }

      // Clear out any existing selection
      this.endSelection();
    }

    // Get the element's view
    this._view = aElement.ownerDocument.defaultView;
    this._view.addEventListener("pagehide", this, false);
    this._isRTL = (this._view.getComputedStyle(aElement, "").direction == "rtl");

    // Remove any previous selected or created ranges. Tapping anywhere on a
    // page will create an empty range.
    let selection = this._view.getSelection();
    selection.removeAllRanges();

    // Position the caret using a fake mouse click sent to the top-level window
    this._sendMouseEvents(aX, aY, false);

    try {
      let selectionController = this._view.QueryInterface(Ci.nsIInterfaceRequestor).
                                           getInterface(Ci.nsIWebNavigation).
                                           QueryInterface(Ci.nsIInterfaceRequestor).
                                           getInterface(Ci.nsISelectionDisplay).
                                           QueryInterface(Ci.nsISelectionController);

      // Select the word nearest the caret
      selectionController.wordMove(false, false);

      // Move forward in LTR, backward in RTL
      selectionController.wordMove(!this._isRTL, true);
    } catch(e) {
      // If we couldn't select the word at the given point, bail
      Cu.reportError("Error selecting word: " + e);
      return;
    }

    // If there isn't an appropriate selection, bail
    if (!selection.rangeCount || !selection.getRangeAt(0) || !selection.toString().trim().length) {
      selection.collapseToStart();
      return;
    }

    // Add a listener to end the selection if it's removed programatically
    selection.QueryInterface(Ci.nsISelectionPrivate).addSelectionListener(this);

    // Initialize the cache
    this.cache = { start: {}, end: {}};
    this.updateCacheForSelection();

    this.showHandles();
    this._active = true;
  },

  showContextMenu: function sh_showContextMenu(aX, aY) {
    let [SELECT_ALL, COPY, SHARE] = [0, 1, 2];
    let listitems = [
      { label: Strings.browser.GetStringFromName("contextmenu.selectAll"), id: SELECT_ALL },
      { label: Strings.browser.GetStringFromName("contextmenu.copy"), id: COPY },
      { label: Strings.browser.GetStringFromName("contextmenu.share"), id: SHARE }
    ];

    let msg = {
      gecko: {
        type: "Prompt:Show",
        title: "",
        listitems: listitems
      }
    };
    let id = JSON.parse(sendMessageToJava(msg)).button;

    switch (id) {
      case SELECT_ALL: {
        let selectionController = this._view.QueryInterface(Ci.nsIInterfaceRequestor).
                                             getInterface(Ci.nsIWebNavigation).
                                             QueryInterface(Ci.nsIInterfaceRequestor).
                                             getInterface(Ci.nsISelectionDisplay).
                                             QueryInterface(Ci.nsISelectionController);
        selectionController.selectAll();
        this.updateCacheForSelection();
        this.positionHandles();
        break;
      }
      case COPY: {
        // Passing coordinates to endSelection takes care of copying for us
        this.endSelection(aX, aY);
        break;
      }
      case SHARE: {
        let selectedText = this.endSelection();
        sendMessageToJava({
          gecko: {
            type: "Share:Text",
            text: selectedText
          }
        });
        break;
      }
    }
  },

  // Moves the ends of the selection in the page. aX/aY are in top-level window
  // browser coordinates.
  moveSelection: function sh_moveSelection(aIsStartHandle, aX, aY) {
    // Update the handle position as it's dragged.
    if (aIsStartHandle) {
      this.cache.start.x = aX;
      this.cache.start.y = aY;
    } else {
      this.cache.end.x = aX;
      this.cache.end.y = aY;
    }

    // The handles work the same on both LTR and RTL pages, but the underlying selection
    // works differently, so we need to reverse how we send mouse events on RTL pages.
    if (this._isRTL) {
      // Position the caret at the end handle using a fake mouse click
      if (!aIsStartHandle)
        this._sendMouseEvents(this.cache.end.x, this.cache.end.y, false);

      // Selects text between the carat and the start handle using a fake shift+click
      this._sendMouseEvents(this.cache.start.x, this.cache.start.y, true);
    } else {
      // Position the caret at the start handle using a fake mouse click
      if (aIsStartHandle)
        this._sendMouseEvents(this.cache.start.x, this.cache.start.y, false);

      // Selects text between the carat and the end handle using a fake shift+click
      this._sendMouseEvents( this.cache.end.x, this.cache.end.y, true);
    }
  },

  _sendMouseEvents: function sh_sendMouseEvents(aX, aY, useShift) {
    // Send mouse event 1px too high to prevent selection from entering the line below where it should be
    this._cwu.sendMouseEventToWindow("mousedown", aX, aY - 1, 0, 0, useShift ? Ci.nsIDOMNSEvent.SHIFT_MASK : 0, true);
    this._cwu.sendMouseEventToWindow("mouseup", aX, aY - 1, 0, 0, useShift ? Ci.nsIDOMNSEvent.SHIFT_MASK : 0, true);
  },

  // aX/aY are in top-level window browser coordinates
  endSelection: function sh_endSelection(aX, aY) {
    if (!this._active)
      return;

    this._active = false;
    this.hideHandles();

    let selectedText = "";
    if (this._view) {
      let selection = this._view.getSelection();
      if (selection) {
        // Get the text to copy if the tap is in the selection
        if (arguments.length == 2 && this._pointInSelection(aX, aY))
          selectedText = selection.toString().trim();

        selection.removeAllRanges();
        selection.QueryInterface(Ci.nsISelectionPrivate).removeSelectionListener(this);
      }
    }

    // Only try copying text if there's text to copy!
    if (selectedText.length) {
      let element = ElementTouchHelper.anyElementFromPoint(BrowserApp.selectedBrowser.contentWindow, aX, aY);
      // Only try copying text if the tap happens in the same view
      if (element.ownerDocument.defaultView == this._view) {
        let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
        clipboard.copyString(selectedText, element.ownerDocument);
        NativeWindow.toast.show(Strings.browser.GetStringFromName("selectionHelper.textCopied"), "short");
      }
    }

    this._view.removeEventListener("pagehide", this, false);
    this._view = null;
    this._isRTL = false;
    this.cache = null;

    return selectedText;
  },

  _getViewOffset: function sh_getViewOffset() {
    let offset = { x: 0, y: 0 };
    let win = this._view;

    // Recursively look through frames to compute the total position offset.
    while (win.frameElement) {
      let rect = win.frameElement.getBoundingClientRect();
      offset.x += rect.left;
      offset.y += rect.top;

      win = win.parent;
    }

    return offset;
  },

  _pointInSelection: function sh_pointInSelection(aX, aY) {
    let offset = this._getViewOffset();
    let rangeRect = this._view.getSelection().getRangeAt(0).getBoundingClientRect();
    let radius = ElementTouchHelper.getTouchRadius();
    return (aX - offset.x > rangeRect.left - radius.left &&
            aX - offset.x < rangeRect.right + radius.right &&
            aY - offset.y > rangeRect.top - radius.top &&
            aY - offset.y < rangeRect.bottom + radius.bottom);
  },

  // Returns true if the selection has been reversed. Takes optional aIsStartHandle
  // param to decide whether the selection has been reversed.
  updateCacheForSelection: function sh_updateCacheForSelection(aIsStartHandle) {
    let rects = this._view.getSelection().getRangeAt(0).getClientRects();
    let start = { x: rects[0].left, y: rects[0].bottom };
    let end = { x: rects[rects.length - 1].right, y: rects[rects.length - 1].bottom };

    let selectionReversed = false;
    if (this.cache.start) {
      // If the end moved past the old end, but we're dragging the start handle, then that handle should become the end handle (and vice versa)
      selectionReversed = (aIsStartHandle && (end.y > this.cache.end.y || (end.y == this.cache.end.y && end.x > this.cache.end.x))) ||
                          (!aIsStartHandle && (start.y < this.cache.start.y || (start.y == this.cache.start.y && start.x < this.cache.start.x)));
    }

    this.cache.start = start;
    this.cache.end = end;

    return selectionReversed;
  },

  positionHandles: function sh_positionHandles() {
    // Translate coordinates to account for selections in sub-frames. We can't cache
    // this because the top-level page may have scrolled since selection started.
    let offset = this._getViewOffset();
    sendMessageToJava({
      gecko: {
        type: "TextSelection:PositionHandles",
        startLeft: this.cache.start.x + offset.x,
        startTop: this.cache.start.y + offset.y,
        endLeft: this.cache.end.x + offset.x,
        endTop: this.cache.end.y + offset.y
      }
    });
  },

  showHandles: function sh_showHandles() {
    this.positionHandles();

    sendMessageToJava({
      gecko: {
        type: "TextSelection:ShowHandles"
      }
    });
  },

  hideHandles: function sh_hideHandles() {
    sendMessageToJava({
      gecko: {
        type: "TextSelection:HideHandles"
      }
    });
  }
};


var UserAgent = {
  DESKTOP_UA: null,

  init: function ua_init() {
    Services.obs.addObserver(this, "DesktopMode:Change", false);
    Services.obs.addObserver(this, "http-on-modify-request", false);

    // See https://developer.mozilla.org/en/Gecko_user_agent_string_reference
    this.DESKTOP_UA = Cc["@mozilla.org/network/protocol;1?name=http"]
                        .getService(Ci.nsIHttpProtocolHandler).userAgent
                        .replace(/Android; [a-zA-Z]+/, "X11; Linux x86_64")
                        .replace(/Gecko\/[0-9\.]+/, "Gecko/20100101");
  },

  uninit: function ua_uninit() {
    Services.obs.removeObserver(this, "DesktopMode:Change");
    Services.obs.removeObserver(this, "http-on-modify-request");
  },

  _getRequestLoadContext: function ua_getRequestLoadContext(aRequest) {
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

  _getWindowForRequest: function ua_getWindowForRequest(aRequest) {
    let loadContext = this._getRequestLoadContext(aRequest);
    if (loadContext)
      return loadContext.associatedWindow;
    return null;
  },

  observe: function ua_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "DesktopMode:Change": {
        let args = JSON.parse(aData);
        let tab = BrowserApp.getTabForId(args.tabId);
        if (tab != null)
          tab.reloadWithMode(args.desktopMode);
        break;
      }
      case "http-on-modify-request": {
        let channel = aSubject.QueryInterface(Ci.nsIHttpChannel);
        let channelWindow = this._getWindowForRequest(channel);
        let tab = BrowserApp.getTabForWindow(channelWindow);
        if (tab == null)
          break;

        if (channel.URI.host.indexOf("youtube") != -1) {
          let ua = Cc["@mozilla.org/network/protocol;1?name=http"].getService(Ci.nsIHttpProtocolHandler).userAgent;
#expand let version = "__MOZ_APP_VERSION__";
          ua += " Fennec/" + version;
          channel.setRequestHeader("User-Agent", ua, false);
        }

        // Send desktop UA if "Request Desktop Site" is enabled
        if (tab.desktopMode)
          channel.setRequestHeader("User-Agent", this.DESKTOP_UA, false);

        break;
      }
    }
  }
};


function nsBrowserAccess() {
}

nsBrowserAccess.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBrowserDOMWindow]),

  _getBrowser: function _getBrowser(aURI, aOpener, aWhere, aContext) {
    let isExternal = (aContext == Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);
    if (isExternal && aURI && aURI.schemeIs("chrome"))
      return null;

    let loadflags = isExternal ?
                      Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL :
                      Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW) {
      switch (aContext) {
        case Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL:
          aWhere = Services.prefs.getIntPref("browser.link.open_external");
          break;
        default: // OPEN_NEW or an illegal value
          aWhere = Services.prefs.getIntPref("browser.link.open_newwindow");
      }
    }

    Services.io.offline = false;

    let referrer;
    if (aOpener) {
      try {
        let location = aOpener.location;
        referrer = Services.io.newURI(location, null, null);
      } catch(e) { }
    }

    let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
    let pinned = false;

    if (aURI && aWhere == Ci.nsIBrowserDOMWindow.OPEN_SWITCHTAB) {
      pinned = true;
      let spec = aURI.spec;
      let tabs = BrowserApp.tabs;
      for (let i = 0; i < tabs.length; i++) {
        let appOrigin = ss.getTabValue(tabs[i], "appOrigin");
        if (appOrigin == spec) {
          let tab = tabs[i];
          BrowserApp.selectTab(tab);
          return tab.browser;
        }
      }
    }

    let newTab = (aWhere == Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW ||
                  aWhere == Ci.nsIBrowserDOMWindow.OPEN_NEWTAB ||
                  aWhere == Ci.nsIBrowserDOMWindow.OPEN_SWITCHTAB);

    if (newTab) {
      let parentId = -1;
      if (!isExternal && aOpener) {
        let parent = BrowserApp.getTabForWindow(aOpener.top);
        if (parent)
          parentId = parent.id;
      }

      // BrowserApp.addTab calls loadURIWithFlags with the appropriate params
      let tab = BrowserApp.addTab(aURI ? aURI.spec : "about:blank", { flags: loadflags,
                                                                      referrerURI: referrer,
                                                                      external: isExternal,
                                                                      parentId: parentId,
                                                                      selected: true,
                                                                      pinned: pinned });

      return tab.browser;
    }

    // OPEN_CURRENTWINDOW and illegal values
    let browser = BrowserApp.selectedBrowser;
    if (aURI && browser)
      browser.loadURIWithFlags(aURI.spec, loadflags, referrer, null, null);

    return browser;
  },

  openURI: function browser_openURI(aURI, aOpener, aWhere, aContext) {
    let browser = this._getBrowser(aURI, aOpener, aWhere, aContext);
    return browser ? browser.contentWindow : null;
  },

  openURIInFrame: function browser_openURIInFrame(aURI, aOpener, aWhere, aContext) {
    let browser = this._getBrowser(aURI, aOpener, aWhere, aContext);
    return browser ? browser.QueryInterface(Ci.nsIFrameLoaderOwner) : null;
  },

  isTabContentWindow: function(aWindow) {
    return BrowserApp.getBrowserForWindow(aWindow) != null;
  }
};


let gTabIDFactory = 0;

// track the last known screen size so that new tabs
// get created with the right size rather than being 1x1
let gScreenWidth = 1;
let gScreenHeight = 1;

function Tab(aURL, aParams) {
  this.browser = null;
  this.id = 0;
  this.showProgress = true;
  this._zoom = 1.0;
  this._drawZoom = 1.0;
  this.userScrollPos = { x: 0, y: 0 };
  this.contentDocumentIsDisplayed = true;
  this.pluginDoorhangerTimeout = null;
  this.shouldShowPluginDoorhanger = true;
  this.clickToPlayPluginsActivated = false;
  this.desktopMode = false;
  this.originalURI = null;

  this.create(aURL, aParams);
}

Tab.prototype = {
  create: function(aURL, aParams) {
    if (this.browser)
      return;

    aParams = aParams || {};

    this.browser = document.createElement("browser");
    this.browser.setAttribute("type", "content-targetable");
    this.setBrowserSize(kDefaultCSSViewportWidth, kDefaultCSSViewportHeight);
    BrowserApp.deck.appendChild(this.browser);

    // Must be called after appendChild so the docshell has been created.
    this.setActive(false);

    this.browser.stop();

    let frameLoader = this.browser.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader;
    frameLoader.renderMode = Ci.nsIFrameLoader.RENDER_MODE_ASYNC_SCROLL;

    // only set tab uri if uri is valid
    let uri = null;
    try {
      uri = Services.io.newURI(aURL, null, null).spec;
    } catch (e) {}

    this.id = ++gTabIDFactory;
    this.desktopMode = ("desktopMode" in aParams) ? aParams.desktopMode : false;

    let message = {
      gecko: {
        type: "Tab:Added",
        tabID: this.id,
        uri: uri,
        parentId: ("parentId" in aParams) ? aParams.parentId : -1,
        external: ("external" in aParams) ? aParams.external : false,
        selected: ("selected" in aParams) ? aParams.selected : true,
        title: aParams.title || aURL,
        delayLoad: aParams.delayLoad || false,
        desktopMode: this.desktopMode
      }
    };
    sendMessageToJava(message);

    this.overscrollController = new OverscrollController(this);
    this.browser.contentWindow.controllers.insertControllerAt(0, this.overscrollController);

    let flags = Ci.nsIWebProgress.NOTIFY_STATE_ALL |
                Ci.nsIWebProgress.NOTIFY_LOCATION |
                Ci.nsIWebProgress.NOTIFY_SECURITY;
    this.browser.addProgressListener(this, flags);
    this.browser.sessionHistory.addSHistoryListener(this);

    this.browser.addEventListener("DOMContentLoaded", this, true);
    this.browser.addEventListener("DOMLinkAdded", this, true);
    this.browser.addEventListener("DOMTitleChanged", this, true);
    this.browser.addEventListener("DOMWindowClose", this, true);
    this.browser.addEventListener("DOMWillOpenModalDialog", this, true);
    this.browser.addEventListener("scroll", this, true);
    this.browser.addEventListener("MozScrolledAreaChanged", this, true);
    this.browser.addEventListener("PluginClickToPlay", this, true);
    this.browser.addEventListener("pageshow", this, true);

    Services.obs.addObserver(this, "before-first-paint", false);
    Services.prefs.addObserver("browser.ui.zoom.force-user-scalable", this, false);

    if (!aParams.delayLoad) {
      let flags = "flags" in aParams ? aParams.flags : Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
      let postData = ("postData" in aParams && aParams.postData) ? aParams.postData.value : null;
      let referrerURI = "referrerURI" in aParams ? aParams.referrerURI : null;
      let charset = "charset" in aParams ? aParams.charset : null;

      // This determines whether or not we show the progress throbber in the urlbar
      this.showProgress = "showProgress" in aParams ? aParams.showProgress : true;

      try {
        this.browser.loadURIWithFlags(aURL, flags, referrerURI, charset, postData);
      } catch(e) {
        let message = {
          gecko: {
            type: "Content:LoadError",
            tabID: this.id,
            uri: this.browser.currentURI.spec,
            title: this.browser.contentTitle
          }
        };
        sendMessageToJava(message);
        dump("Handled load error: " + e)
      }
    }
  },

  /** 
   * Reloads the tab with the desktop mode setting.
   */
  reloadWithMode: function (aDesktopMode) {
    // Set desktop mode for tab and send change to Java
    if (this.desktopMode != aDesktopMode) {
      this.desktopMode = aDesktopMode;
      sendMessageToJava({
        gecko: {
          type: "DesktopMode:Changed",
          desktopMode: aDesktopMode,
          tabId: this.id
        }
      });
    }

    // Only reload the page for http/https schemes
    let currentURI = this.browser.currentURI;
    if (!currentURI.schemeIs("http") && !currentURI.schemeIs("https"))
      return;

    let url = currentURI.spec;
    let flags = Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
    if (this.originalURI && !this.originalURI.equals(currentURI)) {
      // We were redirected; reload the original URL
      url = this.originalURI.spec;
      flags |= Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY;
    } else {
      // Many sites use mobile-specific URLs, such as:
      //   http://m.yahoo.com
      //   http://www.google.com/m
      // If the user clicks "Request Desktop Site" while on a mobile site, it
      // will appear to do nothing since the mobile URL is still being
      // requested. To address this, we do the following:
      //   1) Remove the path from the URL (http://www.google.com/m?q=query -> http://www.google.com)
      //   2) If a host subdomain is "m", remove it (http://en.m.wikipedia.org -> http://en.wikipedia.org)
      // This means the user is sent to site's home page, but this is better
      // than the setting having no effect at all.
      if (aDesktopMode)
        url = currentURI.prePath.replace(/([\/\.])m\./g, "$1");
      else
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY;
    }

    this.browser.docShell.loadURI(url, flags, null, null, null);
  },

  destroy: function() {
    if (!this.browser)
      return;

    this.browser.contentWindow.controllers.removeController(this.overscrollController);

    this.browser.removeProgressListener(this);
    this.browser.removeEventListener("DOMContentLoaded", this, true);
    this.browser.removeEventListener("DOMLinkAdded", this, true);
    this.browser.removeEventListener("DOMTitleChanged", this, true);
    this.browser.removeEventListener("DOMWindowClose", this, true);
    this.browser.removeEventListener("DOMWillOpenModalDialog", this, true);
    this.browser.removeEventListener("scroll", this, true);
    this.browser.removeEventListener("PluginClickToPlay", this, true);
    this.browser.removeEventListener("MozScrolledAreaChanged", this, true);

    Services.obs.removeObserver(this, "before-first-paint");
    Services.prefs.removeObserver("browser.ui.zoom.force-user-scalable", this);

    // Make sure the previously selected panel remains selected. The selected panel of a deck is
    // not stable when panels are removed.
    let selectedPanel = BrowserApp.deck.selectedPanel;
    BrowserApp.deck.removeChild(this.browser);
    BrowserApp.deck.selectedPanel = selectedPanel;

    this.browser = null;
  },

  // This should be called to update the browser when the tab gets selected/unselected
  setActive: function setActive(aActive) {
    if (!this.browser || !this.browser.docShell)
      return;

    if (aActive) {
      this.browser.setAttribute("type", "content-primary");
      this.browser.focus();
      this.browser.docShellIsActive = true;
    } else {
      this.browser.setAttribute("type", "content-targetable");
      this.browser.docShellIsActive = false;
    }
  },

  getActive: function getActive() {
      return this.browser.docShellIsActive;
  },

  setDisplayPort: function(aDisplayPort) {
    let zoom = this._zoom;
    let resolution = aDisplayPort.resolution;
    if (zoom <= 0 || resolution <= 0)
      return;

    // "zoom" is the user-visible zoom of the "this" tab
    // "resolution" is the zoom at which we wish gecko to render "this" tab at
    // these two may be different if we are, for example, trying to render a
    // large area of the page at low resolution because the user is panning real
    // fast.
    // The gecko scroll position is in CSS pixels. The display port rect
    // values (aDisplayPort), however, are in CSS pixels multiplied by the desired
    // rendering resolution. Therefore care must be taken when doing math with
    // these sets of values, to ensure that they are normalized to the same coordinate
    // space first.

    let element = this.browser.contentDocument.documentElement;
    if (!element)
      return;

    // we should never be drawing background tabs at resolutions other than the user-
    // visible zoom. for foreground tabs, however, if we are drawing at some other
    // resolution, we need to set the resolution as specified.
    let cwu = window.top.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    if (BrowserApp.selectedTab == this) {
      if (resolution != this._drawZoom) {
        this._drawZoom = resolution;
        cwu.setResolution(resolution, resolution);
      }
    } else if (resolution != zoom) {
      dump("Warning: setDisplayPort resolution did not match zoom for background tab!");
    }

    // Finally, we set the display port, taking care to convert everything into the CSS-pixel
    // coordinate space, because that is what the function accepts. Also we have to fudge the
    // displayport somewhat to make sure it gets through all the conversions gecko will do on it
    // without deforming too much. See https://bugzilla.mozilla.org/show_bug.cgi?id=737510#c10
    // for details on what these operations are.
    let geckoScrollX = this.browser.contentWindow.scrollX;
    let geckoScrollY = this.browser.contentWindow.scrollY;
    aDisplayPort = this._dirtiestHackEverToWorkAroundGeckoRounding(aDisplayPort, geckoScrollX, geckoScrollY);

    cwu = this.browser.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    cwu.setDisplayPortForElement((aDisplayPort.left / resolution) - geckoScrollX,
                                 (aDisplayPort.top / resolution) - geckoScrollY,
                                 (aDisplayPort.right - aDisplayPort.left) / resolution,
                                 (aDisplayPort.bottom - aDisplayPort.top) / resolution,
                                 element);
  },

  /*
   * Yes, this is ugly. But it's currently the safest way to account for the rounding errors that occur
   * when we pump the displayport coordinates through gecko and they pop out in the compositor.
   *
   * In general, the values are converted from page-relative device pixels to viewport-relative app units,
   * and then back to page-relative device pixels (now as ints). The first half of this is only slightly
   * lossy, but it's enough to throw off the numbers a little. Because of this, when gecko calls
   * ScaleToOutsidePixels to generate the final rect, the rect may get expanded more than it should,
   * ending up a pixel larger than it started off. This is undesirable in general, but specifically
   * bad for tiling, because it means we means we end up painting one line of pixels from a tile,
   * causing an otherwise unnecessary upload of the whole tile.
   *
   * In order to counteract the rounding error, this code simulates the conversions that will happen
   * to the display port, and calculates whether or not that final ScaleToOutsidePixels is actually
   * expanding the rect more than it should. If so, it determines how much rounding error was introduced
   * up until that point, and adjusts the original values to compensate for that rounding error.
   */
  _dirtiestHackEverToWorkAroundGeckoRounding: function(aDisplayPort, aGeckoScrollX, aGeckoScrollY) {
    const APP_UNITS_PER_CSS_PIXEL = 60.0;
    const EXTRA_FUDGE = 0.04;

    let resolution = aDisplayPort.resolution;

    // Some helper functions that simulate conversion processes in gecko

    function cssPixelsToAppUnits(aVal) {
      return Math.floor((aVal * APP_UNITS_PER_CSS_PIXEL) + 0.5);
    }

    function appUnitsToDevicePixels(aVal) {
      return aVal / APP_UNITS_PER_CSS_PIXEL * resolution;
    }

    function devicePixelsToAppUnits(aVal) {
      return cssPixelsToAppUnits(aVal / resolution);
    }

    // Stash our original (desired) displayport width and height away, we need it
    // later and we might modify the displayport in between.
    let originalWidth = aDisplayPort.right - aDisplayPort.left;
    let originalHeight = aDisplayPort.bottom - aDisplayPort.top;

    // This is the first conversion the displayport goes through, going from page-relative
    // device pixels to viewport-relative app units.
    let appUnitDisplayPort = {
      x: cssPixelsToAppUnits((aDisplayPort.left / resolution) - aGeckoScrollX),
      y: cssPixelsToAppUnits((aDisplayPort.top / resolution) - aGeckoScrollY),
      w: cssPixelsToAppUnits((aDisplayPort.right - aDisplayPort.left) / resolution),
      h: cssPixelsToAppUnits((aDisplayPort.bottom - aDisplayPort.top) / resolution)
    };

    // This is the translation gecko applies when converting back from viewport-relative
    // device pixels to page-relative device pixels.
    let geckoTransformX = -Math.floor((-aGeckoScrollX * resolution) + 0.5);
    let geckoTransformY = -Math.floor((-aGeckoScrollY * resolution) + 0.5);

    // The final "left" value as calculated in gecko is:
    //    left = geckoTransformX + Math.floor(appUnitsToDevicePixels(appUnitDisplayPort.x))
    // In a perfect world, this value would be identical to aDisplayPort.left, which is what
    // we started with. However, this may not be the case if the value being floored has accumulated
    // enough error to drop below what it should be.
    // For example, assume geckoTransformX is 0, and aDisplayPort.left is 4, but
    // appUnitsToDevicePixels(appUnitsToDevicePixels.x) comes out as 3.9 because of rounding error.
    // That's bad, because the -0.1 error has caused it to floor to 3 instead of 4. (If it had errored
    // the other way and come out as 4.1, there's no problem). In this example, we need to increase the
    // "left" value by some amount so that the 3.9 actually comes out as >= 4, and it gets floored into
    // the expected value of 4. The delta values calculated below calculate that error amount (e.g. -0.1).
    let errorLeft = (geckoTransformX + appUnitsToDevicePixels(appUnitDisplayPort.x)) - aDisplayPort.left;
    let errorTop = (geckoTransformY + appUnitsToDevicePixels(appUnitDisplayPort.y)) - aDisplayPort.top;

    // If the error was negative, that means it will floor incorrectly, so we need to bump up the
    // original aDisplayPort.left and/or aDisplayPort.top values. The amount we bump it up by is
    // the error amount (increased by a small fudge factor to ensure it's sufficient), converted
    // backwards through the conversion process.
    if (errorLeft < 0) {
      aDisplayPort.left += appUnitsToDevicePixels(devicePixelsToAppUnits(EXTRA_FUDGE - errorLeft));
      // After we modify the left value, we need to re-simulate some values to take that into account
      appUnitDisplayPort.x = cssPixelsToAppUnits((aDisplayPort.left / resolution) - aGeckoScrollX);
      appUnitDisplayPort.w = cssPixelsToAppUnits((aDisplayPort.right - aDisplayPort.left) / resolution);
    }
    if (errorTop < 0) {
      aDisplayPort.top += appUnitsToDevicePixels(devicePixelsToAppUnits(EXTRA_FUDGE - errorTop));
      // After we modify the top value, we need to re-simulate some values to take that into account
      appUnitDisplayPort.y = cssPixelsToAppUnits((aDisplayPort.top / resolution) - aGeckoScrollY);
      appUnitDisplayPort.h = cssPixelsToAppUnits((aDisplayPort.bottom - aDisplayPort.top) / resolution);
    }

    // At this point, the aDisplayPort.left and aDisplayPort.top values have been corrected to account
    // for the error in conversion such that they end up where we want them. Now we need to also do the
    // same for the right/bottom values so that the width/height end up where we want them.

    // This is the final conversion that the displayport goes through before gecko spits it back to
    // us. Note that the width/height calculates are of the form "ceil(transform(right)) - floor(transform(left))"
    let scaledOutDevicePixels = {
      x: Math.floor(appUnitsToDevicePixels(appUnitDisplayPort.x)),
      y: Math.floor(appUnitsToDevicePixels(appUnitDisplayPort.y)),
      w: Math.ceil(appUnitsToDevicePixels(appUnitDisplayPort.x + appUnitDisplayPort.w)) - Math.floor(appUnitsToDevicePixels(appUnitDisplayPort.x)),
      h: Math.ceil(appUnitsToDevicePixels(appUnitDisplayPort.y + appUnitDisplayPort.h)) - Math.floor(appUnitsToDevicePixels(appUnitDisplayPort.y))
    };

    // The final "width" value as calculated in gecko is scaledOutDevicePixels.w.
    // In a perfect world, this would equal originalWidth. However, things are not perfect, and as before,
    // we need to calculate how much rounding error has been introduced. In this case the rounding error is causing
    // the Math.ceil call above to ceiling to the wrong final value. For example, 4 gets converted 4.1 and gets
    // ceiling'd to 5; in this case the error is 0.1.
    let errorRight = (appUnitsToDevicePixels(appUnitDisplayPort.x + appUnitDisplayPort.w) - scaledOutDevicePixels.x) - originalWidth;
    let errorBottom = (appUnitsToDevicePixels(appUnitDisplayPort.y + appUnitDisplayPort.h) - scaledOutDevicePixels.y) - originalHeight;

    // If the error was positive, that means it will ceiling incorrectly, so we need to bump down the
    // original aDisplayPort.right and/or aDisplayPort.bottom. Again, we back-convert the error amount
    // with a small fudge factor to figure out how much to adjust the original values.
    if (errorRight > 0) aDisplayPort.right -= appUnitsToDevicePixels(devicePixelsToAppUnits(errorRight + EXTRA_FUDGE));
    if (errorBottom > 0) aDisplayPort.bottom -= appUnitsToDevicePixels(devicePixelsToAppUnits(errorBottom + EXTRA_FUDGE));

    // Et voila!
    return aDisplayPort;
  },

  setViewport: function(aViewport) {
    // Transform coordinates based on zoom
    let x = aViewport.x / aViewport.zoom;
    let y = aViewport.y / aViewport.zoom;

    // Set scroll position and scroll-port clamping size
    let viewportWidth = gScreenWidth / aViewport.zoom;
    let viewportHeight = gScreenHeight / aViewport.zoom;
    let [pageWidth, pageHeight] = this.getPageSize(this.browser.contentDocument,
                                                   viewportWidth, viewportHeight);
    let scrollPortWidth = Math.min(viewportWidth, pageWidth);
    let scrollPortHeight = Math.min(viewportHeight, pageHeight);

    let win = this.browser.contentWindow;
    win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).
        setScrollPositionClampingScrollPortSize(scrollPortWidth, scrollPortHeight);
    win.scrollTo(x, y);

    this.userScrollPos.x = win.scrollX;
    this.userScrollPos.y = win.scrollY;
    this.setResolution(aViewport.zoom, false);

    if (aViewport.displayPort)
      this.setDisplayPort(aViewport.displayPort);

    Services.obs.notifyObservers(null, "after-viewport-change", "");
  },

  setResolution: function(aZoom, aForce) {
    // Set zoom level
    if (aForce || Math.abs(aZoom - this._zoom) >= 1e-6) {
      this._zoom = aZoom;
      if (BrowserApp.selectedTab == this) {
        let cwu = window.top.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
        this._drawZoom = aZoom;
        cwu.setResolution(aZoom, aZoom);
      }
    }
  },

  getPageSize: function(aDocument, aDefaultWidth, aDefaultHeight) {
    let body = aDocument.body || { scrollWidth: aDefaultWidth, scrollHeight: aDefaultHeight };
    let html = aDocument.documentElement || { scrollWidth: aDefaultWidth, scrollHeight: aDefaultHeight };
    return [Math.max(body.scrollWidth, html.scrollWidth),
      Math.max(body.scrollHeight, html.scrollHeight)];
  },

  getViewport: function() {
    let viewport = {
      width: gScreenWidth,
      height: gScreenHeight,
      cssWidth: gScreenWidth / this._zoom,
      cssHeight: gScreenHeight / this._zoom,
      pageLeft: 0,
      pageTop: 0,
      pageRight: gScreenWidth,
      pageBottom: gScreenHeight,
      // We make up matching css page dimensions
      cssPageLeft: 0,
      cssPageTop: 0,
      cssPageRight: gScreenWidth / this._zoom,
      cssPageBottom: gScreenHeight / this._zoom,
      zoom: this._zoom,
    };

    // Set the viewport offset to current scroll offset
    viewport.cssX = this.browser.contentWindow.scrollX || 0;
    viewport.cssY = this.browser.contentWindow.scrollY || 0;

    // Transform coordinates based on zoom
    viewport.x = Math.round(viewport.cssX * viewport.zoom);
    viewport.y = Math.round(viewport.cssY * viewport.zoom);

    let doc = this.browser.contentDocument;
    if (doc != null) {
      let cwu = this.browser.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      let cssPageRect = cwu.getRootBounds();

      /*
       * Avoid sending page sizes of less than screen size before we hit DOMContentLoaded, because
       * this causes the page size to jump around wildly during page load. After the page is loaded,
       * send updates regardless of page size; we'll zoom to fit the content as needed.
       *
       * In the check below, we floor the viewport size because there might be slight rounding errors
       * introduced in the CSS page size due to the conversion to and from app units in Gecko. The
       * error should be no more than one app unit so doing the floor is overkill, but safe in the
       * sense that the extra page size updates that get sent as a result will be mostly harmless.
       */
      let pageLargerThanScreen = (cssPageRect.width >= Math.floor(viewport.cssWidth))
                              && (cssPageRect.height >= Math.floor(viewport.cssHeight));
      if (doc.readyState === 'complete' || pageLargerThanScreen) {
        viewport.cssPageLeft = cssPageRect.left;
        viewport.cssPageTop = cssPageRect.top;
        viewport.cssPageRight = cssPageRect.right;
        viewport.cssPageBottom = cssPageRect.bottom;
        /* Transform the page width and height based on the zoom factor. */
        viewport.pageLeft = (viewport.cssPageLeft * viewport.zoom);
        viewport.pageTop = (viewport.cssPageTop * viewport.zoom);
        viewport.pageRight = (viewport.cssPageRight * viewport.zoom);
        viewport.pageBottom = (viewport.cssPageBottom * viewport.zoom);
      }
    }

    return viewport;
  },

  sendViewportUpdate: function(aPageSizeUpdate) {
    let message;
    // for foreground tabs, send the viewport update unless the document
    // displayed is different from the content document. In that case, just
    // calculate the display port.
    if (BrowserApp.selectedTab == this && BrowserApp.isBrowserContentDocumentDisplayed()) {
      message = this.getViewport();
      message.type = aPageSizeUpdate ? "Viewport:PageSize" : "Viewport:Update";
    } else {
      // for background tabs, request a new display port calculation, so that
      // when we do switch to that tab, we have the correct display port and
      // don't need to draw twice (once to allow the first-paint viewport to
      // get to java, and again once java figures out the display port).
      message = this.getViewport();
      message.type = "Viewport:CalculateDisplayPort";
    }
    let displayPort = sendMessageToJava({ gecko: message });
    if (displayPort != null)
      this.setDisplayPort(JSON.parse(displayPort));
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "DOMContentLoaded": {
        let target = aEvent.originalTarget;

        // ignore on frames
        if (target.defaultView != this.browser.contentWindow)
          return;

        // Sample the background color of the page and pass it along. (This is used to draw the
        // checkerboard.) Right now we don't detect changes in the background color after this
        // event fires; it's not clear that doing so is worth the effort.
        var backgroundColor = null;
        try {
          let browser = BrowserApp.selectedBrowser;
          if (browser) {
            let { contentDocument, contentWindow } = browser;
            let computedStyle = contentWindow.getComputedStyle(contentDocument.body);
            backgroundColor = computedStyle.backgroundColor;
          }
        } catch (e) {
          // Ignore. Catching and ignoring exceptions here ensures that Talos succeeds.
        }

        sendMessageToJava({
          gecko: {
            type: "DOMContentLoaded",
            tabID: this.id,
            bgColor: backgroundColor
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
          }.bind(this), true);
        }
        break;
      }

      case "DOMLinkAdded": {
        let target = aEvent.originalTarget;
        if (!target.href || target.disabled)
          return;

        // ignore on frames
        if (target.ownerDocument.defaultView != this.browser.contentWindow)
          return;

        // sanitize the rel string
        let list = [];
        if (target.rel) {
          list = target.rel.toLowerCase().split(/\s+/);
          let hash = {};
          list.forEach(function(value) { hash[value] = true; });
          list = [];
          for (let rel in hash)
            list.push("[" + rel + "]");
        }

        // We want to get the largest icon size possible for our UI.
        let maxSize = 0;

        // We use the sizes attribute if available
        // see http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#rel-icon
        if (target.hasAttribute("sizes")) {
          let sizes = target.getAttribute("sizes").toLowerCase();

          if (sizes == "any") {
            // Since Java expects an integer, use -1 to represent icons with sizes="any"
            maxSize = -1; 
          } else {
            let tokens = sizes.split(" ");
            tokens.forEach(function(token) {
              // TODO: check for invalid tokens
              let [w, h] = token.split("x");
              maxSize = Math.max(maxSize, Math.max(w, h));
            });
          }
        }

        let json = {
          type: "DOMLinkAdded",
          tabID: this.id,
          href: resolveGeckoURI(target.href),
          charset: target.ownerDocument.characterSet,
          title: target.title,
          rel: list.join(" "),
          size: maxSize
        };

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
            title: aEvent.target.title.substring(0, 255)
          }
        });
        break;
      }

      case "DOMWindowClose": {
        if (!aEvent.isTrusted)
          return;

        // Find the relevant tab, and close it from Java
        if (this.browser.contentWindow == aEvent.target) {
          aEvent.preventDefault();

          sendMessageToJava({
            gecko: {
              type: "DOMWindowClose",
              tabID: this.id
            }
          });
        }
        break;
      }

      case "DOMWillOpenModalDialog": {
        if (!aEvent.isTrusted)
          return;

        // We're about to open a modal dialog, make sure the opening
        // tab is brought to the front.
        let tab = BrowserApp.getTabForWindow(aEvent.target.top);
        BrowserApp.selectTab(tab);
        break;
      }

      case "scroll": {
        let win = this.browser.contentWindow;
        if (this.userScrollPos.x != win.scrollX || this.userScrollPos.y != win.scrollY) {
          this.sendViewportUpdate();
        }
        break;
      }

      case "MozScrolledAreaChanged": {
        // This event is only fired for root scroll frames, and only when the
        // scrolled area has actually changed, so no need to check for that.
        // Just make sure it's the event for the correct root scroll frame.
        if (aEvent.originalTarget != this.browser.contentDocument)
          return;

        this.sendViewportUpdate(true);
        break;
      }

      case "PluginClickToPlay": {
        let plugin = aEvent.target;

        // Check if plugins have already been activated for this page, or if the user
        // has set a permission to always play plugins on the site
        if (this.clickToPlayPluginsActivated ||
            Services.perms.testPermission(this.browser.currentURI, "plugins") == Services.perms.ALLOW_ACTION) {
          PluginHelper.playPlugin(plugin);
          return;
        }

        // Force a style flush, so that we ensure our binding is attached.
        plugin.clientTop;

        // If the plugin is hidden, or if the overlay is too small, show a doorhanger notification
        let overlay = plugin.ownerDocument.getAnonymousElementByAttribute(plugin, "class", "mainBox");
        if (!overlay || PluginHelper.isTooSmall(plugin, overlay)) {
          // To avoid showing the doorhanger if there are also visible plugin overlays on the page,
          // delay showing the doorhanger to check if visible plugins get added in the near future.
          if (!this.pluginDoorhangerTimeout) {
            this.pluginDoorhangerTimeout = setTimeout(function() {
              if (this.shouldShowPluginDoorhanger)
                PluginHelper.showDoorHanger(this);
            }.bind(this), 500);
          }

          // No overlay? We're done here.
          if (!overlay)
            return;

        } else {
          // There's a large enough visible overlay that we don't need to show the doorhanger.
          this.shouldShowPluginDoorhanger = false;
        }

        // Add click to play listener to the overlay
        overlay.addEventListener("click", function(e) {
          if (e) {
            if (!e.isTrusted)
              return;
            e.preventDefault();
          }
          let win = e.target.ownerDocument.defaultView.top;
          let tab = BrowserApp.getTabForWindow(win);
          tab.clickToPlayPluginsActivated = true;
          PluginHelper.playAllPlugins(win);

          NativeWindow.doorhanger.hide("ask-to-play-plugins", tab.id);
        }, true);
        break;
      }

      case "pageshow": {
        // only send pageshow for the top-level document
        if (aEvent.originalTarget.defaultView != this.browser.contentWindow)
          return;

        sendMessageToJava({
          gecko: {
            type: "Content:PageShow",
            tabID: this.id
          }
        });

        // Once document is fully loaded, we can do a readability check to
        // possibly enable reader mode for this page
        Reader.checkTabReadability(this.id, function(isReadable) {
          if (!isReadable)
            return;

          sendMessageToJava({
            gecko: {
              type: "Content:ReaderEnabled",
              tabID: this.id
            }
          });
        }.bind(this));
      }
    }
  },

  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    let contentWin = aWebProgress.DOMWindow;
    if (contentWin != contentWin.top)
        return;

    // Filter optimization: Only really send NETWORK state changes to Java listener
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
      if ((aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) && aWebProgress.isLoadingDocument) {
        // We may receive a document stop event while a document is still loading
        // (such as when doing URI fixup). Don't notify Java UI in these cases.
        return;
      }

      // Check to see if we restoring the content from a previous presentation (session)
      // since there should be no real network activity
      let restoring = aStateFlags & Ci.nsIWebProgressListener.STATE_RESTORING;
      let showProgress = restoring ? false : this.showProgress;

      // true if the page loaded successfully (i.e., no 404s or other errors)
      let success = false; 
      let uri = "";
      try {
        // Remember original URI for UA changes on redirected pages
        this.originalURI = aRequest.QueryInterface(Components.interfaces.nsIChannel).originalURI;

        if (this.originalURI != null)
          uri = this.originalURI.spec;
      } catch (e) { }
      try {
        success = aRequest.QueryInterface(Components.interfaces.nsIHttpChannel).requestSucceeded;
      } catch (e) { }

      let message = {
        gecko: {
          type: "Content:StateChange",
          tabID: this.id,
          uri: uri,
          state: aStateFlags,
          showProgress: showProgress,
          success: success
        }
      };
      sendMessageToJava(message);

      // Reset showProgress after state change
      this.showProgress = true;
    }
  },

  onLocationChange: function(aWebProgress, aRequest, aLocationURI, aFlags) {
    let contentWin = aWebProgress.DOMWindow;
    if (contentWin != contentWin.top)
        return;

    this._hostChanged = true;

    let fixedURI = aLocationURI;
    try {
      fixedURI = URIFixup.createExposableURI(aLocationURI);
    } catch (ex) { }

    let documentURI = contentWin.document.documentURIObject.spec;
    let contentType = contentWin.document.contentType;
    
    // If fixedURI matches browser.lastURI, we assume this isn't a real location
    // change but rather a spurious addition like a wyciwyg URI prefix. See Bug 747883.
    // Note that we have to ensure fixedURI is not the same as aLocationURI so we
    // don't false-positive page reloads as spurious additions.
    let sameDocument = (aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) != 0 ||
                       ((this.browser.lastURI != null) && fixedURI.equals(this.browser.lastURI) && !fixedURI.equals(aLocationURI));
    this.browser.lastURI = fixedURI;

    // Reset state of click-to-play plugin notifications.
    clearTimeout(this.pluginDoorhangerTimeout);
    this.pluginDoorhangerTimeout = null;
    this.shouldShowPluginDoorhanger = true;
    this.clickToPlayPluginsActivated = false;

    let message = {
      gecko: {
        type: "Content:LocationChange",
        tabID: this.id,
        uri: fixedURI.spec,
        documentURI: documentURI,
        contentType: contentType,
        sameDocument: sameDocument
      }
    };

    sendMessageToJava(message);

    if (!sameDocument) {
      // XXX This code assumes that this is the earliest hook we have at which
      // browser.contentDocument is changed to the new document we're loading
      this.contentDocumentIsDisplayed = false;
    } else {
      this.sendViewportUpdate();
    }
  },

  // Properties used to cache security state used to update the UI
  _state: null,
  _hostChanged: false, // onLocationChange will flip this bit

  onSecurityChange: function(aWebProgress, aRequest, aState) {
    // Don't need to do anything if the data we use to update the UI hasn't changed
    if (this._state == aState && !this._hostChanged)
      return;

    this._state = aState;
    this._hostChanged = false;

    let identity = IdentityHandler.checkIdentity(aState, this.browser);

    let message = {
      gecko: {
        type: "Content:SecurityChange",
        tabID: this.id,
        identity: identity
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
    this._sendHistoryEvent("Purge", aNumEntries, null);
    return true;
  },

  get metadata() {
    return ViewportHandler.getMetadataForDocument(this.browser.contentDocument);
  },

  /** Update viewport when the metadata changes. */
  updateViewportMetadata: function updateViewportMetadata(aMetadata) {
    if (Services.prefs.getBoolPref("browser.ui.zoom.force-user-scalable")) {
      aMetadata.allowZoom = true;
      aMetadata.minZoom = aMetadata.maxZoom = NaN;
    }
    if (aMetadata && aMetadata.autoScale) {
      let scaleRatio = aMetadata.scaleRatio = ViewportHandler.getScaleRatio();

      if ("defaultZoom" in aMetadata && aMetadata.defaultZoom > 0)
        aMetadata.defaultZoom *= scaleRatio;
      if ("minZoom" in aMetadata && aMetadata.minZoom > 0)
        aMetadata.minZoom *= scaleRatio;
      if ("maxZoom" in aMetadata && aMetadata.maxZoom > 0)
        aMetadata.maxZoom *= scaleRatio;
    }
    ViewportHandler.setMetadataForDocument(this.browser.contentDocument, aMetadata);
    this.updateViewportSize(gScreenWidth);
    this.sendViewportMetadata();
  },

  /** Update viewport when the metadata or the window size changes. */
  updateViewportSize: function updateViewportSize(aOldScreenWidth) {
    // When this function gets called on window resize, we must execute
    // this.sendViewportUpdate() so that refreshDisplayPort is called.
    // Ensure that when making changes to this function that code path
    // is not accidentally removed (the call to sendViewportUpdate() is
    // at the very end).

    let browser = this.browser;
    if (!browser)
      return;

    let screenW = gScreenWidth;
    let screenH = gScreenHeight;
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
    // the page is zoomed out to show its full width. Note that before
    // we set the viewport width, the "full width" of the page isn't properly
    // defined, so that's why we have to call setBrowserSize twice - once
    // to set the width, and the second time to figure out the height based
    // on the layout at that width.
    let oldBrowserWidth = this.browserWidth;
    this.setBrowserSize(viewportW, viewportH);
    let minScale = 1.0;
    if (this.browser.contentDocument) {
      // this may get run during a Viewport:Change message while the document
      // has not yet loaded, so need to guard against a null document.
      let [pageWidth, pageHeight] = this.getPageSize(this.browser.contentDocument, viewportW, viewportH);
      minScale = gScreenWidth / pageWidth;
    }
    minScale = this.clampZoom(minScale);
    viewportH = Math.max(viewportH, screenH / minScale);
    this.setBrowserSize(viewportW, viewportH);

    // Avoid having the scroll position jump around after device rotation.
    let win = this.browser.contentWindow;
    this.userScrollPos.x = win.scrollX;
    this.userScrollPos.y = win.scrollY;

    // This change to the zoom accounts for all types of changes I can conceive:
    // 1. screen size changes, CSS viewport does not (pages with no meta viewport
    //    or a fixed size viewport)
    // 2. screen size changes, CSS viewport also does (pages with a device-width
    //    viewport)
    // 3. screen size remains constant, but CSS viewport changes (meta viewport
    //    tag is added or removed)
    // 4. neither screen size nor CSS viewport changes
    //
    // In all of these cases, we maintain how much actual content is visible
    // within the screen width. Note that "actual content" may be different
    // with respect to CSS pixels because of the CSS viewport size changing.
    let zoomScale = (screenW * oldBrowserWidth) / (aOldScreenWidth * viewportW);
    let zoom = this.clampZoom(this._zoom * zoomScale);
    this.setResolution(zoom, false);
    this.sendViewportUpdate();
  },

  sendViewportMetadata: function sendViewportMetadata() {
    sendMessageToJava({ gecko: {
      type: "Tab:ViewportMetadata",
      allowZoom: this.metadata.allowZoom,
      defaultZoom: this.metadata.defaultZoom || 0,
      minZoom: this.metadata.minZoom || 0,
      maxZoom: this.metadata.maxZoom || 0,
      tabID: this.id
    }});
  },

  setBrowserSize: function(aWidth, aHeight) {
    this.browserWidth = aWidth;

    if (!this.browser.contentWindow)
      return;
    let cwu = this.browser.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    cwu.setCSSViewport(aWidth, aHeight);
  },

  /** Takes a scale and restricts it based on this tab's zoom limits. */
  clampZoom: function clampZoom(aZoom) {
    let zoom = ViewportHandler.clamp(aZoom, kViewportMinScale, kViewportMaxScale);

    let md = this.metadata;
    if (!md.allowZoom)
      return md.defaultZoom || zoom;

    if (md && md.minZoom)
      zoom = Math.max(zoom, md.minZoom);
    if (md && md.maxZoom)
      zoom = Math.min(zoom, md.maxZoom);
    return zoom;
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "before-first-paint":
        // Is it on the top level?
        let contentDocument = aSubject;
        if (contentDocument == this.browser.contentDocument) {
          // reset CSS viewport and zoom to default on new page, and then calculate
          // them properly using the actual metadata from the page. note that the
          // updateMetadata call takes into account the existing CSS viewport size
          // and zoom when calculating the new ones, so we need to reset these
          // things here before calling updateMetadata.
          this.setBrowserSize(kDefaultCSSViewportWidth, kDefaultCSSViewportHeight);
          this.setResolution(gScreenWidth / this.browserWidth, false);
          ViewportHandler.updateMetadata(this);

          // Note that if we draw without a display-port, things can go wrong. By the
          // time we execute this, it's almost certain a display-port has been set via
          // the MozScrolledAreaChanged event. If that didn't happen, the updateMetadata
          // call above does so at the end of the updateViewportSize function. As long
          // as that is happening, we don't need to do it again here.

          if (contentDocument.mozSyntheticDocument) {
            // for images, scale to fit width. this needs to happen *after* the call
            // to updateMetadata above, because that call sets the CSS viewport which
            // will affect the page size (i.e. contentDocument.body.scroll*) that we
            // use in this calculation. also we call sendViewportUpdate after changing
            // the resolution so that the display port gets recalculated appropriately.
            let fitZoom = Math.min(gScreenWidth / contentDocument.body.scrollWidth,
                                   gScreenHeight / contentDocument.body.scrollHeight);
            this.setResolution(fitZoom, false);
            this.sendViewportUpdate();
          }

          BrowserApp.displayedDocumentChanged();
          this.contentDocumentIsDisplayed = true;
        }
        break;
      case "nsPref:changed":
        if (aData == "browser.ui.zoom.force-user-scalable")
          ViewportHandler.updateMetadata(this);
        break;
    }
  },

  // nsIBrowserTab
  get window() {
    if (!this.browser)
      return null;
    return this.browser.contentWindow;
  },

  get scale() {
    return this._zoom;
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIWebProgressListener,
    Ci.nsISHistoryListener,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
    Ci.nsIBrowserTab
  ])
};

const kTapHighlightDelay = 50; // milliseconds

var BrowserEventHandler = {
  init: function init() {
    Services.obs.addObserver(this, "Gesture:SingleTap", false);
    Services.obs.addObserver(this, "Gesture:CancelTouch", false);
    Services.obs.addObserver(this, "Gesture:DoubleTap", false);
    Services.obs.addObserver(this, "Gesture:Scroll", false);
    Services.obs.addObserver(this, "dom-touch-listener-added", false);

    BrowserApp.deck.addEventListener("DOMUpdatePageReport", PopupBlockerObserver.onUpdatePageReport, false);
    BrowserApp.deck.addEventListener("touchstart", this, false);
    BrowserApp.deck.addEventListener("click", SelectHelper, true);
  },

  handleEvent: function(aEvent) {
    if (!BrowserApp.isBrowserContentDocumentDisplayed() || aEvent.touches.length > 1 || aEvent.defaultPrevented)
      return;

    let closest = aEvent.target;

    if (closest) {
      // If we've pressed a scrollable element, let Java know that we may
      // want to override the scroll behaviour (for document sub-frames)
      this._scrollableElement = this._findScrollableElement(closest, true);
      this._firstScrollEvent = true;

      if (this._scrollableElement != null) {
        // Discard if it's the top-level scrollable, we let Java handle this
        let doc = BrowserApp.selectedBrowser.contentDocument;
        if (this._scrollableElement != doc.body && this._scrollableElement != doc.documentElement)
          sendMessageToJava({ gecko: { type: "Panning:Override" } });
      }
    }

    if (!ElementTouchHelper.isElementClickable(closest, null, false))
      closest = ElementTouchHelper.elementFromPoint(BrowserApp.selectedBrowser.contentWindow,
                                                    aEvent.changedTouches[0].screenX,
                                                    aEvent.changedTouches[0].screenY);
    if (!closest)
      closest = aEvent.target;

    if (closest)
      this._doTapHighlight(closest);
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "dom-touch-listener-added") {
      let tab = BrowserApp.getTabForWindow(aSubject.top);
      if (!tab)
        return;

      sendMessageToJava({
        gecko: {
          type: "Tab:HasTouchListener",
          tabID: tab.id
        }
      });
      return;
    }

    // the remaining events are all dependent on the browser content document being the
    // same as the browser displayed document. if they are not the same, we should ignore
    // the event.
    if (!BrowserApp.isBrowserContentDocumentDisplayed())
      return;

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

      // round the scroll amounts because they come in as floats and might be
      // subject to minor rounding errors because of zoom values. I've seen values
      // like 0.99 come in here and get truncated to 0; this avoids that problem.
      let zoom = BrowserApp.selectedTab._zoom;
      data.x = Math.round(data.x / zoom);
      data.y = Math.round(data.y / zoom);

      if (this._firstScrollEvent) {
        while (this._scrollableElement != null && !this._elementCanScroll(this._scrollableElement, data.x, data.y))
          this._scrollableElement = this._findScrollableElement(this._scrollableElement, false);

        let doc = BrowserApp.selectedBrowser.contentDocument;
        if (this._scrollableElement == null || this._scrollableElement == doc.body || this._scrollableElement == doc.documentElement) {
          sendMessageToJava({ gecko: { type: "Panning:CancelOverride" } });
          return;
        }

        this._firstScrollEvent = false;
      }

      // Scroll the scrollable element
      if (this._elementCanScroll(this._scrollableElement, data.x, data.y)) {
        this._scrollElementBy(this._scrollableElement, data.x, data.y);
        sendMessageToJava({ gecko: { type: "Gesture:ScrollAck", scrolled: true } });
      } else {
        sendMessageToJava({ gecko: { type: "Gesture:ScrollAck", scrolled: false } });
      }
    } else if (aTopic == "Gesture:CancelTouch") {
      this._cancelTapHighlight();
    } else if (aTopic == "Gesture:SingleTap") {
      let element = this._highlightElement;
      if (element) {
        try {
          let data = JSON.parse(aData);
          let isClickable = ElementTouchHelper.isElementClickable(element);

          if (isClickable) {
            [data.x, data.y] = this._moveClickPoint(element, data.x, data.y);
            element = ElementTouchHelper.anyElementFromPoint(element.ownerDocument.defaultView, data.x, data.y);
            isClickable = ElementTouchHelper.isElementClickable(element);
          }

          this._sendMouseEvent("mousemove", element, data.x, data.y);
          this._sendMouseEvent("mousedown", element, data.x, data.y);
          this._sendMouseEvent("mouseup",   element, data.x, data.y);
  
          if (isClickable)
            Haptic.performSimpleAction(Haptic.LongPress);
        } catch(e) {
          Cu.reportError(e);
        }
      }
      this._cancelTapHighlight();
    } else if (aTopic == "Gesture:DoubleTap") {
      this._cancelTapHighlight();
      this.onDoubleTap(aData);
    }
  },
 
  _zoomOut: function() {
    sendMessageToJava({ gecko: { type: "Browser:ZoomToPageWidth"} });
  },

  _isRectZoomedIn: function(aRect, aViewport) {
    // This function checks to see if the area of the rect visible in the
    // viewport (i.e. the "overlapArea" variable below) is approximately
    // the max area of the rect we can show. It also checks that the rect
    // is actually on-screen by testing the left and right edges of the rect.
    // In effect, this tells us whether or not zooming in to this rect
    // will significantly change what the user is seeing.
    const minDifference = -20;
    const maxDifference = 20;

    let vRect = new Rect(aViewport.cssX, aViewport.cssY, aViewport.cssWidth, aViewport.cssHeight);
    let overlap = vRect.intersect(aRect);
    let overlapArea = overlap.width * overlap.height;
    let availHeight = Math.min(aRect.width * vRect.height / vRect.width, aRect.height);
    let showing = overlapArea / (aRect.width * availHeight);
    let dw = (aRect.width - vRect.width);
    let dx = (aRect.x - vRect.x);

    return (showing > 0.9 &&
            dx > minDifference && dx < maxDifference &&
            dw > minDifference && dw < maxDifference);
  },

  onDoubleTap: function(aData) {
    let data = JSON.parse(aData);

    let win = BrowserApp.selectedBrowser.contentWindow;
    
    let zoom = BrowserApp.selectedTab._zoom;
    let element = ElementTouchHelper.anyElementFromPoint(win, data.x, data.y);
    if (!element) {
      this._zoomOut();
      return;
    }

    while (element && !this._shouldZoomToElement(element))
      element = element.parentNode;

    if (!element) {
      this._zoomOut();
    } else {
      const margin = 15;
      let rect = ElementTouchHelper.getBoundingContentRect(element);

      let viewport = BrowserApp.selectedTab.getViewport();
      let bRect = new Rect(Math.max(viewport.cssPageLeft, rect.x - margin),
                           rect.y,
                           rect.w + 2 * margin,
                           rect.h);
      // constrict the rect to the screen's right edge
      bRect.width = Math.min(bRect.width, viewport.cssPageRight - bRect.x);

      // if the rect is already taking up most of the visible area and is stretching the
      // width of the page, then we want to zoom out instead.
      if (this._isRectZoomedIn(bRect, viewport)) {
        this._zoomOut();
        return;
      }

      rect.type = "Browser:ZoomToRect";
      rect.x = bRect.x;
      rect.y = bRect.y;
      rect.w = bRect.width;
      rect.h = Math.min(bRect.width * viewport.cssHeight / viewport.cssWidth, bRect.height);

      // if the block we're zooming to is really tall, and the user double-tapped
      // more than a screenful of height from the top of it, then adjust the y-coordinate
      // so that we center the actual point the user double-tapped upon. this prevents
      // flying to the top of a page when double-tapping to zoom in (bug 761721).
      // the 1.2 multiplier is just a little fuzz to compensate for bRect including horizontal
      // margins but not vertical ones.
      let cssTapY = viewport.cssY + data.y;
      if ((bRect.height > rect.h) && (cssTapY > rect.y + (rect.h * 1.2))) {
        rect.y = cssTapY - (rect.h / 2);
      }

      sendMessageToJava({ gecko: rect });
    }
  },

  _shouldZoomToElement: function(aElement) {
    let win = aElement.ownerDocument.defaultView;
    if (win.getComputedStyle(aElement, null).display == "inline")
      return false;
    if (aElement instanceof Ci.nsIDOMHTMLLIElement)
      return false;
    if (aElement instanceof Ci.nsIDOMHTMLQuoteElement)
      return false;
    return true;
  },

  _firstScrollEvent: false,

  _scrollableElement: null,

  _highlightElement: null,

  _highlightTimeout: null,

  _doTapHighlight: function _doTapHighlight(aElement) {
    this._cancelTapHighlight();
    this._highlightTimeout = setTimeout(function(self) {
      DOMUtils.setContentState(aElement, kStateActive);
      self._highlightElement = aElement;
    }, kTapHighlightDelay, this);
  },

  _cancelTapHighlight: function _cancelTapHighlight() {
    if (this._highlightTimeout) {
      clearTimeout(this._highlightTimeout);
      this._highlightTimeout = null;
    }

    if (!this._highlightElement)
      return;

    // If the active element is in a sub-frame, we need to make that frame's document
    // active to remove the element's active state.
    if (this._highlightElement.ownerDocument != BrowserApp.selectedBrowser.contentWindow.document)
      DOMUtils.setContentState(this._highlightElement.ownerDocument.documentElement, kStateActive);

    DOMUtils.setContentState(BrowserApp.selectedBrowser.contentWindow.document.documentElement, kStateActive);
    this._highlightElement = null;
  },

  _updateLastPosition: function(x, y, dx, dy) {
    this.lastX = x;
    this.lastY = y;
    this.lastTime = Date.now();

    this.motionBuffer.push({ dx: dx, dy: dy, time: this.lastTime });
  },

  _moveClickPoint: function(aElement, aX, aY) {
    // the element can be out of the aX/aY point because of the touch radius
    // if outside, we gracefully move the touch point to the edge of the element
    if (!(aElement instanceof HTMLHtmlElement)) {
      let isTouchClick = true;
      let rects = ElementTouchHelper.getContentClientRects(aElement);
      for (let i = 0; i < rects.length; i++) {
        let rect = rects[i];
        let inBounds =
          (aX> rect.left  && aX < (rect.left + rect.width)) &&
          (aY > rect.top && aY < (rect.top + rect.height));
        if (inBounds) {
          isTouchClick = false;
          break;
        }
      }

      if (isTouchClick) {
        let rect = rects[0];
        if (rect.width != 0 || rect.height != 0) {
          aX = Math.min(rect.left + rect.width, Math.max(rect.left, aX));
          aY = Math.min(rect.top + rect.height, Math.max(rect.top,  aY));
        }
      }
    }
    return [aX, aY];
  },

  _sendMouseEvent: function _sendMouseEvent(aName, aElement, aX, aY) {
    let window = aElement.ownerDocument.defaultView;
    try {
      let cwu = window.top.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      cwu.sendMouseEventToWindow(aName, Math.round(aX), Math.round(aY), 0, 1, 0, true);
    } catch(e) {
      Cu.reportError(e);
    }
  },

  _hasScrollableOverflow: function(elem) {
    var win = elem.ownerDocument.defaultView;
    if (!win)
      return false;
    var computedStyle = win.getComputedStyle(elem);
    if (!computedStyle)
      return false;
    return computedStyle.overflowX == 'auto' || computedStyle.overflowX == 'scroll'
        || computedStyle.overflowY == 'auto' || computedStyle.overflowY == 'scroll';
  },

  _findScrollableElement: function(elem, checkElem) {
    // Walk the DOM tree until we find a scrollable element
    let scrollable = false;
    while (elem) {
      /* Element is scrollable if its scroll-size exceeds its client size, and:
       * - It has overflow 'auto' or 'scroll'
       * - It's a textarea
       * - It's an HTML/BODY node
       * - It's a select element showing multiple rows
       */
      if (checkElem) {
        if (((elem.scrollHeight > elem.clientHeight) ||
             (elem.scrollWidth > elem.clientWidth)) &&
            (this._hasScrollableOverflow(elem) ||
             elem.mozMatchesSelector("html, body, textarea")) ||
            (elem instanceof HTMLSelectElement && (elem.size > 1 || elem.multiple))) {
          scrollable = true;
          break;
        }
      } else {
        checkElem = true;
      }

      // Propagate up iFrames
      if (!elem.parentNode && elem.documentElement && elem.documentElement.ownerDocument)
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
    let scrollX = (x < 0 && elem.scrollLeft > 0)
               || (x > 0 && elem.scrollLeft < (elem.scrollWidth - elem.clientWidth));

    let scrollY = (y < 0 && elem.scrollTop > 0)
               || (y > 0 && elem.scrollTop < (elem.scrollHeight - elem.clientHeight));

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
      elem = this.getClosest(cwu, aX, aY);
    }

    return elem;
  },

  /* Returns the touch radius in content px. */
  getTouchRadius: function getTouchRadius() {
    let dpiRatio = ViewportHandler.displayDPI / kReferenceDpi;
    let zoom = BrowserApp.selectedTab._zoom;
    return {
      top: this.radius.top * dpiRatio / zoom,
      right: this.radius.right * dpiRatio / zoom,
      bottom: this.radius.bottom * dpiRatio / zoom,
      left: this.radius.left * dpiRatio / zoom
    };
  },

  /* Returns the touch radius in reference pixels. */
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
    let target = aWindowUtils.elementFromPoint(aX, aY,
                                               true,   /* ignore root scroll frame*/
                                               false); /* don't flush layout */

    // if this element is clickable we return quickly. also, if it isn't,
    // use a cache to speed up future calls to isElementClickable in the
    // loop below.
    let unclickableCache = new Array();
    if (this.isElementClickable(target, unclickableCache, false))
      return target;

    target = null;
    let radius = this.getTouchRadius();
    let nodes = aWindowUtils.nodesFromRect(aX, aY, radius.top, radius.right, radius.bottom, radius.left, true, false);

    let threshold = Number.POSITIVE_INFINITY;
    for (let i = 0; i < nodes.length; i++) {
      let current = nodes[i];
      if (!current.mozMatchesSelector || !this.isElementClickable(current, unclickableCache, true))
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

  isElementClickable: function isElementClickable(aElement, aUnclickableCache, aAllowBodyListeners) {
    const selector = "a,:link,:visited,[role=button],button,input,select,textarea";

    let stopNode = null;
    if (!aAllowBodyListeners && aElement && aElement.ownerDocument)
      stopNode = aElement.ownerDocument.body;

    for (let elem = aElement; elem && elem != stopNode; elem = elem.parentNode) {
      if (aUnclickableCache && aUnclickableCache.indexOf(elem) != -1)
        continue;
      if (this._hasMouseListener(elem))
        return true;
      if (elem.mozMatchesSelector && elem.mozMatchesSelector(selector))
        return true;
      if (elem instanceof HTMLLabelElement && elem.control != null)
        return true;
      if (aUnclickableCache)
        aUnclickableCache.push(elem);
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

    return {x: r.left + scrollX.value,
            y: r.top + scrollY.value,
            w: r.width,
            h: r.height };
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
            errorDoc.location = "about:home";
          }
        }
        break;
      }
    }
  }
};

var FindHelper = {
  _fastFind: null,
  _targetTab: null,
  _initialViewport: null,
  _viewportChanged: false,

  init: function() {
    Services.obs.addObserver(this, "FindInPage:Find", false);
    Services.obs.addObserver(this, "FindInPage:Prev", false);
    Services.obs.addObserver(this, "FindInPage:Next", false);
    Services.obs.addObserver(this, "FindInPage:Closed", false);
    Services.obs.addObserver(this, "Tab:Selected", false);
  },

  uninit: function() {
    Services.obs.removeObserver(this, "FindInPage:Find");
    Services.obs.removeObserver(this, "FindInPage:Prev");
    Services.obs.removeObserver(this, "FindInPage:Next");
    Services.obs.removeObserver(this, "FindInPage:Closed");
    Services.obs.removeObserver(this, "Tab:Selected");
  },

  observe: function(aMessage, aTopic, aData) {
    switch(aTopic) {
      case "FindInPage:Find":
        this.doFind(aData);
        break;

      case "FindInPage:Prev":
        this.findAgain(aData, true);
        break;

      case "FindInPage:Next":
        this.findAgain(aData, false);
        break;

      case "Tab:Selected":
      case "FindInPage:Closed":
        this.findClosed();
        break;
    }
  },

  doFind: function(aSearchString) {
    if (!this._fastFind) {
      this._targetTab = BrowserApp.selectedTab;
      this._fastFind = this._targetTab.browser.fastFind;
      this._initialViewport = JSON.stringify(this._targetTab.getViewport());
      this._viewportChanged = false;
    }

    let result = this._fastFind.find(aSearchString, false);
    this.handleResult(result);
  },

  findAgain: function(aString, aFindBackwards) {
    // This can happen if the user taps next/previous after re-opening the search bar
    if (!this._fastFind) {
      this.doFind(aString);
      return;
    }

    let result = this._fastFind.findAgain(aFindBackwards, false);
    this.handleResult(result);
  },

  findClosed: function() {
    // If there's no find in progress, there's nothing to clean up
    if (!this._fastFind)
      return;

    this._fastFind.collapseSelection();
    this._fastFind = null;
    this._targetTab = null;
    this._initialViewport = null;
    this._viewportChanged = false;
  },

  handleResult: function(aResult) {
    if (aResult == Ci.nsITypeAheadFind.FIND_NOTFOUND) {
      if (this._viewportChanged) {
        if (this._targetTab != BrowserApp.selectedTab) {
          // this should never happen
          Cu.reportError("Warning: selected tab changed during find!");
          // fall through and restore viewport on the initial tab anyway
        }
        this._targetTab.setViewport(JSON.parse(this._initialViewport));
        this._targetTab.sendViewportUpdate();
      }
    } else {
      this._viewportChanged = true;
    }
  }
};

var FormAssistant = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFormSubmitObserver]),

  // Used to keep track of the element that corresponds to the current
  // autocomplete suggestions
  _currentInputElement: null,

  _isBlocklisted: false,

  // Keep track of whether or not an invalid form has been submitted
  _invalidSubmit: false,

  init: function() {
    Services.obs.addObserver(this, "FormAssist:AutoComplete", false);
    Services.obs.addObserver(this, "FormAssist:Blocklisted", false);
    Services.obs.addObserver(this, "FormAssist:Hidden", false);
    Services.obs.addObserver(this, "invalidformsubmit", false);

    // We need to use a capturing listener for focus events
    BrowserApp.deck.addEventListener("focus", this, true);
    BrowserApp.deck.addEventListener("click", this, true);
    BrowserApp.deck.addEventListener("input", this, false);
    BrowserApp.deck.addEventListener("pageshow", this, false);
  },

  uninit: function() {
    Services.obs.removeObserver(this, "FormAssist:AutoComplete");
    Services.obs.removeObserver(this, "FormAssist:Blocklisted");
    Services.obs.removeObserver(this, "FormAssist:Hidden");
    Services.obs.removeObserver(this, "invalidformsubmit");

    BrowserApp.deck.removeEventListener("focus", this);
    BrowserApp.deck.removeEventListener("click", this);
    BrowserApp.deck.removeEventListener("input", this);
    BrowserApp.deck.removeEventListener("pageshow", this);
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "FormAssist:AutoComplete":
        if (!this._currentInputElement)
          break;

        this._currentInputElement.QueryInterface(Ci.nsIDOMNSEditableElement).setUserInput(aData);

        let event = this._currentInputElement.ownerDocument.createEvent("Events");
        event.initEvent("DOMAutoComplete", true, true);
        this._currentInputElement.dispatchEvent(event);

        break;

      case "FormAssist:Blocklisted":
        this._isBlocklisted = (aData == "true");
        break;

      case "FormAssist:Hidden":
        this._currentInputElement = null;
        break;
    }
  },

  notifyInvalidSubmit: function notifyInvalidSubmit(aFormElement, aInvalidElements) {
    if (!aInvalidElements.length)
      return;

    // Ignore this notificaiton if the current tab doesn't contain the invalid form
    if (BrowserApp.selectedBrowser.contentDocument !=
        aFormElement.ownerDocument.defaultView.top.document)
      return;

    this._invalidSubmit = true;

    // Our focus listener will show the element's validation message
    let currentElement = aInvalidElements.queryElementAt(0, Ci.nsISupports);
    currentElement.focus();
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "focus":
        let currentElement = aEvent.target;

        // Only show a validation message on focus.
        this._showValidationMessage(currentElement);
        break;

      case "click":
        currentElement = aEvent.target;

        // Prioritize a form validation message over autocomplete suggestions
        // when the element is first focused (a form validation message will
        // only be available if an invalid form was submitted)
        if (this._showValidationMessage(currentElement))
          break;
        this._showAutoCompleteSuggestions(currentElement);
        break;

      case "input":
        currentElement = aEvent.target;

        // Since we can only show one popup at a time, prioritze autocomplete
        // suggestions over a form validation message
        if (this._showAutoCompleteSuggestions(currentElement))
          break;
        if (this._showValidationMessage(currentElement))
          break;

        // If we're not showing autocomplete suggestions, hide the form assist popup
        this._hideFormAssistPopup();
        break;

      // Reset invalid submit state on each pageshow
      case "pageshow":
        let target = aEvent.originalTarget;
        let selectedDocument = BrowserApp.selectedBrowser.contentDocument;
        if (target == selectedDocument || target.ownerDocument == selectedDocument)
          this._invalidSubmit = false;
    }
  },

  // We only want to show autocomplete suggestions for certain elements
  _isAutoComplete: function _isAutoComplete(aElement) {
    if (!(aElement instanceof HTMLInputElement) || aElement.readOnly ||
        (aElement.getAttribute("type") == "password") ||
        (aElement.hasAttribute("autocomplete") &&
         aElement.getAttribute("autocomplete").toLowerCase() == "off"))
      return false;

    return true;
  },

  // Retrieves autocomplete suggestions for an element from the form autocomplete service.
  _getAutoCompleteSuggestions: function _getAutoCompleteSuggestions(aSearchString, aElement) {
    // Cache the form autocomplete service for future use
    if (!this._formAutoCompleteService)
      this._formAutoCompleteService = Cc["@mozilla.org/satchel/form-autocomplete;1"].
                                      getService(Ci.nsIFormAutoComplete);

    let results = this._formAutoCompleteService.autoCompleteSearch(aElement.name || aElement.id,
                                                                   aSearchString, aElement, null);
    let suggestions = [];
    for (let i = 0; i < results.matchCount; i++) {
      let value = results.getValueAt(i);

      // Do not show the value if it is the current one in the input field
      if (value == aSearchString)
        continue;

      // Supply a label and value, since they can differ for datalist suggestions
      suggestions.push({ label: value, value: value });
    }

    return suggestions;
  },

  /**
   * (Copied from mobile/xul/chrome/content/forms.js)
   * This function is similar to getListSuggestions from
   * components/satchel/src/nsInputListAutoComplete.js but sadly this one is
   * used by the autocomplete.xml binding which is not in used in fennec
   */
  _getListSuggestions: function _getListSuggestions(aElement) {
    if (!(aElement instanceof HTMLInputElement) || !aElement.list)
      return [];

    let suggestions = [];
    let filter = !aElement.hasAttribute("mozNoFilter");
    let lowerFieldValue = aElement.value.toLowerCase();

    let options = aElement.list.options;
    let length = options.length;
    for (let i = 0; i < length; i++) {
      let item = options.item(i);

      let label = item.value;
      if (item.label)
        label = item.label;
      else if (item.text)
        label = item.text;

      if (filter && label.toLowerCase().indexOf(lowerFieldValue) == -1)
        continue;
      suggestions.push({ label: label, value: item.value });
    }

    return suggestions;
  },

  // Gets the element position data necessary for the Java UI to position
  // the form assist popup.
  _getElementPositionData: function _getElementPositionData(aElement) {
    let rect = ElementTouchHelper.getBoundingContentRect(aElement);
    let viewport = BrowserApp.selectedTab.getViewport();
    
    return { rect: [rect.x - (viewport.x / viewport.zoom),
                    rect.y - (viewport.y / viewport.zoom),
                    rect.w, rect.h],
             zoom: viewport.zoom }
  },

  // Retrieves autocomplete suggestions for an element from the form autocomplete service
  // and sends the suggestions to the Java UI, along with element position data.
  // Returns true if there are suggestions to show, false otherwise.
  _showAutoCompleteSuggestions: function _showAutoCompleteSuggestions(aElement) {
    if (!this._isAutoComplete(aElement))
      return false;

    // Don't display the form auto-complete popup after the user starts typing
    // to avoid confusing somes IME. See bug 758820 and bug 632744.
    if (this._isBlocklisted && aElement.value.length > 0) {
      return false;
    }

    let autoCompleteSuggestions = this._getAutoCompleteSuggestions(aElement.value, aElement);
    let listSuggestions = this._getListSuggestions(aElement);

    // On desktop, we show datalist suggestions below autocomplete suggestions,
    // without duplicates removed.
    let suggestions = autoCompleteSuggestions.concat(listSuggestions);

    // Return false if there are no suggestions to show
    if (!suggestions.length)
      return false;

    let positionData = this._getElementPositionData(aElement);
    sendMessageToJava({
      gecko: {
        type:  "FormAssist:AutoComplete",
        suggestions: suggestions,
        rect: positionData.rect,
        zoom: positionData.zoom
      }
    });

    // Keep track of input element so we can fill it in if the user
    // selects an autocomplete suggestion
    this._currentInputElement = aElement;

    return true;
  },

  // Only show a validation message if the user submitted an invalid form,
  // there's a non-empty message string, and the element is the correct type
  _isValidateable: function _isValidateable(aElement) {
    if (!this._invalidSubmit ||
        !aElement.validationMessage ||
        !(aElement instanceof HTMLInputElement ||
          aElement instanceof HTMLTextAreaElement ||
          aElement instanceof HTMLSelectElement ||
          aElement instanceof HTMLButtonElement))
      return false;

    return true;
  },

  // Sends a validation message and position data for an element to the Java UI.
  // Returns true if there's a validation message to show, false otherwise.
  _showValidationMessage: function _sendValidationMessage(aElement) {
    if (!this._isValidateable(aElement))
      return false;

    let positionData = this._getElementPositionData(aElement);
    sendMessageToJava({
      gecko: {
        type: "FormAssist:ValidationMessage",
        validationMessage: aElement.validationMessage,
        rect: positionData.rect,
        zoom: positionData.zoom
      }
    });

    return true;
  },

  _hideFormAssistPopup: function _hideFormAssistPopup() {
    sendMessageToJava({
      gecko: { type:  "FormAssist:Hide" }
    });
  }
};

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
        let win = installInfo.originatingWindow;
        let tab = BrowserApp.getTabForWindow(win.top);
        if (!tab)
          return;

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
        NativeWindow.doorhanger.show(message, aTopic, buttons, tab.id);
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
      this.showRestartPrompt();
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

  onDownloadCancelled: function(aInstall) {
    let host = (aInstall.originatingURI instanceof Ci.nsIStandardURL) && aInstall.originatingURI.host;
    if (!host)
      host = (aInstall.sourceURI instanceof Ci.nsIStandardURL) && aInstall.sourceURI.host;

    let error = (host || aInstall.error == 0) ? "addonError" : "addonLocalError";
    if (aInstall.error != 0)
      error += aInstall.error;
    else if (aInstall.addon && aInstall.addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED)
      error += "Blocklisted";
    else if (aInstall.addon && (!aInstall.addon.isCompatible || !aInstall.addon.isPlatformCompatible))
      error += "Incompatible";
    else
      return; // No need to show anything in this case.

    let msg = Strings.browser.GetStringFromName(error);
    // TODO: formatStringFromName
    msg = msg.replace("#1", aInstall.name);
    if (host)
      msg = msg.replace("#2", host);
    msg = msg.replace("#3", Strings.brand.GetStringFromName("brandShortName"));
    msg = msg.replace("#4", Services.appinfo.version);

    NativeWindow.toast.show(msg, "short");
  },

  showRestartPrompt: function() {
    let buttons = [{
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
  },

  hideRestartPrompt: function() {
    NativeWindow.doorhanger.hide("addon-app-restart", BrowserApp.selectedTab.id);
  }
};

// Blindly copied from Safari documentation for now.
const kViewportMinScale  = 0;
const kViewportMaxScale  = 10;
const kViewportMinWidth  = 200;
const kViewportMaxWidth  = 10000;
const kViewportMinHeight = 223;
const kViewportMaxHeight = 10000;

var ViewportHandler = {
  // The cached viewport metadata for each document. We tie viewport metadata to each document
  // instead of to each tab so that we don't have to update it when the document changes. Using an
  // ES6 weak map lets us avoid leaks.
  _metadata: new WeakMap(),

  init: function init() {
    addEventListener("DOMMetaAdded", this, false);
    Services.obs.addObserver(this, "Window:Resize", false);
  },

  uninit: function uninit() {
    removeEventListener("DOMMetaAdded", this, false);
    Services.obs.removeObserver(this, "Window:Resize", false);
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "DOMMetaAdded":
        let target = aEvent.originalTarget;
        if (target.name != "viewport")
          break;
        let document = target.ownerDocument;
        let browser = BrowserApp.getBrowserForDocument(document);
        let tab = BrowserApp.getTabForBrowser(browser);
        if (tab && tab.contentDocumentIsDisplayed)
          this.updateMetadata(tab);
        break;
    }
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "Window:Resize":
        if (window.outerWidth == gScreenWidth && window.outerHeight == gScreenHeight)
          break;
        if (window.outerWidth == 0 || window.outerHeight == 0)
          break;

        let oldScreenWidth = gScreenWidth;
        gScreenWidth = window.outerWidth;
        gScreenHeight = window.outerHeight;
        let tabs = BrowserApp.tabs;
        for (let i = 0; i < tabs.length; i++)
          tabs[i].updateViewportSize(oldScreenWidth);
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
    if (aWindow.document instanceof XULDocument)
      return { defaultZoom: 1, autoSize: true, allowZoom: false, autoScale: false };

    let windowUtils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);

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

    if (isNaN(scale) && isNaN(minScale) && isNaN(maxScale) && allowZoomStr == "" && widthStr == "" && heightStr == "") {
      // Only check for HandheldFriendly if we don't have a viewport meta tag
      let handheldFriendly = windowUtils.getDocumentMetadata("HandheldFriendly");
      if (handheldFriendly == "true")
        return { defaultZoom: 1, autoSize: true, allowZoom: true, autoScale: true };

      let doctype = aWindow.document.doctype;
      if (doctype && /(WAP|WML|Mobile)/.test(doctype.publicId))
        return { defaultZoom: 1, autoSize: true, allowZoom: true, autoScale: true };
    }

    scale = this.clamp(scale, kViewportMinScale, kViewportMaxScale);
    minScale = this.clamp(minScale, kViewportMinScale, kViewportMaxScale);
    maxScale = this.clamp(maxScale, minScale, kViewportMaxScale);

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
  },

  /**
   * Returns the viewport metadata for the given document, or the default metrics if no viewport
   * metadata is available for that document.
   */
  getMetadataForDocument: function getMetadataForDocument(aDocument) {
    let metadata = this._metadata.get(aDocument, this.getDefaultMetadata());
    return metadata;
  },

  /** Updates the saved viewport metadata for the given content document. */
  setMetadataForDocument: function setMetadataForDocument(aDocument, aMetadata) {
    if (!aMetadata)
      this._metadata.delete(aDocument);
    else
      this._metadata.set(aDocument, aMetadata);
  },

  /** Returns the default viewport metadata for a document. */
  getDefaultMetadata: function getDefaultMetadata() {
    return {
      autoSize: false,
      allowZoom: true,
      autoScale: true,
      scaleRatio: ViewportHandler.getScaleRatio()
    };
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
            callback: function() {
              // Set permission before opening popup windows
              PopupBlockerObserver.allowPopupsForSite(true);
              PopupBlockerObserver.showPopupsForSite();
            }
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

        let parent = BrowserApp.selectedTab;
        BrowserApp.addTab(popupURIspec, { parentId: parent.id });
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

    let tab = BrowserApp.getTabForWindow(aContentWindow);
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
    let tab = BrowserApp.getTabForWindow(contentWindow);
    if (!tab)
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

var ClipboardHelper = {
  init: function() {
    NativeWindow.contextmenus.add(Strings.browser.GetStringFromName("contextmenu.copy"), ClipboardHelper.getCopyContext(false), ClipboardHelper.copy.bind(ClipboardHelper));
    NativeWindow.contextmenus.add(Strings.browser.GetStringFromName("contextmenu.copyAll"), ClipboardHelper.getCopyContext(true), ClipboardHelper.copy.bind(ClipboardHelper));
    NativeWindow.contextmenus.add(Strings.browser.GetStringFromName("contextmenu.selectAll"), ClipboardHelper.selectAllContext, ClipboardHelper.select.bind(ClipboardHelper));
    NativeWindow.contextmenus.add(Strings.browser.GetStringFromName("contextmenu.paste"), ClipboardHelper.pasteContext, ClipboardHelper.paste.bind(ClipboardHelper));
    NativeWindow.contextmenus.add(Strings.browser.GetStringFromName("contextmenu.changeInputMethod"), NativeWindow.contextmenus.textContext, ClipboardHelper.inputMethod.bind(ClipboardHelper));
  },

  get clipboardHelper() {
    delete this.clipboardHelper;
    return this.clipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
  },

  get clipboard() {
    delete this.clipboard;
    return this.clipboard = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
  },

  copy: function(aElement) {
    let selectionStart = aElement.selectionStart;
    let selectionEnd = aElement.selectionEnd;
    if (selectionStart != selectionEnd) {
      string = aElement.value.slice(selectionStart, selectionEnd);
      this.clipboardHelper.copyString(string, aElement.ownerDocument);
    } else {
      this.clipboardHelper.copyString(aElement.value, aElement.ownerDocument);
    }
  },

  select: function(aElement) {
    if (!aElement || !(aElement instanceof Ci.nsIDOMNSEditableElement))
      return;
    let target = aElement.QueryInterface(Ci.nsIDOMNSEditableElement);
    target.editor.selectAll();
    target.focus();
  },

  paste: function(aElement) {
    if (!aElement || !(aElement instanceof Ci.nsIDOMNSEditableElement))
      return;
    let target = aElement.QueryInterface(Ci.nsIDOMNSEditableElement);
    target.editor.paste(Ci.nsIClipboard.kGlobalClipboard);
    target.focus();  
  },

  inputMethod: function(aElement) {
    Cc["@mozilla.org/imepicker;1"].getService(Ci.nsIIMEPicker).show();
  },

  getCopyContext: function(isCopyAll) {
    return {
      matches: function(aElement) {
        if (NativeWindow.contextmenus.textContext.matches(aElement)) {
          // Don't include "copy" for password fields.
          // mozIsTextField(true) tests for only non-password fields.
          if (aElement instanceof Ci.nsIDOMHTMLInputElement && !aElement.mozIsTextField(true))
            return false;

          let selectionStart = aElement.selectionStart;
          let selectionEnd = aElement.selectionEnd;
          if (selectionStart != selectionEnd)
            return true;

          if (isCopyAll && aElement.textLength > 0)
            return true;
        }
        return false;
      }
    }
  },

  selectAllContext: {
    matches: function selectAllContextMatches(aElement) {
      if (NativeWindow.contextmenus.textContext.matches(aElement)) {
          let selectionStart = aElement.selectionStart;
          let selectionEnd = aElement.selectionEnd;
          return (selectionStart > 0 || selectionEnd < aElement.textLength);
      }
      return false;
    }
  },

  pasteContext: {
    matches: function(aElement) {
      if (NativeWindow.contextmenus.textContext.matches(aElement)) {
        let flavors = ["text/unicode"];
        return ClipboardHelper.clipboard.hasDataMatchingFlavors(flavors, flavors.length, Ci.nsIClipboard.kGlobalClipboard);
      }
      return false;
    }
  }
};

var PluginHelper = {
  showDoorHanger: function(aTab) {
    if (!aTab.browser)
      return;

    // Even though we may not end up showing a doorhanger, this flag
    // lets us know that we've tried to show a doorhanger.
    aTab.shouldShowPluginDoorhanger = false;

    let uri = aTab.browser.currentURI;

    // If the user has previously set a plugins permission for this website,
    // either play or don't play the plugins instead of showing a doorhanger.
    let permValue = Services.perms.testPermission(uri, "plugins");
    if (permValue != Services.perms.UNKNOWN_ACTION) {
      if (permValue == Services.perms.ALLOW_ACTION)
        PluginHelper.playAllPlugins(aTab.browser.contentWindow);

      return;
    }

    let message = Strings.browser.formatStringFromName("clickToPlayPlugins.message1",
                                                       [uri.host], 1);
    let buttons = [
      {
        label: Strings.browser.GetStringFromName("clickToPlayPlugins.yes"),
        callback: function(aChecked) {
          // If the user checked "Don't ask again", make a permanent exception
          if (aChecked)
            Services.perms.add(uri, "plugins", Ci.nsIPermissionManager.ALLOW_ACTION);

          PluginHelper.playAllPlugins(aTab.browser.contentWindow);
        }
      },
      {
        label: Strings.browser.GetStringFromName("clickToPlayPlugins.no"),
        callback: function(aChecked) {
          // If the user checked "Don't ask again", make a permanent exception
          if (aChecked)
            Services.perms.add(uri, "plugins", Ci.nsIPermissionManager.DENY_ACTION);

          // Other than that, do nothing
        }
      }
    ];

    // Add a checkbox with a "Don't ask again" message
    let options = { checkbox: Strings.browser.GetStringFromName("clickToPlayPlugins.dontAskAgain") };

    NativeWindow.doorhanger.show(message, "ask-to-play-plugins", buttons, aTab.id, options);
  },

  playAllPlugins: function(aContentWindow) {
    let cwu = aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils);
    // XXX not sure if we should enable plugins for the parent documents...
    let plugins = cwu.plugins;
    if (!plugins || !plugins.length)
      return;

    plugins.forEach(this.playPlugin);
  },

  playPlugin: function(plugin) {
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    if (!objLoadingContent.activated)
      objLoadingContent.playPlugin();
  },

  getPluginPreference: function getPluginPreference() {
    let pluginDisable = Services.prefs.getBoolPref("plugin.disable");
    if (pluginDisable)
      return "0";

    let clickToPlay = Services.prefs.getBoolPref("plugins.click_to_play");
    return clickToPlay ? "2" : "1";
  },

  setPluginPreference: function setPluginPreference(aValue) {
    switch (aValue) {
      case "0": // Enable Plugins = No
        Services.prefs.setBoolPref("plugin.disable", true);
        Services.prefs.clearUserPref("plugins.click_to_play");
        break;
      case "1": // Enable Plugins = Yes
        Services.prefs.clearUserPref("plugin.disable");
        Services.prefs.setBoolPref("plugins.click_to_play", false);
        break;
      case "2": // Enable Plugins = Tap to Play (default)
        Services.prefs.clearUserPref("plugin.disable");
        Services.prefs.clearUserPref("plugins.click_to_play");
        break;
    }
  },

  // Copied from /browser/base/content/browser.js
  isTooSmall : function (plugin, overlay) {
    // Is the <object>'s size too small to hold what we want to show?
    let pluginRect = plugin.getBoundingClientRect();
    // XXX bug 446693. The text-shadow on the submitted-report text at
    //     the bottom causes scrollHeight to be larger than it should be.
    let overflows = (overlay.scrollWidth > pluginRect.width) ||
                    (overlay.scrollHeight - 5 > pluginRect.height);

    return overflows;
  }
};

var PermissionsHelper = {

  _permissonTypes: ["password", "geolocation", "popup", "indexedDB",
                    "offline-app", "desktop-notification", "plugins"],
  _permissionStrings: {
    "password": {
      label: "password.rememberPassword",
      allowed: "password.remember",
      denied: "password.never"
    },
    "geolocation": {
      label: "geolocation.shareLocation",
      allowed: "geolocation.alwaysAllow",
      denied: "geolocation.neverAllow"
    },
    "popup": {
      label: "blockPopups.label",
      allowed: "popupButtonAlwaysAllow2",
      denied: "popupButtonNeverWarn2"
    },
    "indexedDB": {
      label: "offlineApps.storeOfflineData",
      allowed: "offlineApps.allow",
      denied: "offlineApps.never"
    },
    "offline-app": {
      label: "offlineApps.storeOfflineData",
      allowed: "offlineApps.allow",
      denied: "offlineApps.never"
    },
    "desktop-notification": {
      label: "desktopNotification.useNotifications",
      allowed: "desktopNotification.allow",
      denied: "desktopNotification.dontAllow"
    },
    "plugins": {
      label: "clickToPlayPlugins.playPlugins",
      allowed: "clickToPlayPlugins.yes",
      denied: "clickToPlayPlugins.no"
    }
  },

  init: function init() {
    Services.obs.addObserver(this, "Permissions:Get", false);
    Services.obs.addObserver(this, "Permissions:Clear", false);
  },

  observe: function observe(aSubject, aTopic, aData) {
    let uri = BrowserApp.selectedBrowser.currentURI;

    switch (aTopic) {
      case "Permissions:Get":
        let permissions = [];
        for (let i = 0; i < this._permissonTypes.length; i++) {
          let type = this._permissonTypes[i];
          let value = this.getPermission(uri, type);

          // Only add the permission if it was set by the user
          if (value == Services.perms.UNKNOWN_ACTION)
            continue;

          // Get the strings that correspond to the permission type
          let typeStrings = this._permissionStrings[type];
          let label = Strings.browser.GetStringFromName(typeStrings["label"]);

          // Get the key to look up the appropriate string entity
          let valueKey = value == Services.perms.ALLOW_ACTION ?
                         "allowed" : "denied";
          let valueString = Strings.browser.GetStringFromName(typeStrings[valueKey]);

          // If we implement a two-line UI, we will need to pass the label and
          // value individually and let java handle the formatting
          let setting = Strings.browser.formatStringFromName("siteSettings.labelToValue",
                                                             [ label, valueString ], 2);
          permissions.push({
            type: type,
            setting: setting
          });
        }

        // Keep track of permissions, so we know which ones to clear
        this._currentPermissions = permissions; 

        let host;
        try {
          host = uri.host;
        } catch(e) {
          host = uri.spec;
        }
        sendMessageToJava({
          gecko: {
            type: "Permissions:Data",
            host: host,
            permissions: permissions
          }
        });
        break;
 
      case "Permissions:Clear":
        // An array of the indices of the permissions we want to clear
        let permissionsToClear = JSON.parse(aData);

        for (let i = 0; i < permissionsToClear.length; i++) {
          let indexToClear = permissionsToClear[i];
          let permissionType = this._currentPermissions[indexToClear]["type"];
          this.clearPermission(uri, permissionType);
        }
        break;
    }
  },

  /**
   * Gets the permission value stored for a specified permission type.
   *
   * @param aType
   *        The permission type string stored in permission manager.
   *        e.g. "geolocation", "indexedDB", "popup"
   *
   * @return A permission value defined in nsIPermissionManager.
   */
  getPermission: function getPermission(aURI, aType) {
    // Password saving isn't a nsIPermissionManager permission type, so handle
    // it seperately.
    if (aType == "password") {
      // By default, login saving is enabled, so if it is disabled, the
      // user selected the never remember option
      if (!Services.logins.getLoginSavingEnabled(aURI.prePath))
        return Services.perms.DENY_ACTION;

      // Check to see if the user ever actually saved a login
      if (Services.logins.countLogins(aURI.prePath, "", ""))
        return Services.perms.ALLOW_ACTION;

      return Services.perms.UNKNOWN_ACTION;
    }

    // Geolocation consumers use testExactPermission
    if (aType == "geolocation")
      return Services.perms.testExactPermission(aURI, aType);

    return Services.perms.testPermission(aURI, aType);
  },

  /**
   * Clears a user-set permission value for the site given a permission type.
   *
   * @param aType
   *        The permission type string stored in permission manager.
   *        e.g. "geolocation", "indexedDB", "popup"
   */
  clearPermission: function clearPermission(aURI, aType) {
    // Password saving isn't a nsIPermissionManager permission type, so handle
    // it seperately.
    if (aType == "password") {
      // Get rid of exisiting stored logings
      let logins = Services.logins.findLogins({}, aURI.prePath, "", "");
      for (let i = 0; i < logins.length; i++) {
        Services.logins.removeLogin(logins[i]);
      }
      // Re-set login saving to enabled
      Services.logins.setLoginSavingEnabled(aURI.prePath, true);
    } else {
      Services.perms.remove(aURI.host, aType);
      // Clear content prefs set in ContentPermissionPrompt.js
      Services.contentPrefs.removePref(aURI, aType + ".request.remember");
    }
  }
};

var MasterPassword = {
  pref: "privacy.masterpassword.enabled",
  _tokenName: "",

  get _secModuleDB() {
    delete this._secModuleDB;
    return this._secModuleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"].getService(Ci.nsIPKCS11ModuleDB);
  },

  get _pk11DB() {
    delete this._pk11DB;
    return this._pk11DB = Cc["@mozilla.org/security/pk11tokendb;1"].getService(Ci.nsIPK11TokenDB);
  },

  get enabled() {
    let slot = this._secModuleDB.findSlotByName(this._tokenName);
    if (slot) {
      let status = slot.status;
      return status != Ci.nsIPKCS11Slot.SLOT_UNINITIALIZED && status != Ci.nsIPKCS11Slot.SLOT_READY;
    }
    return false;
  },

  setPassword: function setPassword(aPassword) {
    try {
      let status;
      let slot = this._secModuleDB.findSlotByName(this._tokenName);
      if (slot)
        status = slot.status;
      else
        return false;

      let token = this._pk11DB.findTokenByName(this._tokenName);

      if (status == Ci.nsIPKCS11Slot.SLOT_UNINITIALIZED)
        token.initPassword(aPassword);
      else if (status == Ci.nsIPKCS11Slot.SLOT_READY)
        token.changePassword("", aPassword);

      this.updatePref();
      return true;
    } catch(e) {
      dump("MasterPassword.setPassword: " + e);
    }
    return false;
  },

  removePassword: function removePassword(aOldPassword) {
    try {
      let token = this._pk11DB.getInternalKeyToken();
      if (token.checkPassword(aOldPassword)) {
        token.changePassword(aOldPassword, "");
        this.updatePref();
        return true;
      }
    } catch(e) {
      dump("MasterPassword.removePassword: " + e + "\n");
    }
    NativeWindow.toast.show(Strings.browser.GetStringFromName("masterPassword.incorrect"), "short");
    return false;
  },

  updatePref: function() {
    var prefs = [];
    let pref = {
      name: this.pref,
      type: "bool",
      value: this.enabled
    };
    prefs.push(pref);

    sendMessageToJava({
      gecko: {
        type: "Preferences:Data",
        preferences: prefs
      }
    });
  }
};

var CharacterEncoding = {
  _charsets: [],

  init: function init() {
    Services.obs.addObserver(this, "CharEncoding:Get", false);
    Services.obs.addObserver(this, "CharEncoding:Set", false);
    this.sendState();
  },

  uninit: function uninit() {
    Services.obs.removeObserver(this, "CharEncoding:Get", false);
    Services.obs.removeObserver(this, "CharEncoding:Set", false);
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "CharEncoding:Get":
        this.getEncoding();
        break;
      case "CharEncoding:Set":
        this.setEncoding(aData);
        break;
    }
  },

  sendState: function sendState() {
    let showCharEncoding = "false";
    try {
      showCharEncoding = Services.prefs.getComplexValue("browser.menu.showCharacterEncoding", Ci.nsIPrefLocalizedString).data;
    } catch (e) { /* Optional */ }

    sendMessageToJava({
      gecko: {
        type: "CharEncoding:State",
        visible: showCharEncoding
      }
    });
  },

  getEncoding: function getEncoding() {
    function normalizeCharsetCode(charsetCode) {
      return charsetCode.trim().toLowerCase();
    }

    function getTitle(charsetCode) {
      let charsetTitle = charsetCode;
      try {
        charsetTitle = Strings.charset.GetStringFromName(charsetCode + ".title");
      } catch (e) {
        dump("error: title not found for " + charsetCode);
      }
      return charsetTitle;
    }

    if (!this._charsets.length) {
      let charsets = Services.prefs.getComplexValue("intl.charsetmenu.browser.static", Ci.nsIPrefLocalizedString).data;
      this._charsets = charsets.split(",").map(function (charset) {
        return {
          code: normalizeCharsetCode(charset),
          title: getTitle(charset)
        };
      });
    }

    // if document charset is not in charset options, add it
    let docCharset = normalizeCharsetCode(BrowserApp.selectedBrowser.contentDocument.characterSet);
    let selected = 0;
    let charsetCount = this._charsets.length;
    for (; selected < charsetCount && this._charsets[selected].code != docCharset; selected++);
    if (selected == charsetCount) {
      this._charsets.push({
        code: docCharset,
        title: getTitle(docCharset)
      });
    }

    sendMessageToJava({
      gecko: {
        type: "CharEncoding:Data",
        charsets: this._charsets,
        selected: selected
      }
    });
  },

  setEncoding: function setEncoding(aEncoding) {
    let browser = BrowserApp.selectedBrowser;
    let docCharset = browser.docShell.QueryInterface(Ci.nsIDocCharset);
    docCharset.charset = aEncoding;
    browser.reload(Ci.nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
  }
};

var IdentityHandler = {
  // Mode strings used to control CSS display
  IDENTITY_MODE_IDENTIFIED       : "identified", // High-quality identity information
  IDENTITY_MODE_DOMAIN_VERIFIED  : "verified",   // Minimal SSL CA-signed domain verification
  IDENTITY_MODE_UNKNOWN          : "unknown",  // No trusted identity information

  // Cache the most recent SSLStatus and Location seen in getIdentityStrings
  _lastStatus : null,
  _lastLocation : null,

  /**
   * Helper to parse out the important parts of _lastStatus (of the SSL cert in
   * particular) for use in constructing identity UI strings
  */
  getIdentityData : function() {
    let result = {};
    let status = this._lastStatus.QueryInterface(Components.interfaces.nsISSLStatus);
    let cert = status.serverCert;

    // Human readable name of Subject
    result.subjectOrg = cert.organization;

    // SubjectName fields, broken up for individual access
    if (cert.subjectName) {
      result.subjectNameFields = {};
      cert.subjectName.split(",").forEach(function(v) {
        let field = v.split("=");
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

  getIdentityMode: function getIdentityMode(aState) {
    if (aState & Ci.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL)
      return this.IDENTITY_MODE_IDENTIFIED;

    if (aState & Ci.nsIWebProgressListener.STATE_SECURE_HIGH)
      return this.IDENTITY_MODE_DOMAIN_VERIFIED;

    return this.IDENTITY_MODE_UNKNOWN;
  },

  /**
   * Determine the identity of the page being displayed by examining its SSL cert
   * (if available). Return the data needed to update the UI.
   */
  checkIdentity: function checkIdentity(aState, aBrowser) {
    this._lastStatus = aBrowser.securityUI
                               .QueryInterface(Components.interfaces.nsISSLStatusProvider)
                               .SSLStatus;

    // Don't pass in the actual location object, since it can cause us to 
    // hold on to the window object too long.  Just pass in the fields we
    // care about. (bug 424829)
    let locationObj = {};
    try {
      let location = aBrowser.contentWindow.location;
      locationObj.host = location.host;
      locationObj.hostname = location.hostname;
      locationObj.port = location.port;
    } catch (ex) {
      // Can sometimes throw if the URL being visited has no host/hostname,
      // e.g. about:blank. The _state for these pages means we won't need these
      // properties anyways, though.
    }
    this._lastLocation = locationObj;

    let mode = this.getIdentityMode(aState);
    let result = { mode: mode };

    // We can't to do anything else for pages without identity data
    if (mode == this.IDENTITY_MODE_UNKNOWN)
      return result;

    // Ideally we'd just make this a Java string
    result.encrypted = Strings.browser.GetStringFromName("identity.encrypted2");
    result.host = this.getEffectiveHost();

    let iData = this.getIdentityData();
    result.verifier = Strings.browser.formatStringFromName("identity.identified.verifier", [iData.caOrg], 1);

    // If the cert is identified, then we can populate the results with credentials
    if (mode == this.IDENTITY_MODE_IDENTIFIED) {
      result.owner = iData.subjectOrg;

      // Build an appropriate supplemental block out of whatever location data we have
      let supplemental = "";
      if (iData.city)
        supplemental += iData.city + "\n";
      if (iData.state && iData.country)
        supplemental += Strings.browser.formatStringFromName("identity.identified.state_and_country", [iData.state, iData.country], 2);
      else if (iData.state) // State only
        supplemental += iData.state;
      else if (iData.country) // Country only
        supplemental += iData.country;
      result.supplemental = supplemental;

      return result;
    }
    
    // Otherwise, we don't know the cert owner
    result.owner = Strings.browser.GetStringFromName("identity.ownerUnknown2");

    // Cache the override service the first time we need to check it
    if (!this._overrideService)
      this._overrideService = Cc["@mozilla.org/security/certoverride;1"].getService(Ci.nsICertOverrideService);

    // Check whether this site is a security exception. XPConnect does the right
    // thing here in terms of converting _lastLocation.port from string to int, but
    // the overrideService doesn't like undefined ports, so make sure we have
    // something in the default case (bug 432241).
    // .hostname can return an empty string in some exceptional cases -
    // hasMatchingOverride does not handle that, so avoid calling it.
    // Updating the tooltip value in those cases isn't critical.
    // FIXME: Fixing bug 646690 would probably makes this check unnecessary
    if (this._lastLocation.hostname &&
        this._overrideService.hasMatchingOverride(this._lastLocation.hostname,
                                                  (this._lastLocation.port || 443),
                                                  iData.cert, {}, {}))
      result.verifier = Strings.browser.GetStringFromName("identity.identified.verified_by_you");

    return result;
  },

  /**
   * Return the eTLD+1 version of the current hostname
   */
  getEffectiveHost: function getEffectiveHost() {
    if (!this._IDNService)
      this._IDNService = Cc["@mozilla.org/network/idn-service;1"]
                         .getService(Ci.nsIIDNService);
    try {
      let baseDomain = Services.eTLD.getBaseDomainFromHost(this._lastLocation.hostname);
      return this._IDNService.convertToDisplayIDN(baseDomain, {});
    } catch (e) {
      // If something goes wrong (e.g. hostname is an IP address) just fail back
      // to the full domain.
      return this._lastLocation.hostname;
    }
  }
};

function OverscrollController(aTab) {
  this.tab = aTab;
}

OverscrollController.prototype = {
  supportsCommand : function supportsCommand(aCommand) {
    if (aCommand != "cmd_linePrevious" && aCommand != "cmd_scrollPageUp")
      return false;

    return (this.tab.getViewport().y == 0);
  },

  isCommandEnabled : function isCommandEnabled(aCommand) {
    return this.supportsCommand(aCommand);
  },

  doCommand : function doCommand(aCommand){
    sendMessageToJava({ gecko: { type: "ToggleChrome:Focus" } });
  },

  onEvent : function onEvent(aEvent) { }
};

var SearchEngines = {
  _contextMenuId: null,

  init: function init() {
    Services.obs.addObserver(this, "SearchEngines:Get", false);
    let contextName = Strings.browser.GetStringFromName("contextmenu.addSearchEngine");
    let filter = {
      matches: function (aElement) {
        return (aElement.form && NativeWindow.contextmenus.textContext.matches(aElement));
      }
    };
    this._contextMenuId = NativeWindow.contextmenus.add(contextName, filter, this.addEngine);
  },

  uninit: function uninit() {
    Services.obs.removeObserver(this, "SearchEngines:Get", false);
    if (this._contextMenuId != null)
      NativeWindow.contextmenus.remove(this._contextMenuId);
  },

  _handleSearchEnginesGet: function _handleSearchEnginesGet(rv) {
    if (!Components.isSuccessCode(rv)) {
      Cu.reportError("Could not initialize search service, bailing out.");
      return;
    }
    let engineData = Services.search.getVisibleEngines({});
    let searchEngines = engineData.map(function (engine) {
      return {
        name: engine.name,
        iconURI: (engine.iconURI ? engine.iconURI.spec : null)
      };
    });

    let suggestTemplate = null;
    let suggestEngine = null;
    if (Services.prefs.getBoolPref("browser.search.suggest.enabled")) {
      let engine = this.getSuggestionEngine();
      if (engine != null) {
        suggestEngine = engine.name;
        suggestTemplate = engine.getSubmission("__searchTerms__", "application/x-suggestions+json").uri.spec;
      }
    }

    sendMessageToJava({
      gecko: {
        type: "SearchEngines:Data",
        searchEngines: searchEngines,
        suggestEngine: suggestEngine,
        suggestTemplate: suggestTemplate
      }
    });
  },

  observe: function observe(aSubject, aTopic, aData) {
    if (aTopic == "SearchEngines:Get") {
      Services.search.init(this._handleSearchEnginesGet.bind(this));
    }
  },

  getSuggestionEngine: function () {
    let engines = [ Services.search.currentEngine,
                    Services.search.defaultEngine,
                    Services.search.originalDefaultEngine ];

    for (let i = 0; i < engines.length; i++) {
      let engine = engines[i];
      if (engine && engine.supportsResponseType("application/x-suggestions+json"))
        return engine;
    }

    return null;
  },

  addEngine: function addEngine(aElement) {
    let form = aElement.form;
    let charset = aElement.ownerDocument.characterSet;
    let docURI = Services.io.newURI(aElement.ownerDocument.URL, charset, null);
    let formURL = Services.io.newURI(form.getAttribute("action"), charset, docURI).spec;
    let method = form.method.toUpperCase();
    let formData = [];

    for each (let el in form.elements) {
      if (!el.type)
        continue;

      // make this text field a generic search parameter
      if (aElement == el) {
        formData.push({ name: el.name, value: "{searchTerms}" });
        continue;
      }

      let type = el.type.toLowerCase();
      let escapedName = escape(el.name);
      let escapedValue = escape(el.value);

      // add other form elements as parameters
      switch (el.type) {
        case "checkbox":
        case "radio":
          if (!el.checked) break;
        case "text":
        case "hidden":
        case "textarea":
          formData.push({ name: escapedName, value: escapedValue });
          break;
        case "select-one":
          for each (let option in el.options) {
            if (option.selected) {
              formData.push({ name: escapedName, value: escapedValue });
              break;
            }
          }
      }
    }

    // prompt user for name of search engine
    let promptTitle = Strings.browser.GetStringFromName("contextmenu.addSearchEngine");
    let title = { value: (aElement.ownerDocument.title || docURI.host) };
    if (!Services.prompt.prompt(null, promptTitle, null, title, null, {}))
      return;

    // fetch the favicon for this page
    let dbFile = FileUtils.getFile("ProfD", ["browser.db"]);
    let mDBConn = Services.storage.openDatabase(dbFile);
    let stmts = [];
    stmts[0] = mDBConn.createStatement("SELECT favicon FROM images WHERE url_key = ?");
    stmts[0].bindStringParameter(0, docURI.spec);
    let favicon = null;
    Services.search.init(function addEngine_cb(rv) {
      if (!Components.isSuccessCode(rv)) {
        Cu.reportError("Could not initialize search service, bailing out.");
        return;
      }
      mDBConn.executeAsync(stmts, stmts.length, {
        handleResult: function (results) {
          let bytes = results.getNextRow().getResultByName("favicon");
          favicon = "data:image/png;base64," + btoa(String.fromCharCode.apply(null, bytes));
        },
        handleCompletion: function (reason) {
          // if there's already an engine with this name, add a number to
          // make the name unique (e.g., "Google" becomes "Google 2")
          let name = title.value;
          for (let i = 2; Services.search.getEngineByName(name); i++)
            name = title.value + " " + i;

          Services.search.addEngineWithDetails(name, favicon, null, null, method, formURL);
          let engine = Services.search.getEngineByName(name);
          engine.wrappedJSObject._queryCharset = charset;
          for each (let param in formData) {
            if (param.name && param.value)
              engine.addParam(param.name, param.value, null);
          }
        }
      });
    });
  }
};

var ActivityObserver = {
  init: function ao_init() {
    Services.obs.addObserver(this, "application-background", false);
    Services.obs.addObserver(this, "application-foreground", false);
  },

  observe: function ao_observe(aSubject, aTopic, aData) {
    let isForeground = false
    switch (aTopic) {
      case "application-background" :
        isForeground = false;
        break;
      case "application-foreground" :
        isForeground = true;
        break;
    }

    let tab = BrowserApp.selectedTab;
    if (tab && tab.getActive() != isForeground) {
      tab.setActive(isForeground);
    }
  }
};

var WebappsUI = {
  init: function init() {
    Cu.import("resource://gre/modules/Webapps.jsm");
    DOMApplicationRegistry.allAppsLaunchable = true;

    Services.obs.addObserver(this, "webapps-ask-install", false);
    Services.obs.addObserver(this, "webapps-launch", false);
    Services.obs.addObserver(this, "webapps-sync-install", false);
    Services.obs.addObserver(this, "webapps-sync-uninstall", false);
  },
  
  uninit: function unint() {
    Services.obs.removeObserver(this, "webapps-ask-install");
    Services.obs.removeObserver(this, "webapps-launch");
    Services.obs.removeObserver(this, "webapps-sync-install");
    Services.obs.removeObserver(this, "webapps-sync-uninstall");
  },
  
  observe: function observe(aSubject, aTopic, aData) {
    let data = JSON.parse(aData);
    switch (aTopic) {
      case "webapps-ask-install":
        this.doInstall(data);
        break;
      case "webapps-launch":
        DOMApplicationRegistry.getManifestFor(data.origin, (function(aManifest) {
          if (!aManifest)
            return;
          let manifest = new DOMApplicationManifest(aManifest, data.origin);
          this.openURL(manifest.fullLaunchPath(), data.origin);
        }).bind(this));
        break;
      case "webapps-sync-install":
        // Wait until we know the app install worked, then make a homescreen shortcut
        DOMApplicationRegistry.getManifestFor(data.origin, (function(aManifest) {
          if (!aManifest)
            return;
          let manifest = new DOMApplicationManifest(aManifest, data.origin);

          // Add a homescreen shortcut -- we can't use createShortcut, since we need to pass
          // a unique ID for Android webapp allocation
          this.makeBase64Icon(this.getBiggestIcon(manifest.icons, Services.io.newURI(data.origin, null, null)),
                              function(icon) {
                                sendMessageToJava({
                                  gecko: {
                                    type: "WebApps:Install",
                                    name: manifest.name,
                                    launchPath: manifest.fullLaunchPath(),
                                    iconURL: icon,
                                    uniqueURI: data.origin
                                  }
                                })});

          // Create a system notification allowing the user to launch the app
          let observer = {
            observe: function (aSubject, aTopic) {
              if (aTopic == "alertclickcallback") {
                WebappsUI.openURL(manifest.fullLaunchPath(), data.origin);
              }
            }
          };

          let message = Strings.browser.GetStringFromName("webapps.alertSuccess");
          let alerts = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
          alerts.showAlertNotification("drawable://alert_app", manifest.name, message, true, "", observer, "webapp");
        }).bind(this));
        break;
      case "webapps-sync-uninstall":
        sendMessageToJava({
          gecko: {
            type: "WebApps:Uninstall",
            uniqueURI: data.origin
          }
        });
        break;
    }
  },

  getBiggestIcon: function getBiggestIcon(aIcons, aOrigin) {
    const DEFAULT_ICON = "chrome://browser/skin/images/default-app-icon.png";
    if (!aIcons)
      return DEFAULT_ICON;
  
    let iconSizes = Object.keys(aIcons);
    if (iconSizes.length == 0)
      return DEFAULT_ICON;
    iconSizes.sort(function(a, b) a - b);

    let biggestIcon = aIcons[iconSizes.pop()];
    let iconURI = null;
    try {
      iconURI = Services.io.newURI(biggestIcon, null, null);
      if (iconURI.scheme == "data") {
        return iconURI.spec;
      }
    } catch (ex) {
      // we don't have a biggestIcon or its not a valid url
    }

    // if we have an origin, try to resolve biggestIcon as a relative url
    if (!iconURI && aOrigin) {
      try {
        iconURI = Services.io.newURI(aOrigin.resolve(biggestIcon), null, null);
      } catch (ex) {
        console.log("Could not resolve url: " + aOrigin.spec + " " + biggestIcon + " - " + ex);
      }
    }

    return iconURI ? iconURI.spec : DEFAULT_ICON;
  },

  doInstall: function doInstall(aData) {
    let manifest = new DOMApplicationManifest(aData.app.manifest, aData.app.origin);
    let name = manifest.name ? manifest.name : manifest.fullLaunchPath();
    if (Services.prompt.confirm(null, Strings.browser.GetStringFromName("webapps.installTitle"), name))
      DOMApplicationRegistry.confirmInstall(aData);
    else
      DOMApplicationRegistry.denyInstall(aData);
  },
  
  openURL: function openURL(aURI, aOrigin) {
    sendMessageToJava({
      gecko: {
        type: "WebApps:Open",
        uri: aURI,
        origin: aOrigin
      }
    });
  },

  makeBase64Icon: function loadAndMakeBase64Icon(aIconURL, aCallbackFunction) {
    // The images are 64px, but Android will resize as needed.
    // Bigger is better than too small.
    const kIconSize = 64;

    let canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");

    canvas.width = canvas.height = kIconSize;
    let ctx = canvas.getContext("2d");
    let favicon = new Image();
    favicon.onload = function() {
      ctx.drawImage(favicon, 0, 0, kIconSize, kIconSize);
      let base64icon = canvas.toDataURL("image/png", "");
      canvas = null;
      aCallbackFunction.call(null, base64icon);
    };
    favicon.onerror = function() {
      Cu.reportError("CreateShortcut: favicon image load error");

      // if the image failed to load, and it was not our default icon, attempt to
      // use our default as a fallback
      let uri = Services.io.newURI(favicon.src, null, null);
      if (!/^chrome$/.test(uri.scheme)) {
        favicon.src = WebappsUI.getBiggestIcon(null);
      }
    };
  
    favicon.src = aIconURL;
  },

  createShortcut: function createShortcut(aTitle, aURL, aIconURL, aType) {
    this.makeBase64Icon(aIconURL, function _createShortcut(icon) {
      try {
        let shell = Cc["@mozilla.org/browser/shell-service;1"].createInstance(Ci.nsIShellService);
        shell.createShortcut(aTitle, aURL, icon, aType);
      } catch(e) {
        Cu.reportError(e);
      }
    });
  }
}

var RemoteDebugger = {
  init: function rd_init() {
    Services.prefs.addObserver("devtools.debugger.", this, false);

    if (this._isEnabled())
      this._start();
  },

  observe: function rd_observe(aSubject, aTopic, aData) {
    if (aTopic != "nsPref:changed")
      return;

    switch (aData) {
      case "devtools.debugger.remote-enabled":
        if (this._isEnabled())
          this._start();
        else
          this._stop();
        break;

      case "devtools.debugger.remote-port":
        if (this._isEnabled())
          this._restart();
        break;
    }
  },

  uninit: function rd_uninit() {
    Services.prefs.removeObserver("devtools.debugger.", this);
    this._stop();
  },

  _getPort: function _rd_getPort() {
    return Services.prefs.getIntPref("devtools.debugger.remote-port");
  },

  _isEnabled: function rd_isEnabled() {
    return Services.prefs.getBoolPref("devtools.debugger.remote-enabled");
  },

  /**
   * Prompt the user to accept or decline the incoming connection.
   *
   * @return true if the connection should be permitted, false otherwise
   */
  _allowConnection: function rd_allowConnection() {
    let title = Strings.browser.GetStringFromName("remoteIncomingPromptTitle");
    let msg = Strings.browser.GetStringFromName("remoteIncomingPromptMessage");
    let btn = Strings.browser.GetStringFromName("remoteIncomingPromptDisable");
    let prompt = Services.prompt;
    let flags = prompt.BUTTON_POS_0 * prompt.BUTTON_TITLE_OK +
                prompt.BUTTON_POS_1 * prompt.BUTTON_TITLE_CANCEL +
                prompt.BUTTON_POS_2 * prompt.BUTTON_TITLE_IS_STRING +
                prompt.BUTTON_POS_1_DEFAULT;
    let result = prompt.confirmEx(null, title, msg, flags, null, null, btn, null, { value: false });
    if (result == 0)
      return true;
    if (result == 2) {
      this._stop();
      Services.prefs.setBoolPref("devtools.debugger.remote-enabled", false);
    }
    return false;
  },

  _restart: function rd_restart() {
    this._stop();
    this._start();
  },

  _start: function rd_start() {
    try {
      if (!DebuggerServer.initialized) {
        DebuggerServer.init(this._allowConnection);
        DebuggerServer.addActors("chrome://browser/content/dbg-browser-actors.js");
      }

      let port = this._getPort();
      DebuggerServer.openListener(port);
      dump("Remote debugger listening on port " + port);
    } catch(e) {
      dump("Remote debugger didn't start: " + e);
    }
  },

  _stop: function rd_start() {
    DebuggerServer.closeListener();
    dump("Remote debugger stopped");
  }
};

var Telemetry = {
  _PREF_TELEMETRY_PROMPTED: "toolkit.telemetry.prompted",
  _PREF_TELEMETRY_ENABLED: "toolkit.telemetry.enabled",
  _PREF_TELEMETRY_REJECTED: "toolkit.telemetry.rejected",
  _PREF_TELEMETRY_SERVER_OWNER: "toolkit.telemetry.server_owner",

  // This is used to reprompt users when privacy message changes
  _TELEMETRY_PROMPT_REV: 2,

  init: function init() {
    Services.obs.addObserver(this, "Preferences:Set", false);
    Services.obs.addObserver(this, "Telemetry:Add", false);
  },

  uninit: function uninit() {
    Services.obs.removeObserver(this, "Preferences:Set");
    Services.obs.removeObserver(this, "Telemetry:Add");
  },

  observe: function observe(aSubject, aTopic, aData) {
    if (aTopic == "Preferences:Set") {
      // if user changes telemetry pref, treat it like they have been prompted
      let pref = JSON.parse(aData);
      if (pref.name == this._PREF_TELEMETRY_ENABLED)
        Services.prefs.setIntPref(this._PREF_TELEMETRY_PROMPTED, this._TELEMETRY_PROMPT_REV);
    } else if (aTopic == "Telemetry:Add") {
      let json = JSON.parse(aData);
      var telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
      let histogram = telemetry.getHistogramById(json.name);
      histogram.add(json.value);
    }
  },

  prompt: function prompt() {
    let serverOwner = Services.prefs.getCharPref(this._PREF_TELEMETRY_SERVER_OWNER);
    let telemetryPrompted = null;
    let self = this;
    try {
      telemetryPrompted = Services.prefs.getIntPref(this._PREF_TELEMETRY_PROMPTED);
    } catch (e) { /* Optional */ }

    // If the user has seen the latest telemetry prompt, do not prompt again
    // else clear old prefs and reprompt
    if (telemetryPrompted === this._TELEMETRY_PROMPT_REV)
      return;

    Services.prefs.clearUserPref(this._PREF_TELEMETRY_PROMPTED);
    Services.prefs.clearUserPref(this._PREF_TELEMETRY_ENABLED);

    let buttons = [
      {
        label: Strings.browser.GetStringFromName("telemetry.optin.yes"),
        callback: function () {
          Services.prefs.setIntPref(self._PREF_TELEMETRY_PROMPTED, self._TELEMETRY_PROMPT_REV);
          Services.prefs.setBoolPref(self._PREF_TELEMETRY_ENABLED, true);
        }
      },
      {
        label: Strings.browser.GetStringFromName("telemetry.optin.no"),
        callback: function () {
          Services.prefs.setIntPref(self._PREF_TELEMETRY_PROMPTED, self._TELEMETRY_PROMPT_REV);
          Services.prefs.setBoolPref(self._PREF_TELEMETRY_REJECTED, true);
        }
      }
    ];

    let brandShortName = Strings.brand.GetStringFromName("brandShortName");
    let message = Strings.browser.formatStringFromName("telemetry.optin.message2", [serverOwner, brandShortName], 2);
    let learnMoreLabel = Strings.browser.GetStringFromName("telemetry.optin.learnMore");
    let learnMoreUrl = Services.urlFormatter.formatURLPref("app.support.baseURL");
    learnMoreUrl += "how-can-i-help-submitting-performance-data";
    let options = {
      link: {
        label: learnMoreLabel,
        url: learnMoreUrl
      },
      // We're adding this doorhanger during startup, before the initial onLocationChange
      // event fires, so we need to set persistence to make sure it doesn't disappear.
      persistence: 1
    };
    NativeWindow.doorhanger.show(message, "telemetry-optin", buttons, BrowserApp.selectedTab.id, options);
  },
};

let Reader = {
  // Version of the cache database schema
  DB_VERSION: 1,

  DEBUG: 1,

  init: function Reader_init() {
    this.log("Init()");
    this._requests = {};

    Services.obs.addObserver(this, "Reader:Add", false);
    Services.obs.addObserver(this, "Reader:Remove", false);
  },

  observe: function(aMessage, aTopic, aData) {
    switch(aTopic) {
      case "Reader:Add": {
        let tab = BrowserApp.getTabForId(aData);
        let url = tab.browser.contentWindow.location.href;

        let sendResult = function(success, title) {
          this.log("Reader:Add success=" + success + ", url=" + url + ", title=" + title);

          sendMessageToJava({
            gecko: {
              type: "Reader:Added",
              success: success,
              title: title,
              url: url,
            }
          });
        }.bind(this);

        this.parseDocumentFromTab(aData, function(article) {
          if (!article) {
            sendResult(false, "");
            return;
          }

          this.storeArticleInCache(article, function(success) {
            sendResult(success, article.title);
          });
        }.bind(this));
        break;
      }

      case "Reader:Remove": {
        this.removeArticleFromCache(aData, function(success) {
          this.log("Reader:Remove success=" + success + ", url=" + aData);
        }.bind(this));
        break;
      }
    }
  },

  parseDocumentFromURL: function Reader_parseDocumentFromURL(url, callback) {
    // If there's an on-going request for the same URL, simply append one
    // more callback to it to be called when the request is done.
    if (url in this._requests) {
      let request = this._requests[url];
      request.callbacks.push(callback);
      return;
    }

    let request = { url: url, callbacks: [callback] };
    this._requests[url] = request;

    try {
      this.log("parseDocumentFromURL: " + url);

      // First, try to find a cached parsed article in the DB
      this.getArticleFromCache(url, function(article) {
        if (article) {
          this.log("Page found in cache, return article immediately");
          this._runCallbacksAndFinish(request, article);
          return;
        }

        if (!this._requests) {
          this.log("Reader has been destroyed, abort");
          return;
        }

        // Article hasn't been found in the cache DB, we need to
        // download the page and parse the article out of it.
        this._downloadAndParseDocument(url, request);
      }.bind(this));
    } catch (e) {
      this.log("Error parsing document from URL: " + e);
      this._runCallbacksAndFinish(request, null);
    }
  },

  parseDocumentFromTab: function(tabId, callback) {
    try {
      this.log("parseDocumentFromTab: " + tabId);

      let tab = BrowserApp.getTabForId(tabId);
      let url = tab.browser.contentWindow.location.href;
      let uri = Services.io.newURI(url, null, null);

      if (!(uri.schemeIs("http") || uri.schemeIs("https") || uri.schemeIs("file"))) {
        this.log("Not parsing URI scheme: " + uri.scheme);
        callback(null);
        return;
      }

      // First, try to find a cached parsed article in the DB
      this.getArticleFromCache(url, function(article) {
        if (article) {
          this.log("Page found in cache, return article immediately");
          callback(article);
          return;
        }

        // We need to clone the document before parsing because readability
        // changes the document object in several ways to find the article
        // in it.
        let doc = tab.browser.contentWindow.document.cloneNode(true);

        let readability = new Readability(uri, doc);
        article = readability.parse();

        if (!article) {
          this.log("Failed to parse page");
          callback(null);
          return;
        }

        // Append URL to the article data
        article.url = url;

        callback(article);
      }.bind(this));
    } catch (e) {
      this.log("Error parsing document from tab: " + e);
      callback(null);
    }
  },

  checkTabReadability: function Reader_checkTabReadability(tabId, callback) {
    try {
      this.log("checkTabReadability: " + tabId);

      let tab = BrowserApp.getTabForId(tabId);
      let url = tab.browser.contentWindow.location.href;

      // First, try to find a cached parsed article in the DB
      this.getArticleFromCache(url, function(article) {
        if (article) {
          this.log("Page found in cache, page is definitely readable");
          callback(true);
          return;
        }

        let uri = Services.io.newURI(url, null, null);
        let doc = tab.browser.contentWindow.document;

        let readability = new Readability(uri, doc);
        callback(readability.check());
      }.bind(this));
    } catch (e) {
      this.log("Error checking tab readability: " + e);
      callback(false);
    }
  },

  getArticleFromCache: function Reader_getArticleFromCache(url, callback) {
    this._getCacheDB(function(cacheDB) {
      if (!cacheDB) {
        callback(false);
        return;
      }

      let transaction = cacheDB.transaction(cacheDB.objectStoreNames);
      let articles = transaction.objectStore(cacheDB.objectStoreNames[0]);

      let request = articles.get(url);

      request.onerror = function(event) {
        this.log("Error getting article from the cache DB: " + url);
        callback(null);
      }.bind(this);

      request.onsuccess = function(event) {
        this.log("Got article from the cache DB: " + event.target.result);
        callback(event.target.result);
      }.bind(this);
    }.bind(this));
  },

  storeArticleInCache: function Reader_storeArticleInCache(article, callback) {
    this._getCacheDB(function(cacheDB) {
      if (!cacheDB) {
        callback(false);
        return;
      }

      let transaction = cacheDB.transaction(cacheDB.objectStoreNames, "readwrite");
      let articles = transaction.objectStore(cacheDB.objectStoreNames[0]);

      let request = articles.add(article);

      request.onerror = function(event) {
        this.log("Error storing article in the cache DB: " + article.url);
        callback(false);
      }.bind(this);

      request.onsuccess = function(event) {
        this.log("Stored article in the cache DB: " + article.url);
        callback(true);
      }.bind(this);
    }.bind(this));
  },

  removeArticleFromCache: function Reader_removeArticleFromCache(url, callback) {
    this._getCacheDB(function(cacheDB) {
      if (!cacheDB) {
        callback(false);
        return;
      }

      let transaction = cacheDB.transaction(cacheDB.objectStoreNames, "readwrite");
      let articles = transaction.objectStore(cacheDB.objectStoreNames[0]);

      let request = articles.delete(url);

      request.onerror = function(event) {
        this.log("Error removing article from the cache DB: " + url);
        callback(false);
      }.bind(this);

      request.onsuccess = function(event) {
        this.log("Removed article from the cache DB: " + url);
        callback(true);
      }.bind(this);
    }.bind(this));
  },

  uninit: function Reader_uninit() {
    Services.obs.removeObserver(this, "Reader:Add", false);
    Services.obs.removeObserver(this, "Reader:Remove", false);

    let requests = this._requests;
    for (let url in requests) {
      let request = requests[url];
      if (request.browser) {
        let browser = request.browser;
        browser.parentNode.removeChild(browser);
      }
    }
    delete this._requests;

    if (this._cacheDB) {
      this._cacheDB.close();
      delete this._cacheDB;
    }
  },

  log: function(msg) {
    if (this.DEBUG)
      dump("Reader: " + msg);
  },

  _runCallbacksAndFinish: function Reader_runCallbacksAndFinish(request, result) {
    delete this._requests[request.url];

    request.callbacks.forEach(function(callback) {
      callback(result);
    });
  },

  _downloadDocument: function Reader_downloadDocument(url, callback) {
    // We want to parse those arbitrary pages safely, outside the privileged
    // context of chrome. We create a hidden browser element to fetch the
    // loaded page's document object then discard the browser element.

    let browser = document.createElement("browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("collapsed", "true");

    document.documentElement.appendChild(browser);
    browser.stop();

    browser.webNavigation.allowAuth = false;
    browser.webNavigation.allowImages = false;
    browser.webNavigation.allowJavascript = false;
    browser.webNavigation.allowMetaRedirects = true;
    browser.webNavigation.allowPlugins = false;

    browser.addEventListener("DOMContentLoaded", function (event) {
      let doc = event.originalTarget;
      this.log("Done loading: " + doc);
      if (doc.location.href == "about:blank" || doc.defaultView.frameElement) {
        callback(null);

        // Request has finished with error, remove browser element
        browser.parentNode.removeChild(browser);
        return;
      }

      callback(doc);

      // Request has finished, remove browser element
      browser.parentNode.removeChild(browser);
    }.bind(this));

    browser.loadURIWithFlags(url, Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                             null, null, null);

    return browser;
  },

  _downloadAndParseDocument: function Reader_downloadAndParseDocument(url, request) {
    try {
      this.log("Needs to fetch page, creating request: " + url);

      request.browser = this._downloadDocument(url, function(doc) {
        this.log("Finished loading page: " + doc);

        // Delete reference to the browser element as we're
        // now done with this request.
        delete request.browser;

        if (!doc) {
          this.log("Error loading page");
          this._runCallbacksAndFinish(request, null);
        }

        this.log("Parsing response with Readability");

        let uri = Services.io.newURI(url, null, null);
        let readability = new Readability(uri, doc);
        let article = readability.parse();

        if (!article) {
          this.log("Failed to parse page");
          this._runCallbacksAndFinish(request, null);
          return;
        }

        this.log("Parsing has been successful");

        // Append URL to the article data
        article.url = url;

        this._runCallbacksAndFinish(request, article);
      }.bind(this));
    } catch (e) {
      this.log("Error downloading and parsing document: " + e);
      this._runCallbacksAndFinish(request, null);
    }
  },

  _getCacheDB: function Reader_getCacheDB(callback) {
    if (this._cacheDB) {
      callback(this._cacheDB);
      return;
    }

    let request = window.indexedDB.open("about:reader", this.DB_VERSION);

    request.onerror = function(event) {
      this.log("Error connecting to the cache DB");
      this._cacheDB = null;
      callback(null);
    }.bind(this);

    request.onsuccess = function(event) {
      this.log("Successfully connected to the cache DB");
      this._cacheDB = event.target.result;
      callback(this._cacheDB);
    }.bind(this);

    request.onupgradeneeded = function(event) {
      this.log("Database schema upgrade from " +
           event.oldVersion + " to " + event.newVersion);

      let cacheDB = event.target.result;

      // Create the articles object store
      this.log("Creating articles object store");
      cacheDB.createObjectStore("articles", { keyPath: "url" });

      this.log("Database upgrade done: " + this.DB_VERSION);
    }.bind(this);
  }
};

var ExternalApps = {
  _contextMenuId: -1,

  init: function helper_init() {
    this._contextMenuId = NativeWindow.contextmenus.add(function(aElement) {
      let uri = null;
      var node = aElement;
      while (node && !uri) {
        uri = NativeWindow.contextmenus._getLink(node);
        node = node.parentNode;
      }
      let apps = [];
      if (uri)
        apps = HelperApps.getAppsForUri(uri);

      return apps.length == 1 ? Strings.browser.formatStringFromName("helperapps.openWithApp2", [apps[0].name], 1) :
                                Strings.browser.GetStringFromName("helperapps.openWithList2");
    }, this.filter, this.openExternal);
  },

  uninit: function helper_uninit() {
    NativeWindow.contextmenus.remove(this._contextMenuId);
  },

  filter: {
    matches: function(aElement) {
      let uri = NativeWindow.contextmenus._getLink(aElement);
      let apps = [];
      if (uri) {
        apps = HelperApps.getAppsForUri(uri);
      }
      return apps.length > 0;
    }
  },

  openExternal: function(aElement) {
    let uri = NativeWindow.contextmenus._getLink(aElement);
    HelperApps.openUriInApp(uri);
  }
}
