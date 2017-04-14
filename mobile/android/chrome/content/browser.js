// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/AsyncPrefs.jsm");
Cu.import("resource://gre/modules/DelayedInit.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/TelemetryController.jsm");

if (AppConstants.ACCESSIBILITY) {
  XPCOMUtils.defineLazyModuleGetter(this, "AccessFu",
                                    "resource://gre/modules/accessibility/AccessFu.jsm");
}

XPCOMUtils.defineLazyModuleGetter(this, "Manifests",
                                  "resource://gre/modules/Manifest.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadNotifications",
                                  "resource://gre/modules/DownloadNotifications.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "JNI",
                                  "resource://gre/modules/JNI.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UITelemetry",
                                  "resource://gre/modules/UITelemetry.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UserAgentOverrides",
                                  "resource://gre/modules/UserAgentOverrides.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoginManagerContent",
                                  "resource://gre/modules/LoginManagerContent.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoginManagerParent",
                                  "resource://gre/modules/LoginManagerParent.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Task", "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SafeBrowsing",
                                  "resource://gre/modules/SafeBrowsing.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
                                  "resource://gre/modules/BrowserUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Sanitizer",
                                  "resource://gre/modules/Sanitizer.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Prompt",
                                  "resource://gre/modules/Prompt.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "HelperApps",
                                  "resource://gre/modules/HelperApps.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SSLExceptions",
                                  "resource://gre/modules/SSLExceptions.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormHistory",
                                  "resource://gre/modules/FormHistory.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

if (AppConstants.MOZ_ENABLE_PROFILER_SPS) {
  XPCOMUtils.defineLazyServiceGetter(this, "Profiler",
                                     "@mozilla.org/tools/profiler;1",
                                     "nsIProfiler");
}

XPCOMUtils.defineLazyModuleGetter(this, "SimpleServiceDiscovery",
                                  "resource://gre/modules/SimpleServiceDiscovery.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CharsetMenu",
                                  "resource://gre/modules/CharsetMenu.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetErrorHelper",
                                  "resource://gre/modules/NetErrorHelper.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PermissionsUtils",
                                  "resource://gre/modules/PermissionsUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SharedPreferences",
                                  "resource://gre/modules/SharedPreferences.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Notifications",
                                  "resource://gre/modules/Notifications.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode", "resource://gre/modules/ReaderMode.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Snackbars", "resource://gre/modules/Snackbars.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "RuntimePermissions", "resource://gre/modules/RuntimePermissions.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebsiteMetadata", "resource://gre/modules/WebsiteMetadata.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch", "resource://gre/modules/TelemetryStopwatch.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "FontEnumerator",
  "@mozilla.org/gfx/fontenumerator;1",
  "nsIFontEnumerator");

var GlobalEventDispatcher = EventDispatcher.instance;
var WindowEventDispatcher = EventDispatcher.for(window);

var lazilyLoadedBrowserScripts = [
  ["SelectHelper", "chrome://browser/content/SelectHelper.js"],
  ["InputWidgetHelper", "chrome://browser/content/InputWidgetHelper.js"],
  ["MasterPassword", "chrome://browser/content/MasterPassword.js"],
  ["PluginHelper", "chrome://browser/content/PluginHelper.js"],
  ["OfflineApps", "chrome://browser/content/OfflineApps.js"],
  ["Linkifier", "chrome://browser/content/Linkify.js"],
  ["CastingApps", "chrome://browser/content/CastingApps.js"],
  ["RemoteDebugger", "chrome://browser/content/RemoteDebugger.js"],
];
if (!AppConstants.RELEASE_OR_BETA) {
  lazilyLoadedBrowserScripts.push(
    ["WebcompatReporter", "chrome://browser/content/WebcompatReporter.js"]);
}

lazilyLoadedBrowserScripts.forEach(function (aScript) {
  let [name, script] = aScript;
  XPCOMUtils.defineLazyGetter(window, name, function() {
    let sandbox = {};
    Services.scriptloader.loadSubScript(script, sandbox);
    return sandbox[name];
  });
});

var lazilyLoadedObserverScripts = [
  ["MemoryObserver", ["memory-pressure", "Memory:Dump"], "chrome://browser/content/MemoryObserver.js"],
  ["ConsoleAPI", ["console-api-log-event"], "chrome://browser/content/ConsoleAPI.js"],
];

if (AppConstants.MOZ_WEBRTC) {
  lazilyLoadedObserverScripts.push(
    ["WebrtcUI", ["getUserMedia:request",
                  "PeerConnection:request",
                  "recording-device-events",
                  "VideoCapture:Paused",
                  "VideoCapture:Resumed"], "chrome://browser/content/WebrtcUI.js"])
}

lazilyLoadedObserverScripts.forEach(function (aScript) {
  let [name, notifications, script] = aScript;
  XPCOMUtils.defineLazyGetter(window, name, function() {
    let sandbox = {};
    Services.scriptloader.loadSubScript(script, sandbox);
    return sandbox[name];
  });
  let observer = (s, t, d) => {
    Services.obs.removeObserver(observer, t);
    Services.obs.addObserver(window[name], t);
    window[name].observe(s, t, d); // Explicitly notify new observer
  };
  notifications.forEach((notification) => {
    Services.obs.addObserver(observer, notification);
  });
});

// Lazily-loaded browser scripts that use message listeners.
[
  ["Reader", [
    ["Reader:AddToCache", false],
    ["Reader:RemoveFromCache", false],
    ["Reader:ArticleGet", false],
    ["Reader:DropdownClosed", true], // 'true' allows us to survive mid-air cycle-collection.
    ["Reader:DropdownOpened", false],
    ["Reader:FaviconRequest", false],
    ["Reader:ToolbarHidden", false],
    ["Reader:SystemUIVisibility", false],
    ["Reader:UpdateReaderButton", false],
  ], "chrome://browser/content/Reader.js"],
].forEach(aScript => {
  let [name, messages, script] = aScript;
  XPCOMUtils.defineLazyGetter(window, name, function() {
    let sandbox = {};
    Services.scriptloader.loadSubScript(script, sandbox);
    return sandbox[name];
  });

  let mm = window.getGroupMessageManager("browsers");
  let listener = (message) => {
    mm.removeMessageListener(message.name, listener);
    let listenAfterClose = false;
    for (let [name, laClose] of messages) {
      if (message.name === name) {
        listenAfterClose = laClose;
        break;
      }
    }

    mm.addMessageListener(message.name, window[name], listenAfterClose);
    window[name].receiveMessage(message);
  };

  messages.forEach((message) => {
    let [name, listenAfterClose] = message;
    mm.addMessageListener(name, listener, listenAfterClose);
  });
});

// Lazily-loaded JS subscripts and modules that use global/window EventDispatcher.
[
  ["ActionBarHandler", WindowEventDispatcher,
   ["TextSelection:Get", "TextSelection:Action", "TextSelection:End"],
   "chrome://browser/content/ActionBarHandler.js"],
  ["EmbedRT", WindowEventDispatcher,
   ["GeckoView:ImportScript"],
   "chrome://browser/content/EmbedRT.js"],
  ["Feedback", GlobalEventDispatcher,
   ["Feedback:Show"],
   "chrome://browser/content/Feedback.js"],
  ["FeedHandler", GlobalEventDispatcher,
   ["Feeds:Subscribe"],
   "chrome://browser/content/FeedHandler.js"],
  ["FindHelper", GlobalEventDispatcher,
   ["FindInPage:Opened", "FindInPage:Closed"],
   "chrome://browser/content/FindHelper.js"],
  ["Home", GlobalEventDispatcher,
   ["HomeBanner:Get", "HomePanels:Get", "HomePanels:Authenticate",
    "HomePanels:RefreshView", "HomePanels:Installed", "HomePanels:Uninstalled"],
   "resource://gre/modules/Home.jsm"],
  ["PermissionsHelper", GlobalEventDispatcher,
   ["Permissions:Check", "Permissions:Get", "Permissions:Clear"],
   "chrome://browser/content/PermissionsHelper.js"],
  ["PrintHelper", GlobalEventDispatcher,
   ["Print:PDF"],
   "chrome://browser/content/PrintHelper.js"],
  ["Reader", GlobalEventDispatcher,
   ["Reader:AddToCache", "Reader:RemoveFromCache"],
   "chrome://browser/content/Reader.js"],
].forEach(module => {
  let [name, dispatcher, events, script] = module;
  XPCOMUtils.defineLazyGetter(window, name, function() {
    let sandbox = {};
    if (script.endsWith(".jsm")) {
      Cu.import(script, sandbox);
    } else {
      Services.scriptloader.loadSubScript(script, sandbox);
    }
    return sandbox[name];
  });
  let listener = (event, message, callback) => {
    dispatcher.unregisterListener(listener, event);
    dispatcher.registerListener(window[name], event);
    window[name].onEvent(event, message, callback); // Explicitly notify new listener
  };
  dispatcher.registerListener(listener, events);
});

XPCOMUtils.defineLazyServiceGetter(this, "Haptic",
  "@mozilla.org/widget/hapticfeedback;1", "nsIHapticFeedback");

XPCOMUtils.defineLazyServiceGetter(this, "ParentalControls",
  "@mozilla.org/parental-controls-service;1", "nsIParentalControlsService");

XPCOMUtils.defineLazyServiceGetter(this, "DOMUtils",
  "@mozilla.org/inspector/dom-utils;1", "inIDOMUtils");

XPCOMUtils.defineLazyServiceGetter(window, "URIFixup",
  "@mozilla.org/docshell/urifixup;1", "nsIURIFixup");

if (AppConstants.MOZ_WEBRTC) {
  XPCOMUtils.defineLazyServiceGetter(this, "MediaManagerService",
    "@mozilla.org/mediaManagerService;1", "nsIMediaManagerService");
}

XPCOMUtils.defineLazyModuleGetter(this, "Log",
  "resource://gre/modules/AndroidLog.jsm", "AndroidLog");

// Define the "dump" function as a binding of the Log.d function so it specifies
// the "debug" priority and a log tag.
function dump(msg) {
  Log.d("Browser", msg);
}

const kStateActive = 0x00000001; // :active pseudoclass for elements

const kXLinkNamespace = "http://www.w3.org/1999/xlink";

function fuzzyEquals(a, b) {
  return (Math.abs(a - b) < 1e-6);
}

XPCOMUtils.defineLazyGetter(this, "ContentAreaUtils", function() {
  let ContentAreaUtils = {};
  Services.scriptloader.loadSubScript("chrome://global/content/contentAreaUtils.js", ContentAreaUtils);
  return ContentAreaUtils;
});

XPCOMUtils.defineLazyModuleGetter(this, "Rect", "resource://gre/modules/Geometry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Point", "resource://gre/modules/Geometry.jsm");

function resolveGeckoURI(aURI) {
  if (!aURI)
    throw "Can't resolve an empty uri";

  if (aURI.startsWith("chrome://")) {
    let registry = Cc['@mozilla.org/chrome/chrome-registry;1'].getService(Ci["nsIChromeRegistry"]);
    return registry.convertChromeURL(Services.io.newURI(aURI)).spec;
  } else if (aURI.startsWith("resource://")) {
    let handler = Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler);
    return handler.resolveURI(Services.io.newURI(aURI));
  }
  return aURI;
}

/**
 * Cache of commonly used string bundles.
 */
var Strings = {
  init: function () {
    XPCOMUtils.defineLazyGetter(Strings, "brand", () => Services.strings.createBundle("chrome://branding/locale/brand.properties"));
    XPCOMUtils.defineLazyGetter(Strings, "browser", () => Services.strings.createBundle("chrome://browser/locale/browser.properties"));
    XPCOMUtils.defineLazyGetter(Strings, "reader", () => Services.strings.createBundle("chrome://global/locale/aboutReader.properties"));
  },

  flush: function () {
    Services.strings.flushBundles();
    this.init();
  },
};

Strings.init();

const kFormHelperModeDisabled = 0;
const kFormHelperModeEnabled = 1;
const kFormHelperModeDynamic = 2;   // disabled on tablets
const kMaxHistoryListSize = 50;

function InitLater(fn, object, name) {
  return DelayedInit.schedule(fn, object, name, 15000 /* 15s max wait */);
}

var BrowserApp = {
  _tabs: [],
  _selectedTab: null,

  get isTablet() {
    let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
    delete this.isTablet;
    return this.isTablet = sysInfo.get("tablet");
  },

  get isOnLowMemoryPlatform() {
    let memory = Cc["@mozilla.org/xpcom/memory-service;1"].getService(Ci.nsIMemory);
    delete this.isOnLowMemoryPlatform;
    return this.isOnLowMemoryPlatform = memory.isLowMemoryPlatform();
  },

  // Note that the deck list order does not necessarily reflect the user visible tab order (see
  // bug 1331154 for the reason), so deck.selectedIndex should not be used (though
  // deck.selectedPanel is still valid) - use selectedTabIndex instead.
  deck: null,

  startup: function startup() {
    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = new nsBrowserAccess();
    dump("zerdatime " + Date.now() + " - browser chrome startup finished.");
    Services.obs.notifyObservers(this.browser, "BrowserChrome:Ready", null);

    this.deck = document.getElementById("browsers");

    BrowserEventHandler.init();

    ViewportHandler.init();

    Services.androidBridge.browserApp = this;

    GlobalEventDispatcher.registerListener(this, [
      "Tab:Load",
      "Tab:Selected",
      "Tab:Closed",
      "Tab:Move",
      "Browser:LoadManifest",
      "Browser:Quit",
      "Fonts:Reload",
      "FormHistory:Init",
      "FullScreen:Exit",
      "Locale:OS",
      "Locale:Changed",
      "Passwords:Init",
      "Sanitize:ClearData",
      "SaveAs:PDF",
      "ScrollTo:FocusedInput",
      "Session:Back",
      "Session:Forward",
      "Session:GetHistory",
      "Session:Navigate",
      "Session:Reload",
      "Session:Stop",
      "Telemetry:CustomTabsPing",
    ]);

    // Provide compatibility for add-ons like QuitNow that send "Browser:Quit"
    // as an observer notification.
    Services.obs.addObserver((subject, topic, data) =>
        this.quit(data ? JSON.parse(data) : undefined), "Browser:Quit");

    Services.obs.addObserver(this, "android-get-pref");
    Services.obs.addObserver(this, "android-set-pref");
    Services.obs.addObserver(this, "gather-telemetry");
    Services.obs.addObserver(this, "keyword-search");
    Services.obs.addObserver(this, "Vibration:Request");

    window.addEventListener("fullscreen", function() {
      WindowEventDispatcher.sendRequest({
        type: window.fullScreen ? "ToggleChrome:Hide" : "ToggleChrome:Show"
      });
    });

    window.addEventListener("fullscreenchange", (e) => {
      // This event gets fired on the document and its entire ancestor chain
      // of documents. When enabling fullscreen, it is fired on the top-level
      // document first and goes down; when disabling the order is reversed
      // (per spec). This means the last event on enabling will be for the innermost
      // document, which will have fullscreenElement set correctly.
      let doc = e.target;
      WindowEventDispatcher.sendRequest({
        type: doc.fullscreenElement ? "DOMFullScreen:Start" : "DOMFullScreen:Stop",
        rootElement: doc.fullscreenElement == doc.documentElement
      });

      if (this.fullscreenTransitionTab) {
        // Tab selection has changed during a fullscreen transition, handle it now.
        let tab = this.fullscreenTransitionTab;
        this.fullscreenTransitionTab = null;
        this.selectTab(tab);
      }
    });

    NativeWindow.init();
    FormAssistant.init();
    IndexedDB.init();
    XPInstallObserver.init();
    CharacterEncoding.init();
    ActivityObserver.init();
    RemoteDebugger.init();
    UserAgentOverrides.init();
    DesktopUserAgent.init();
    Distribution.init();
    Tabs.init();
    SearchEngines.init();
    Experiments.init();

    // XXX maybe we don't do this if the launch was kicked off from external
    Services.io.offline = false;

    // Broadcast a UIReady message so add-ons know we are finished with startup
    let event = document.createEvent("Events");
    event.initEvent("UIReady", true, false);
    window.dispatchEvent(event);

    if (this._startupStatus) {
      this.onAppUpdated();
    }

    if (!ParentalControls.isAllowed(ParentalControls.INSTALL_EXTENSION)) {
      // Disable extension installs
      Services.prefs.setIntPref("extensions.enabledScopes", 1);
      Services.prefs.setIntPref("extensions.autoDisableScopes", 1);
      Services.prefs.setBoolPref("xpinstall.enabled", false);
    } else if (ParentalControls.parentalControlsEnabled) {
      Services.prefs.clearUserPref("extensions.enabledScopes");
      Services.prefs.clearUserPref("extensions.autoDisableScopes");
      Services.prefs.setBoolPref("xpinstall.enabled", true);
    }

    if (ParentalControls.parentalControlsEnabled) {
        let isBlockListEnabled = ParentalControls.isAllowed(ParentalControls.BLOCK_LIST);
        Services.prefs.setBoolPref("browser.safebrowsing.forbiddenURIs.enabled", isBlockListEnabled);
        Services.prefs.setBoolPref("browser.safebrowsing.allowOverride", !isBlockListEnabled);

        let isTelemetryEnabled = ParentalControls.isAllowed(ParentalControls.TELEMETRY);
        Services.prefs.setBoolPref("toolkit.telemetry.enabled", isTelemetryEnabled);

        let isHealthReportEnabled = ParentalControls.isAllowed(ParentalControls.HEALTH_REPORT);
        SharedPreferences.forApp().setBoolPref("android.not_a_preference.healthreport.uploadEnabled", isHealthReportEnabled);
    }

    let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
    if (sysInfo.get("version") < 16) {
      let defaults = Services.prefs.getDefaultBranch(null);
      defaults.setBoolPref("media.autoplay.enabled", false);
    }

    InitLater(() => {
      // The order that context menu items are added is important
      // Make sure the "Open in App" context menu item appears at the bottom of the list
      this.initContextMenu();
      ExternalApps.init();
    }, NativeWindow, "contextmenus");

    if (AppConstants.ACCESSIBILITY) {
      InitLater(() => AccessFu.attach(window), window, "AccessFu");
    }

    // Don't delay loading content.js because when we restore reader mode tabs,
    // we require the reader mode scripts in content.js right away.
    let mm = window.getGroupMessageManager("browsers");
    mm.loadFrameScript("chrome://browser/content/content.js", true);

    // Listen to manifest messages
    mm.loadFrameScript("chrome://global/content/manifestMessages.js", true);

    // We can't delay registering WebChannel listeners: if the first page is
    // about:accounts, which can happen when starting the Firefox Account flow
    // from the first run experience, or via the Firefox Account Status
    // Activity, we can and do miss messages from the fxa-content-server.
    // However, we never allow suitably restricted profiles from listening to
    // fxa-content-server messages.
    if (ParentalControls.isAllowed(ParentalControls.MODIFY_ACCOUNTS)) {
      console.log("browser.js: loading Firefox Accounts WebChannel");
      Cu.import("resource://gre/modules/FxAccountsWebChannel.jsm");
      EnsureFxAccountsWebChannel();
    } else {
      console.log("browser.js: not loading Firefox Accounts WebChannel; this profile cannot connect to Firefox Accounts.");
    }

    // Notify Java that Gecko has loaded.
    GlobalEventDispatcher.sendRequest({ type: "Gecko:Ready" });

    this.deck.addEventListener("DOMContentLoaded", function() {
      InitLater(() => Cu.import("resource://gre/modules/NotificationDB.jsm"));

      InitLater(() => Services.obs.notifyObservers(window, "browser-delayed-startup-finished", ""));
      InitLater(() => GlobalEventDispatcher.sendRequest({ type: "Gecko:DelayedStartup" }));

      if (!AppConstants.RELEASE_OR_BETA) {
        InitLater(() => WebcompatReporter.init());
      }

      // Collect telemetry data.
      // We do this at startup because we want to move away from "gather-telemetry" (bug 1127907)
      InitLater(() => {
        Telemetry.addData("FENNEC_TRACKING_PROTECTION_STATE", parseInt(BrowserApp.getTrackingProtectionState()));
        Telemetry.addData("ZOOMED_VIEW_ENABLED", Services.prefs.getBoolPref("ui.zoomedview.enabled"));
      });

      InitLater(() => LightWeightThemeWebInstaller.init());
      InitLater(() => CastingApps.init(), window, "CastingApps");
      InitLater(() => Services.search.init(), Services, "search");
      InitLater(() => DownloadNotifications.init(), window, "DownloadNotifications");

      // Bug 778855 - Perf regression if we do this here. To be addressed in bug 779008.
      InitLater(() => SafeBrowsing.init(), window, "SafeBrowsing");

      InitLater(() => Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager));
      InitLater(() => LoginManagerParent.init(), window, "LoginManagerParent");

    }, {once: true});

    // Pass caret StateChanged events to ActionBarHandler.
    window.addEventListener("mozcaretstatechanged", e => {
      ActionBarHandler.caretStateChangedHandler(e);
    }, /* useCapture = */ true, /* wantsUntrusted = */ false);
  },

  get _startupStatus() {
    delete this._startupStatus;

    let savedMilestone = Services.prefs.getCharPref("browser.startup.homepage_override.mstone", "");
    let ourMilestone = AppConstants.MOZ_APP_VERSION;
    this._startupStatus = "";
    if (ourMilestone != savedMilestone) {
      Services.prefs.setCharPref("browser.startup.homepage_override.mstone", ourMilestone);
      this._startupStatus = savedMilestone ? "upgrade" : "new";
    }

    return this._startupStatus;
  },

  /**
   * Pass this a locale string, such as "fr" or "es_ES".
   */
  setLocale: function (locale) {
    console.log("browser.js: requesting locale set: " + locale);
    WindowEventDispatcher.sendRequest({ type: "Locale:Set", locale: locale });
  },

  initContextMenu: function () {
    // We pass a thunk in place of a raw label string. This allows the
    // context menu to automatically accommodate locale changes without
    // having to be rebuilt.
    let stringGetter = name => () => Strings.browser.GetStringFromName(name);

    // TODO: These should eventually move into more appropriate classes
    NativeWindow.contextmenus.add(stringGetter("contextmenu.openInNewTab"),
      NativeWindow.contextmenus.linkOpenableNonPrivateContext,
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_open_new_tab");
        UITelemetry.addEvent("loadurl.1", "contextmenu", null);

        let url = NativeWindow.contextmenus._getLinkURL(aTarget);
        ContentAreaUtils.urlSecurityCheck(url, aTarget.ownerDocument.nodePrincipal);
        let tab = BrowserApp.addTab(url, { selected: false, parentId: BrowserApp.selectedTab.id });

        let newtabStrings = Strings.browser.GetStringFromName("newtabpopup.opened");
        let label = PluralForm.get(1, newtabStrings).replace("#1", 1);
        let buttonLabel = Strings.browser.GetStringFromName("newtabpopup.switch");

        Snackbars.show(label, Snackbars.LENGTH_LONG, {
          action: {
            label: buttonLabel,
            callback: () => { BrowserApp.selectTab(tab); },
          }
        });
      });

    let showOpenInPrivateTab = true;
    if ("@mozilla.org/parental-controls-service;1" in Cc) {
      let pc = Cc["@mozilla.org/parental-controls-service;1"].createInstance(Ci.nsIParentalControlsService);
      showOpenInPrivateTab = pc.isAllowed(Ci.nsIParentalControlsService.PRIVATE_BROWSING);
    }

    if (showOpenInPrivateTab) {
      NativeWindow.contextmenus.add(stringGetter("contextmenu.openInPrivateTab"),
        NativeWindow.contextmenus.linkOpenableContext,
        function (aTarget) {
          UITelemetry.addEvent("action.1", "contextmenu", null, "web_open_new_tab");
          UITelemetry.addEvent("loadurl.1", "contextmenu", null);

          let url = NativeWindow.contextmenus._getLinkURL(aTarget);
          ContentAreaUtils.urlSecurityCheck(url, aTarget.ownerDocument.nodePrincipal);
          let tab = BrowserApp.addTab(url, {selected: false, parentId: BrowserApp.selectedTab.id, isPrivate: true});

          let newtabStrings = Strings.browser.GetStringFromName("newprivatetabpopup.opened");
          let label = PluralForm.get(1, newtabStrings).replace("#1", 1);
          let buttonLabel = Strings.browser.GetStringFromName("newtabpopup.switch");
          Snackbars.show(label, Snackbars.LENGTH_LONG, {
            action: {
              label: buttonLabel,
              callback: () => { BrowserApp.selectTab(tab); },
            }
          });
        });
    }

    NativeWindow.contextmenus.add(stringGetter("contextmenu.copyLink"),
      NativeWindow.contextmenus.linkCopyableContext,
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_copy_link");

        let url = NativeWindow.contextmenus._getLinkURL(aTarget);
        NativeWindow.contextmenus._copyStringToDefaultClipboard(url);
      });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.copyEmailAddress"),
      NativeWindow.contextmenus.emailLinkContext,
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_copy_email");

        let url = NativeWindow.contextmenus._getLinkURL(aTarget);
        let emailAddr = NativeWindow.contextmenus._stripScheme(url);
        NativeWindow.contextmenus._copyStringToDefaultClipboard(emailAddr);
      });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.copyPhoneNumber"),
      NativeWindow.contextmenus.phoneNumberLinkContext,
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_copy_phone");

        let url = NativeWindow.contextmenus._getLinkURL(aTarget);
        let phoneNumber = NativeWindow.contextmenus._stripScheme(url);
        NativeWindow.contextmenus._copyStringToDefaultClipboard(phoneNumber);
      });

    NativeWindow.contextmenus.add({
      label: stringGetter("contextmenu.shareLink"),
      order: NativeWindow.contextmenus.DEFAULT_HTML5_ORDER - 1, // Show above HTML5 menu items
      selector: NativeWindow.contextmenus._disableRestricted("SHARE", NativeWindow.contextmenus.linkShareableContext),
      showAsActions: function(aElement) {
        return {
          title: aElement.textContent.trim() || aElement.title.trim(),
          uri: NativeWindow.contextmenus._getLinkURL(aElement),
        };
      },
      icon: "drawable://ic_menu_share",
      callback: function(aTarget) {
        // share.1 telemetry is handled in Java via PromptList
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_share_link");
      }
    });

    NativeWindow.contextmenus.add({
      label: stringGetter("contextmenu.shareEmailAddress"),
      order: NativeWindow.contextmenus.DEFAULT_HTML5_ORDER - 1,
      selector: NativeWindow.contextmenus._disableRestricted("SHARE", NativeWindow.contextmenus.emailLinkContext),
      showAsActions: function(aElement) {
        let url = NativeWindow.contextmenus._getLinkURL(aElement);
        let emailAddr = NativeWindow.contextmenus._stripScheme(url);
        let title = aElement.textContent || aElement.title;
        return {
          title: title,
          uri: emailAddr,
        };
      },
      icon: "drawable://ic_menu_share",
      callback: function(aTarget) {
        // share.1 telemetry is handled in Java via PromptList
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_share_email");
      }
    });

    NativeWindow.contextmenus.add({
      label: stringGetter("contextmenu.sharePhoneNumber"),
      order: NativeWindow.contextmenus.DEFAULT_HTML5_ORDER - 1,
      selector: NativeWindow.contextmenus._disableRestricted("SHARE", NativeWindow.contextmenus.phoneNumberLinkContext),
      showAsActions: function(aElement) {
        let url = NativeWindow.contextmenus._getLinkURL(aElement);
        let phoneNumber = NativeWindow.contextmenus._stripScheme(url);
        let title = aElement.textContent || aElement.title;
        return {
          title: title,
          uri: phoneNumber,
        };
      },
      icon: "drawable://ic_menu_share",
      callback: function(aTarget) {
        // share.1 telemetry is handled in Java via PromptList
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_share_phone");
      }
    });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.addToContacts"),
      NativeWindow.contextmenus._disableRestricted("ADD_CONTACT", NativeWindow.contextmenus.emailLinkContext),
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_contact_email");

        let url = NativeWindow.contextmenus._getLinkURL(aTarget);
        WindowEventDispatcher.sendRequest({
          type: "Contact:Add",
          email: url
        });
      });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.addToContacts"),
      NativeWindow.contextmenus._disableRestricted("ADD_CONTACT", NativeWindow.contextmenus.phoneNumberLinkContext),
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_contact_phone");

        let url = NativeWindow.contextmenus._getLinkURL(aTarget);
        WindowEventDispatcher.sendRequest({
          type: "Contact:Add",
          phone: url
        });
      });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.bookmarkLink"),
      NativeWindow.contextmenus._disableRestricted("BOOKMARK", NativeWindow.contextmenus.linkBookmarkableContext),
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_bookmark");
        UITelemetry.addEvent("save.1", "contextmenu", null, "bookmark");

        let url = NativeWindow.contextmenus._getLinkURL(aTarget);
        let title = aTarget.textContent || aTarget.title || url;
        WindowEventDispatcher.sendRequest({
          type: "Bookmark:Insert",
          url: url,
          title: title
        });
      });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.playMedia"),
      NativeWindow.contextmenus.mediaContext("media-paused"),
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_play");
        aTarget.play();
      });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.pauseMedia"),
      NativeWindow.contextmenus.mediaContext("media-playing"),
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_pause");
        aTarget.pause();
      });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.showControls2"),
      NativeWindow.contextmenus.mediaContext("media-hidingcontrols"),
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_controls_media");
        aTarget.setAttribute("controls", true);
      });

    NativeWindow.contextmenus.add({
      label: stringGetter("contextmenu.shareMedia"),
      order: NativeWindow.contextmenus.DEFAULT_HTML5_ORDER - 1,
      selector: NativeWindow.contextmenus._disableRestricted(
        "SHARE", NativeWindow.contextmenus.videoContext()),
      showAsActions: function(aElement) {
        let url = (aElement.currentSrc || aElement.src);
        let title = aElement.textContent || aElement.title;
        return {
          title: title,
          uri: url,
          type: "video/*",
        };
      },
      icon: "drawable://ic_menu_share",
      callback: function(aTarget) {
        // share.1 telemetry is handled in Java via PromptList
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_share_media");
      }
    });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.fullScreen"),
      NativeWindow.contextmenus.videoContext("not-fullscreen"),
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_fullscreen");
        aTarget.requestFullscreen();
      });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.mute"),
      NativeWindow.contextmenus.mediaContext("media-unmuted"),
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_mute");
        aTarget.muted = true;
      });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.unmute"),
      NativeWindow.contextmenus.mediaContext("media-muted"),
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_unmute");
        aTarget.muted = false;
      });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.viewImage"),
      NativeWindow.contextmenus.imageLocationCopyableContext,
      function(aTarget) {
        let url = aTarget.src;
        ContentAreaUtils.urlSecurityCheck(url, aTarget.ownerDocument.nodePrincipal,
                                          Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT);

        UITelemetry.addEvent("action.1", "contextmenu", null, "web_view_image");
        UITelemetry.addEvent("loadurl.1", "contextmenu", null);
        BrowserApp.selectedBrowser.loadURI(url);
      });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.copyImageLocation"),
      NativeWindow.contextmenus.imageLocationCopyableContext,
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_copy_image");

        let url = aTarget.src;
        NativeWindow.contextmenus._copyStringToDefaultClipboard(url);
      });

    NativeWindow.contextmenus.add({
      label: stringGetter("contextmenu.shareImage"),
      selector: NativeWindow.contextmenus._disableRestricted("SHARE", NativeWindow.contextmenus.imageShareableContext),
      order: NativeWindow.contextmenus.DEFAULT_HTML5_ORDER - 1, // Show above HTML5 menu items
      showAsActions: function(aTarget) {
        let doc = aTarget.ownerDocument;
        let imageCache = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools)
                                                         .getImgCacheForDocument(doc);
        let props = imageCache.findEntryProperties(aTarget.currentURI, doc);
        let src = aTarget.src;
        return {
          title: src,
          uri: src,
          type: "image/*",
        };
      },
      icon: "drawable://ic_menu_share",
      menu: true,
      callback: function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_share_image");
      }
    });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.saveImage"),
      NativeWindow.contextmenus.imageSaveableContext,
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_save_image");
        UITelemetry.addEvent("save.1", "contextmenu", null, "image");

        RuntimePermissions.waitForPermissions(RuntimePermissions.WRITE_EXTERNAL_STORAGE).then(function(permissionGranted) {
            if (!permissionGranted) {
                return;
            }

            ContentAreaUtils.saveImageURL(aTarget.currentURI.spec, null, "SaveImageTitle",
                                          false, true, aTarget.ownerDocument.documentURIObject,
                                          aTarget.ownerDocument);
        });
      });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.setImageAs"),
      NativeWindow.contextmenus._disableRestricted("SET_IMAGE", NativeWindow.contextmenus.imageSaveableContext),
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_background_image");

        let src = aTarget.src;
        WindowEventDispatcher.sendRequest({
          type: "Image:SetAs",
          url: src
        });
      });

    NativeWindow.contextmenus.add(
      function(aTarget) {
        if (aTarget instanceof HTMLVideoElement) {
          if (aTarget.readyState == aTarget.HAVE_NOTHING) {
            // We don't know if the height/width of the video,
            // show a generic string.
            return Strings.browser.GetStringFromName("contextmenu.saveMedia");
          } else if (aTarget.videoWidth == 0 || aTarget.videoHeight == 0) {
            // If a video element is zero width or height, its essentially
            // an HTMLAudioElement.
            return Strings.browser.GetStringFromName("contextmenu.saveAudio");
          }
          return Strings.browser.GetStringFromName("contextmenu.saveVideo");
        } else if (aTarget instanceof HTMLAudioElement) {
          return Strings.browser.GetStringFromName("contextmenu.saveAudio");
        }
        return Strings.browser.GetStringFromName("contextmenu.saveVideo");
      }, NativeWindow.contextmenus.mediaSaveableContext,
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_save_media");
        UITelemetry.addEvent("save.1", "contextmenu", null, "media");

        let url = aTarget.currentSrc || aTarget.src;

        let filePickerTitleKey;
        if (aTarget instanceof HTMLVideoElement) {
          if (aTarget.readyState == aTarget.HAVE_NOTHING) {
            filePickerTitleKey = "SaveMediaTitle";
          } else if (aTarget.videoWidth == 0 || aTarget.videoHeight == 0) {
            filePickerTitleKey = "SaveAudioTitle";
          }
          filePickerTitleKey = "SaveVideoTitle";
        } else {
          filePickerTitleKey = "SaveAudioTitle";
        }

        // Skipped trying to pull MIME type out of cache for now
        ContentAreaUtils.internalSave(url, null, null, null, null, false,
                                      filePickerTitleKey, null, aTarget.ownerDocument.documentURIObject,
                                      aTarget.ownerDocument, true, null);
      });

    NativeWindow.contextmenus.add(stringGetter("contextmenu.showImage"),
      NativeWindow.contextmenus.imageBlockingPolicyContext,
      function(aTarget) {
        UITelemetry.addEvent("action.1", "contextmenu", null, "web_show_image");
        aTarget.setAttribute("data-ctv-show", "true");
        aTarget.setAttribute("src", aTarget.getAttribute("data-ctv-src"));

        // Shows a snackbar to unblock all images if browser.image_blocking.enabled is enabled.
        let blockedImgs = aTarget.ownerDocument.querySelectorAll("[data-ctv-src]");
        if (blockedImgs.length == 0) {
          return;
        }
        let message = Strings.browser.GetStringFromName("imageblocking.downloadedImage");
        Snackbars.show(message, Snackbars.LENGTH_LONG, {
          action: {
            label: Strings.browser.GetStringFromName("imageblocking.showAllImages"),
            callback: () => {
              UITelemetry.addEvent("action.1", "toast", null, "web_show_all_image");
              for (let i = 0; i < blockedImgs.length; ++i) {
                blockedImgs[i].setAttribute("data-ctv-show", "true");
                blockedImgs[i].setAttribute("src", blockedImgs[i].getAttribute("data-ctv-src"));
              }
            },
          }
        });
    });
  },

  onAppUpdated: function() {
    // initialize the form history and passwords databases on upgrades
    GlobalEventDispatcher.dispatch("FormHistory:Init", null);
    GlobalEventDispatcher.dispatch("Passwords:Init", null);

    if (this._startupStatus === "upgrade") {
      this._migrateUI();
    }
  },

  _migrateUI: function() {
    const UI_VERSION = 3;
    let currentUIVersion = Services.prefs.getIntPref("browser.migration.version", 0);
    if (currentUIVersion >= UI_VERSION) {
      return;
    }

    if (currentUIVersion < 1) {
      // Migrate user-set "plugins.click_to_play" pref. See bug 884694.
      // Because the default value is true, a user-set pref means that the pref was set to false.
      if (Services.prefs.prefHasUserValue("plugins.click_to_play")) {
        Services.prefs.setIntPref("plugin.default.state", Ci.nsIPluginTag.STATE_ENABLED);
        Services.prefs.clearUserPref("plugins.click_to_play");
      }

      // Migrate the "privacy.donottrackheader.value" pref. See bug 1042135.
      if (Services.prefs.prefHasUserValue("privacy.donottrackheader.value")) {
        // Make sure the doNotTrack value conforms to the conversion from
        // three-state to two-state. (This reverts a setting of "please track me"
        // to the default "don't say anything").
        if (Services.prefs.getBoolPref("privacy.donottrackheader.enabled") &&
            (Services.prefs.getIntPref("privacy.donottrackheader.value") != 1)) {
          Services.prefs.clearUserPref("privacy.donottrackheader.enabled");
        }

        // This pref has been removed, so always clear it.
        Services.prefs.clearUserPref("privacy.donottrackheader.value");
      }

      // Set the search activity default pref on app upgrade if it has not been set already.
      if (!Services.prefs.prefHasUserValue("searchActivity.default.migrated")) {
        Services.prefs.setBoolPref("searchActivity.default.migrated", true);
        SearchEngines.migrateSearchActivityDefaultPref();
      }

      Reader.migrateCache().catch(e => Cu.reportError("Error migrating Reader cache: " + e));

      // We removed this pref from user visible settings, so we should reset it.
      // Power users can go into about:config to re-enable this if they choose.
      if (Services.prefs.prefHasUserValue("nglayout.debug.paint_flashing")) {
        Services.prefs.clearUserPref("nglayout.debug.paint_flashing");
      }
    }

    if (currentUIVersion < 2) {
      let name;
      if (Services.prefs.prefHasUserValue("browser.search.defaultenginename")) {
        name = Services.prefs.getCharPref("browser.search.defaultenginename");
      }
      if (!name && Services.prefs.prefHasUserValue("browser.search.defaultenginename.US")) {
        name = Services.prefs.getCharPref("browser.search.defaultenginename.US");
      }
      if (name) {
        Services.search.init(() => {
          let engine = Services.search.getEngineByName(name);
          if (engine) {
            Services.search.defaultEngine = engine;
            Services.obs.notifyObservers(null, "default-search-engine-migrated", "");
          }
        });
      }
    }

    if (currentUIVersion < 3) {
      const kOldSafeBrowsingPref = "browser.safebrowsing.enabled";
      // Default value is set to true, a user pref means that the pref was
      // set to false.
      if (Services.prefs.prefHasUserValue(kOldSafeBrowsingPref) &&
          !Services.prefs.getBoolPref(kOldSafeBrowsingPref)) {
        Services.prefs.setBoolPref("browser.safebrowsing.phishing.enabled",
                                   false);
        // Should just remove support for the pref entirely, even if it's
        // only in about:config
        Services.prefs.clearUserPref(kOldSafeBrowsingPref);
      }
    }

    // Update the migration version.
    Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
  },

  // This function returns false during periods where the browser displayed document is
  // different from the browser content document, so user actions and some kinds of viewport
  // updates should be ignored. This period starts when we start loading a new page or
  // switch tabs, and ends when the new browser content document has been drawn and handed
  // off to the compositor.
  isBrowserContentDocumentDisplayed: function() {
    try {
      if (!Services.androidBridge.isContentDocumentDisplayed(window))
        return false;
    } catch (e) {
      return false;
    }

    let tab = this.selectedTab;
    if (!tab)
      return false;
    return tab.contentDocumentIsDisplayed;
  },

  contentDocumentChanged: function() {
    window.top.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).isFirstPaint = true;
    Services.androidBridge.contentDocumentChanged(window);
  },

  get tabs() {
    return this._tabs;
  },

  set selectedTab(aTab) {
    if (this._selectedTab == aTab)
      return;

    if (this._selectedTab) {
      this._selectedTab.setActive(false);
    }

    this._selectedTab = aTab;
    if (!aTab)
      return;

    aTab.setActive(true);
    this.contentDocumentChanged();
    this.deck.selectedPanel = aTab.browser;
    // Focus the browser so that things like selection will be styled correctly.
    aTab.browser.focus();
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

  // Use this instead of deck.selectedIndex (which is invalid for this purpose).
  get selectedTabIndex() {
    return this._tabs.indexOf(this._selectedTab);
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

    let tab = this.getTabForBrowser(aBrowser);
    if (tab) {
      if ("userRequested" in aParams) tab.userRequested = aParams.userRequested;
      tab.isSearch = ("isSearch" in aParams) ? aParams.isSearch : false;
    }

    try {
      aBrowser.loadURIWithFlags(aURI, flags, referrerURI, charset, postData);
    } catch(e) {
      if (tab) {
        let message = {
          type: "Content:LoadError",
          tabID: tab.id
        };
        GlobalEventDispatcher.sendRequest(message);
        dump("Handled load error: " + e)
      }
    }
  },

  addTab: function addTab(aURI, aParams) {
    aParams = aParams || {};

    let fullscreenState;
    if (this.selectedBrowser) {
       fullscreenState = this.selectedBrowser.contentDocument.fullscreenElement;
       if (fullscreenState) {
         aParams.selected = false;
       }
    }

    let newTab = new Tab(aURI, aParams);

    if (fullscreenState) {
       this.fullscreenTransitionTab = newTab;
       this.selectedBrowser.contentDocument.exitFullscreen();
    }

    if (typeof aParams.tabIndex == "number") {
      this._tabs.splice(aParams.tabIndex, 0, newTab);
    } else {
      this._tabs.push(newTab);
    }

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
      type: "Tab:Close",
      tabID: aTab.id
    };
    GlobalEventDispatcher.sendRequest(message);
  },

  // Calling this will update the state in BrowserApp after a tab has been
  // closed in the Java UI.
  _handleTabClosed: function _handleTabClosed(aTab, aShowUndoSnackbar) {
    if (aTab == this.selectedTab)
      this.selectedTab = null;

    let tabIndex = this._tabs.indexOf(aTab);

    let evt = document.createEvent("UIEvents");
    evt.initUIEvent("TabClose", true, false, window, tabIndex);
    aTab.browser.dispatchEvent(evt);

    let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
    if (aShowUndoSnackbar && ss.canUndoLastCloseTab) {
      // Get a title for the undo close snackbar. Fall back to the URL if there is no title.
      let closedTabData = ss.getClosedTabs(window)[0];

      if (closedTabData) {
        let message;
        let title = closedTabData.entries[closedTabData.index - 1].title;
        let isPrivate = PrivateBrowsingUtils.isBrowserPrivate(aTab.browser);

        if (isPrivate) {
          message = Strings.browser.GetStringFromName("privateClosedMessage.message");
        } else if (title) {
          message = Strings.browser.formatStringFromName("undoCloseToast.message", [title], 1);
        } else {
          message = Strings.browser.GetStringFromName("undoCloseToast.messageDefault");
        }

        Snackbars.show(message, Snackbars.LENGTH_LONG, {
          action: {
            label: Strings.browser.GetStringFromName("undoCloseToast.action2"),
            callback: function() {
              UITelemetry.addEvent("undo.1", "toast", null, "closetab");
              ss.undoCloseTab(window, closedTabData);
            }
          }
        });
      }
    }

    aTab.destroy();
    this._tabs.splice(tabIndex, 1);
  },

  _handleTabMove(fromTabId, fromPosition, toTabId, toPosition) {
    let movedTab = this._tabs[fromPosition];
    if (movedTab.id != fromTabId || this._tabs[toPosition].id != toTabId) {
      // The gecko and/or java Tabs tabs lists changed sometime between when the Tabs list was
      // updated and when news of the update arrived here.
      throw "Moved tab mismatch: (" + fromTabId + ", " + movedTab.id + "), " +
            "(" + toTabId + ", " + this._tabs[toPosition].id + ")";
    }

    let step = (fromPosition < toPosition) ? 1 : -1;
    for (let i = fromPosition; i != toPosition; i += step) {
      this._tabs[i] = this._tabs[i + step];
    }
    this._tabs[toPosition] = movedTab;

    let evt = new UIEvent("TabMove", {"bubbles":true, "cancellable":false, "view":window, "detail":fromPosition});
    this.tabs[toPosition].browser.dispatchEvent(evt);
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

    let doc = this.selectedBrowser.contentDocument;
    if (doc.fullscreenElement) {
      // We'll finish the tab selection once the fullscreen transition has ended,
      // remember the new tab for this.
      this.fullscreenTransitionTab = aTab;
      doc.exitFullscreen();
      return;
    }

    let message = {
      type: "Tab:Select",
      tabID: aTab.id
    };
    GlobalEventDispatcher.sendRequest(message);
  },

  /**
   * Gets an open tab with the given URL.
   *
   * @param  aURL URL to look for
   * @param  aOptions Options for the search. Currently supports:
   **  @option startsWith a Boolean indicating whether to search for a tab who's url starts with the
   *           requested url. Useful if you want to ignore hash codes on the end of a url. For instance
   *           to have about:downloads match about:downloads#123.
   * @return the tab with the given URL, or null if no such tab exists
   */
  getTabWithURL: function getTabWithURL(aURL, aOptions) {
    aOptions = aOptions || {};
    let uri = Services.io.newURI(aURL);
    for (let i = 0; i < this._tabs.length; ++i) {
      let tab = this._tabs[i];
      if (aOptions.startsWith) {
        if (tab.currentURI.spec.startsWith(aURL)) {
          return tab;
        }
      } else {
        if (tab.currentURI.equals(uri)) {
          return tab;
        }
      }
    }
    return null;
  },

  /**
   * If a tab with the given URL already exists, that tab is selected.
   * Otherwise, a new tab is opened with the given URL.
   *
   * @param aURL URL to open
   * @param aParam Options used if a tab is created
   * @param aFlags Options for the search. Currently supports:
   **  @option startsWith a Boolean indicating whether to search for a tab who's url starts with the
   *           requested url. Useful if you want to ignore hash codes on the end of a url. For instance
   *           to have about:downloads match about:downloads#123.
   */
  selectOrAddTab: function selectOrAddTab(aURL, aParams, aFlags) {
    let tab = this.getTabWithURL(aURL, aFlags);
    if (tab == null) {
      tab = this.addTab(aURL, aParams);
    } else {
      this.selectTab(tab);
    }

    return tab;
  },

  // This method updates the state in BrowserApp after a tab has been selected
  // in the Java UI.
  _handleTabSelected: function _handleTabSelected(aTab) {
    if (this.fullscreenTransitionTab) {
      // Defer updating to "fullscreenchange" if tab selection happened during
      // a fullscreen transition.
      return;
    }
    this.selectedTab = aTab;

    let evt = document.createEvent("UIEvents");
    evt.initUIEvent("TabSelect", true, false, window, null);
    aTab.browser.dispatchEvent(evt);
  },

  quit: function quit(aClear = { sanitize: {}, dontSaveSession: false }) {
    // Notify all windows that an application quit has been requested.
    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
    Services.obs.notifyObservers(cancelQuit, "quit-application-requested", null);

    // Quit aborted.
    if (cancelQuit.data) {
      return;
    }

    Services.obs.notifyObservers(null, "quit-application-proceeding", null);

    // Tell session store to forget about this window
    if (aClear.dontSaveSession) {
      let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
      ss.removeWindow(window);
    }

    BrowserApp.sanitize(aClear.sanitize, function() {
      let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
      appStartup.quit(Ci.nsIAppStartup.eForceQuit);
    }, true);
  },

  saveAsPDF: function saveAsPDF(aBrowser) {
    RuntimePermissions.waitForPermissions(RuntimePermissions.WRITE_EXTERNAL_STORAGE).then(function(permissionGranted) {
      if (!permissionGranted) {
        return;
      }

      Task.spawn(function* () {
        let fileName = ContentAreaUtils.getDefaultFileName(aBrowser.contentTitle, aBrowser.currentURI, null, null);
        fileName = fileName.trim() + ".pdf";

        let downloadsDir = yield Downloads.getPreferredDownloadsDirectory();
        let file = OS.Path.join(downloadsDir, fileName);

        // Force this to have a unique name.
        let openedFile = yield OS.File.openUnique(file, { humanReadable: true });
        file = openedFile.path;
        yield openedFile.file.close();

        let download = yield Downloads.createDownload({
          source: aBrowser.contentWindow,
          target: file,
          saver: "pdf",
          startTime: Date.now(),
        });

        let list = yield Downloads.getList(download.source.isPrivate ? Downloads.PRIVATE : Downloads.PUBLIC)
        yield list.add(download);
        yield download.start();
      });
    });
  },

  // These values come from pref_tracking_protection_entries in arrays.xml.
  PREF_TRACKING_PROTECTION_ENABLED: "2",
  PREF_TRACKING_PROTECTION_ENABLED_PB: "1",
  PREF_TRACKING_PROTECTION_DISABLED: "0",

  /**
   * Returns the current state of the tracking protection pref.
   * (0 = Disabled, 1 = Enabled in PB, 2 = Enabled)
   */
  getTrackingProtectionState: function() {
    if (Services.prefs.getBoolPref("privacy.trackingprotection.enabled")) {
      return this.PREF_TRACKING_PROTECTION_ENABLED;
    }
    if (Services.prefs.getBoolPref("privacy.trackingprotection.pbmode.enabled")) {
      return this.PREF_TRACKING_PROTECTION_ENABLED_PB;
    }
    return this.PREF_TRACKING_PROTECTION_DISABLED;
  },

  sanitize: function (aItems, callback, aShutdown) {
    let success = true;
    var promises = [];
    let refObj = {};

    TelemetryStopwatch.start("FX_SANITIZE_TOTAL", refObj);

    for (let key in aItems) {
      if (!aItems[key])
        continue;

      key = key.replace("private.data.", "");

      switch (key) {
        case "cookies_sessions":
          promises.push(Sanitizer.clearItem("cookies"));
          promises.push(Sanitizer.clearItem("sessions"));
          break;
        case "openTabs":
          if (aShutdown === true) {
            Services.obs.notifyObservers(null, "browser:purge-session-tabs", "");
            break;
          }
          // fall-through if aShutdown is false
        default:
          promises.push(Sanitizer.clearItem(key));
      }
    }

    Promise.all(promises).then(function() {
      TelemetryStopwatch.finish("FX_SANITIZE_TOTAL", refObj);
      GlobalEventDispatcher.sendRequest({
        type: "Sanitize:Finished",
        success: true,
        shutdown: aShutdown === true
      });

      if (callback) {
        callback();
      }
    }).catch(function(err) {
      TelemetryStopwatch.finish("FX_SANITIZE_TOTAL", refObj);
      GlobalEventDispatcher.sendRequest({
        type: "Sanitize:Finished",
        error: err,
        success: false,
        shutdown: aShutdown === true
      });

      if (callback) {
        callback();
      }
    })
  },

  getFocusedInput: function(aBrowser, aOnlyInputElements = false) {
    if (!aBrowser)
      return null;

    let doc = aBrowser.contentDocument;
    if (!doc)
      return null;

    let focused = doc.activeElement;
    while (focused instanceof HTMLFrameElement || focused instanceof HTMLIFrameElement) {
      doc = focused.contentDocument;
      focused = doc.activeElement;
    }

    if (focused instanceof HTMLInputElement &&
        (focused.mozIsTextField(false) || focused.type === "number")) {
      return focused;
    }

    if (aOnlyInputElements)
      return null;

    if (focused && (focused instanceof HTMLTextAreaElement || focused.isContentEditable)) {

      if (focused instanceof HTMLBodyElement) {
        // we are putting focus into a contentEditable frame. scroll the frame into
        // view instead of the contentEditable document contained within, because that
        // results in a better user experience
        focused = focused.ownerGlobal.frameElement;
      }
      return focused;
    }
    return null;
  },

  scrollToFocusedInput: function(aBrowser, aAllowZoom = true) {
    let formHelperMode = Services.prefs.getIntPref("formhelper.mode");
    if (formHelperMode == kFormHelperModeDisabled)
      return;

    let dwu = aBrowser.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    if (!dwu) {
      return;
    }

    let apzFlushDone = function() {
      Services.obs.removeObserver(apzFlushDone, "apz-repaints-flushed");
      dwu.zoomToFocusedInput();
    };

    let paintDone = function() {
      window.removeEventListener("MozAfterPaint", paintDone);
      if (dwu.flushApzRepaints()) {
        Services.obs.addObserver(apzFlushDone, "apz-repaints-flushed");
      } else {
        apzFlushDone();
      }
    };

    let gotResizeWindow = false;
    let resizeWindow = function(e) {
      gotResizeWindow = true;
      aBrowser.contentWindow.removeEventListener("resize", resizeWindow);
      if (dwu.isMozAfterPaintPending) {
        window.addEventListener("MozAfterPaint", paintDone);
      } else {
        paintDone();
      }
    }

    aBrowser.contentWindow.addEventListener("resize", resizeWindow);

    // The "resize" event sometimes fails to fire, so set a timer to catch that case
    // and unregister the event listener. See Bug 1253469
    setTimeout(function(e) {
    if (!gotResizeWindow) {
        aBrowser.contentWindow.removeEventListener("resize", resizeWindow);
        dwu.zoomToFocusedInput();
      }
    }, 500);
  },

  getUALocalePref: function () {
    try {
      return Services.prefs.getComplexValue("general.useragent.locale", Ci.nsIPrefLocalizedString).data;
    } catch (e) {
      try {
        return Services.prefs.getCharPref("general.useragent.locale");
      } catch (ee) {
        return undefined;
      }
    }
  },

  getOSLocalePref: function () {
    try {
      return Services.prefs.getCharPref("intl.locale.os");
    } catch (e) {
      return undefined;
    }
  },

  setLocalizedPref: function (pref, value) {
    let pls = Cc["@mozilla.org/pref-localizedstring;1"]
                .createInstance(Ci.nsIPrefLocalizedString);
    pls.data = value;
    Services.prefs.setComplexValue(pref, Ci.nsIPrefLocalizedString, pls);
  },

  onEvent: function (event, data, callback) {
    let browser = this.selectedBrowser;

    switch (event) {
      case "Browser:LoadManifest": {
        installManifest(browser, data);
        break;
      }

      case "Browser:Quit":
        this.quit(data);
        break;

      case "Fonts:Reload":
        FontEnumerator.updateFontList();
        break;

      case "FormHistory:Init": {
        // Force creation/upgrade of formhistory.sqlite
        FormHistory.count({});
        GlobalEventDispatcher.unregisterListener(this, event);
        break;
      }

      case "FullScreen:Exit":
        browser.contentDocument.exitFullscreen();
        break;

      case "Locale:OS": {
        // We know the system locale. We use this for generating Accept-Language headers.
        let languageTag = data.languageTag;
        console.log("Locale:OS: " + languageTag);
        let currentOSLocale = this.getOSLocalePref();
        if (currentOSLocale == languageTag) {
          break;
        }

        console.log("New OS locale.");

        // Ensure that this choice is immediately persisted, because
        // Gecko won't be told again if it forgets.
        Services.prefs.setCharPref("intl.locale.os", languageTag);
        Services.prefs.savePrefFile(null);

        let appLocale = this.getUALocalePref();

        this.computeAcceptLanguages(languageTag, appLocale);
        break;
      }

      case "Locale:Changed": {
        if (data) {
          Services.locale.setRequestedLocales([data.languageTag]);
        } else {
          Services.locale.setRequestedLocales([]);
        }

        // Make sure we use the right Accept-Language header.
        let osLocale;
        try {
          // This should never not be set at this point, but better safe than sorry.
          osLocale = Services.prefs.getCharPref("intl.locale.os");
        } catch (e) {
        }

        this.computeAcceptLanguages(osLocale, data && data.languageTag);
        break;
      }

      case "Passwords:Init": {
        let storage = Cc["@mozilla.org/login-manager/storage/mozStorage;1"].
                      getService(Ci.nsILoginManagerStorage);
        storage.initialize();
        GlobalEventDispatcher.unregisterListener(this, event);
        break;
      }

      case "Sanitize:ClearData":
        this.sanitize(data);
        break;

      case "SaveAs:PDF":
        this.saveAsPDF(browser);
        break;

      case "ScrollTo:FocusedInput": {
        // these messages come from a change in the viewable area and not user interaction
        // we allow scrolling to the selected input, but not zooming the page
        this.scrollToFocusedInput(browser, false);
        break;
      }

      case "Telemetry:CustomTabsPing": {
        TelemetryController.submitExternalPing("anonymous", { client: data.client }, { addClientId: false });
        break;
      }

      case "Session:GetHistory": {
        callback.onSuccess(this.getHistory(data));
        break;
      }

      case "Session:Back":
        browser.goBack();
        break;

      case "Session:Forward":
        browser.goForward();
        break;

      case "Session:Navigate": {
        let index = data.index;
        let webNav = BrowserApp.selectedTab.window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebNavigation);
        let historySize = webNav.sessionHistory.count;

        if (index < 0) {
          index = 0;
          Log.e("Browser", "Negative index truncated to zero");
        } else if (index >= historySize) {
          Log.e("Browser", "Incorrect index " + index + " truncated to " + historySize - 1);
          index = historySize - 1;
        }

        browser.gotoIndex(index);
        break;
      }

      case "Session:Reload": {
        let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

        // Check to see if this is a message to enable/disable mixed content blocking.
        if (data) {
          if (data.bypassCache) {
            flags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE |
                     Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY;
          }

          if (data.contentType === "tracking") {
            // Convert document URI into the format used by
            // nsChannelClassifier::ShouldEnableTrackingProtection
            // (any scheme turned into https is correct)
            let normalizedUrl = Services.io.newURI("https://" + browser.currentURI.hostPort);
            if (data.allowContent) {
              // Add the current host in the 'trackingprotection' consumer of
              // the permission manager using a normalized URI. This effectively
              // places this host on the tracking protection white list.
              if (PrivateBrowsingUtils.isBrowserPrivate(browser)) {
                PrivateBrowsingUtils.addToTrackingAllowlist(normalizedUrl);
              } else {
                Services.perms.add(normalizedUrl, "trackingprotection", Services.perms.ALLOW_ACTION);
                Telemetry.addData("TRACKING_PROTECTION_EVENTS", 1);
              }
            } else {
              // Remove the current host from the 'trackingprotection' consumer
              // of the permission manager. This effectively removes this host
              // from the tracking protection white list (any list actually).
              if (PrivateBrowsingUtils.isBrowserPrivate(browser)) {
                PrivateBrowsingUtils.removeFromTrackingAllowlist(normalizedUrl);
              } else {
                Services.perms.remove(normalizedUrl, "trackingprotection");
                Telemetry.addData("TRACKING_PROTECTION_EVENTS", 2);
              }
            }
          }
        }

        // Try to use the session history to reload so that framesets are
        // handled properly. If the window has no session history, fall back
        // to using the web navigation's reload method.
        let webNav = browser.webNavigation;
        try {
          let sh = webNav.sessionHistory;
          if (sh)
            webNav = sh.QueryInterface(Ci.nsIWebNavigation);
        } catch (e) {}
        webNav.reload(flags);
        break;
      }

      case "Session:Stop":
        browser.stop();
        break;

      case "Tab:Load": {
        let url = data.url;
        let flags = Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP
                  | Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS;

        // Pass LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL to prevent any loads from
        // inheriting the currently loaded document's principal.
        if (data.userEntered) {
          flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
        }

        let delayLoad = ("delayLoad" in data) ? data.delayLoad : false;
        let params = {
          selected: ("selected" in data) ? data.selected : !delayLoad,
          parentId: ("parentId" in data) ? data.parentId : -1,
          flags: flags,
          tabID: data.tabID,
          isPrivate: (data.isPrivate === true),
          pinned: (data.pinned === true),
          delayLoad: (delayLoad === true),
          desktopMode: (data.desktopMode === true),
          tabType: ("tabType" in data) ? data.tabType : "BROWSING"
        };

        params.userRequested = url;

        if (data.engine) {
          let engine = Services.search.getEngineByName(data.engine);
          if (engine) {
            let submission = engine.getSubmission(url);
            url = submission.uri.spec;
            params.postData = submission.postData;
            params.isSearch = true;
          }
        }

        if (data.newTab) {
          this.addTab(url, params);
        } else {
          if (data.tabId) {
            // Use a specific browser instead of the selected browser, if it exists
            let specificBrowser = this.getTabForId(data.tabId).browser;
            if (specificBrowser)
              browser = specificBrowser;
          }
          this.loadURI(url, browser, params);
        }
        break;
      }

      case "Tab:Selected":
        this._handleTabSelected(this.getTabForId(data.id));
        break;

      case "Tab:Closed": {
        this._handleTabClosed(this.getTabForId(data.tabId), data.showUndoToast);
        break;
      }

      case "Tab:Move":
        this._handleTabMove(data.fromTabId, data.fromPosition, data.toTabId, data.toPosition);
        break;
    }
  },

  observe: function(aSubject, aTopic, aData) {
    let browser = this.selectedBrowser;

    switch (aTopic) {
      case "keyword-search":
        // This event refers to a search via the URL bar, not a bookmarks
        // keyword search. Note that this code assumes that the user can only
        // perform a keyword search on the selected tab.
        this.selectedTab.isSearch = true;

        // Don't store queries in private browsing mode.
        let isPrivate = PrivateBrowsingUtils.isBrowserPrivate(this.selectedTab.browser);
        let query = isPrivate ? "" : aData;

        let engine = aSubject.QueryInterface(Ci.nsISearchEngine);
        GlobalEventDispatcher.sendRequest({
          type: "Search:Keyword",
          identifier: engine.identifier,
          name: engine.name,
          query: query
        });
        break;

      case "android-get-pref": {
        // These pref names are not "real" pref names. They are used in the
        // setting menu, and these are passed when initializing the setting
        // menu. aSubject is a nsIWritableVariant to hold the pref value.
        aSubject.QueryInterface(Ci.nsIWritableVariant);

        switch (aData) {
          // The plugin pref is actually two separate prefs, so
          // we need to handle it differently
          case "plugin.enable":
            aSubject.setAsAString(PluginHelper.getPluginPreference());
            break;

          // Handle master password
          case "privacy.masterpassword.enabled":
            aSubject.setAsBool(MasterPassword.enabled);
            break;

          case "privacy.trackingprotection.state": {
            aSubject.setAsAString(this.getTrackingProtectionState());
            break;
          }

          // Crash reporter submit pref must be fetched from nsICrashReporter
          // service.
          case "datareporting.crashreporter.submitEnabled":
            let crashReporterBuilt = "nsICrashReporter" in Ci &&
                Services.appinfo instanceof Ci.nsICrashReporter;
            if (crashReporterBuilt) {
              aSubject.setAsBool(Services.appinfo.submitReports);
            }
            break;
        }
        break;
      }

      case "android-set-pref": {
        // Pseudo-prefs. aSubject is an nsIWritableVariant that holds the pref
        // value. Set to empty to signal the pref was handled.
        aSubject.QueryInterface(Ci.nsIWritableVariant);
        let value = aSubject.QueryInterface(Ci.nsIVariant);

        switch (aData) {
          // The plugin pref is actually two separate prefs, so we need to
          // handle it differently.
          case "plugin.enable":
            PluginHelper.setPluginPreference(value);
            aSubject.setAsEmpty();
            break;

          // MasterPassword pref is not real, we just need take action and leave
          case "privacy.masterpassword.enabled":
            if (MasterPassword.enabled) {
              MasterPassword.removePassword(value);
            } else {
              MasterPassword.setPassword(value);
            }
            aSubject.setAsEmpty();
            break;

          // "privacy.trackingprotection.state" is not a "real" pref name, but
          // it's used in the setting menu.  By default
          // "privacy.trackingprotection.pbmode.enabled" is true, and
          // "privacy.trackingprotection.enabled" is false.
          case "privacy.trackingprotection.state": {
            switch (value) {
              // Tracking protection disabled.
              case this.PREF_TRACKING_PROTECTION_DISABLED:
                Services.prefs.setBoolPref("privacy.trackingprotection.pbmode.enabled", false);
                Services.prefs.setBoolPref("privacy.trackingprotection.enabled", false);
                break;
              // Tracking protection only in private browsing,
              case this.PREF_TRACKING_PROTECTION_ENABLED_PB:
                Services.prefs.setBoolPref("privacy.trackingprotection.pbmode.enabled", true);
                Services.prefs.setBoolPref("privacy.trackingprotection.enabled", false);
                break;
              // Tracking protection everywhere.
              case this.PREF_TRACKING_PROTECTION_ENABLED:
                Services.prefs.setBoolPref("privacy.trackingprotection.pbmode.enabled", true);
                Services.prefs.setBoolPref("privacy.trackingprotection.enabled", true);
                break;
            }
            aSubject.setAsEmpty();
            break;
          }

          // Crash reporter preference is in a service; set and return.
          case "datareporting.crashreporter.submitEnabled":
            let crashReporterBuilt = "nsICrashReporter" in Ci &&
                Services.appinfo instanceof Ci.nsICrashReporter;
            if (crashReporterBuilt) {
              Services.appinfo.submitReports = value;
              aSubject.setAsEmpty();
            }
            break;
        }
        break;
      }

      case "gather-telemetry":
        GlobalEventDispatcher.sendRequest({ type: "Telemetry:Gather" });
        break;

      case "Vibration:Request":
        if (aSubject instanceof Navigator) {
          let navigator = aSubject;
          let buttons = [
            {
              label: Strings.browser.GetStringFromName("vibrationRequest.denyButton"),
              callback: function() {
                navigator.setVibrationPermission(false);
              }
            },
            {
              label: Strings.browser.GetStringFromName("vibrationRequest.allowButton"),
              callback: function() {
                navigator.setVibrationPermission(true);
              },
              positive: true
            }
          ];
          let message = Strings.browser.GetStringFromName("vibrationRequest.message");
          let options = {};
          NativeWindow.doorhanger.show(message, "vibration-request", buttons,
              BrowserApp.selectedTab.id, options, "VIBRATION");
        }
        break;

      default:
        dump('BrowserApp.observe: unexpected topic "' + aTopic + '"\n');
        break;

    }
  },

  /**
   * Set intl.accept_languages accordingly.
   *
   * After Bug 881510 this will also accept a real Accept-Language choice as
   * input; all Accept-Language logic lives here.
   *
   * osLocale should never be null, but this method is safe regardless.
   * appLocale may explicitly be null.
   */
  computeAcceptLanguages(osLocale, appLocale) {
    let defaultBranch = Services.prefs.getDefaultBranch(null);
    let defaultAccept = defaultBranch.getComplexValue("intl.accept_languages", Ci.nsIPrefLocalizedString).data;
    console.log("Default intl.accept_languages = " + defaultAccept);

    // A guard for potential breakage. Bug 438031.
    // This should not be necessary, because we're reading from the default branch,
    // but better safe than sorry.
    if (defaultAccept && defaultAccept.startsWith("chrome://")) {
      defaultAccept = null;
    } else {
      // Ensure lowercase everywhere so we can compare.
      defaultAccept = defaultAccept.toLowerCase();
    }

    if (appLocale) {
      appLocale = appLocale.toLowerCase();
    }

    if (osLocale) {
      osLocale = osLocale.toLowerCase();
    }

    // Eliminate values if they're present in the default.
    let chosen;
    if (defaultAccept) {
      // intl.accept_languages is a comma-separated list, with no q-value params. Those
      // are added when the header is generated.
      chosen = defaultAccept.split(",")
                            .map((x) => x.trim())
                            .filter((x) => (x != appLocale && x != osLocale));
    } else {
      chosen = [];
    }

    if (osLocale) {
      chosen.unshift(osLocale);
    }

    if (appLocale && appLocale != osLocale) {
      chosen.unshift(appLocale);
    }

    let result = chosen.join(",");
    console.log("Setting intl.accept_languages to " + result);
    this.setLocalizedPref("intl.accept_languages", result);
  },

  // nsIAndroidBrowserApp
  get selectedTab() {
    return this._selectedTab;
  },

  // nsIAndroidBrowserApp
  getBrowserTab: function(tabId) {
    return this.getTabForId(tabId);
  },

  getUITelemetryObserver: function() {
    return UITelemetry;
  },

  // This method will return a list of history items and toIndex based on the action provided from the fromIndex to toIndex,
  // optionally selecting selIndex (if fromIndex <= selIndex <= toIndex)
  getHistory: function(data) {
    let action = data.action;
    let webNav = BrowserApp.getTabForId(data.tabId).window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebNavigation);
    let historyIndex = webNav.sessionHistory.index;
    let historySize = webNav.sessionHistory.count;
    let canGoBack = webNav.canGoBack;
    let canGoForward = webNav.canGoForward;
    let listitems = [];
    let fromIndex = 0;
    let toIndex = historySize - 1;
    let selIndex = historyIndex;

    if (action == "BACK" && canGoBack) {
      fromIndex = Math.max(historyIndex - kMaxHistoryListSize, 0);
      toIndex = historyIndex;
      selIndex = historyIndex;
    } else if (action == "FORWARD" && canGoForward) {
      fromIndex = historyIndex;
      toIndex = Math.min(historySize - 1, historyIndex + kMaxHistoryListSize);
      selIndex = historyIndex;
    } else if (action == "ALL" && (canGoBack || canGoForward)){
      fromIndex = historyIndex - kMaxHistoryListSize / 2;
      toIndex = historyIndex + kMaxHistoryListSize / 2;
      if (fromIndex < 0) {
        toIndex -= fromIndex;
      }

      if (toIndex > historySize - 1) {
        fromIndex -= toIndex - (historySize - 1);
        toIndex = historySize - 1;
      }

      fromIndex = Math.max(fromIndex, 0);
      selIndex = historyIndex;
    } else {
      // return empty list immediately.
      return {
        "historyItems": listitems,
        "toIndex": toIndex
      };
    }

    let browser = this.selectedBrowser;
    let hist = browser.sessionHistory;
    for (let i = toIndex; i >= fromIndex; i--) {
      let entry = hist.getEntryAtIndex(i, false);
      let item = {
        title: entry.title || entry.URI.spec,
        url: entry.URI.spec,
        selected: (i == selIndex)
      };
      listitems.push(item);
    }

    return {
      "historyItems": listitems,
      "toIndex": toIndex
    };
  },
};

async function notifyManifestStatus(browser) {
  try {
    const manifest = await Manifests.getManifest(browser);
    const evtType = (manifest && manifest.installed) ?
      "Website:AppEntered" : "Website:AppLeft";
    GlobalEventDispatcher.sendRequest({type: evtType});
  } catch (err) {
    Cu.reportError("Error sending status: " + err.message);
  }
}

async function installManifest(browser, data) {
  try {
    const manifest = await Manifests.getManifest(browser, data.manifestUrl);
    await manifest.install();
    const icon = await manifest.icon(data.iconSize);
    GlobalEventDispatcher.sendRequest({
      type: "Website:AppInstalled",
      icon,
      name: manifest.name,
      start_url: manifest.start_url,
      manifest_path: manifest.path
    });
  } catch (err) {
    Cu.reportError("Failed to install: " + err.message);
    // If we fail to install via the manifest, we will fall back to a standard bookmark
    GlobalEventDispatcher.sendRequest({
      type: "Website:AppInstallFailed",
      url: data.originalUrl,
      title: data.originalTitle
    });
  }
}

var NativeWindow = {
  init: function() {
    GlobalEventDispatcher.registerListener(this, [
      "Doorhanger:Reply",
      "Menu:Clicked",
    ]);
    this.contextmenus.init();
  },

  loadDex: function(zipFile, implClass) {
    GlobalEventDispatcher.sendRequest({
      type: "Dex:Load",
      zipfile: zipFile,
      impl: implClass || "Main"
    });
  },

  unloadDex: function(zipFile) {
    GlobalEventDispatcher.sendRequest({
      type: "Dex:Unload",
      zipfile: zipFile
    });
  },

  menu: {
    _callbacks: [],
    _menuId: 1,
    toolsMenuID: -1,
    add: function() {
      let options;
      if (arguments.length == 1) {
        options = arguments[0];
      } else if (arguments.length == 3) {
          Log.w("Browser", "This menu addon API has been deprecated. Instead, use the options object API.");
          options = {
            name: arguments[0],
            callback: arguments[2]
          };
      } else {
         throw "Incorrect number of parameters";
      }

      options.type = "Menu:Add";
      options.id = this._menuId;

      GlobalEventDispatcher.sendRequest(options);
      this._callbacks[this._menuId] = options.callback;
      this._menuId++;
      return this._menuId - 1;
    },

    remove: function(aId) {
      GlobalEventDispatcher.sendRequest({ type: "Menu:Remove", id: aId });
    },

    update: function(aId, aOptions) {
      if (!aOptions)
        return;

      GlobalEventDispatcher.sendRequest({
        type: "Menu:Update",
        id: aId,
        options: aOptions
      });
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
   *
   *        checkbox:    A string to appear next to a checkbox under the notification
   *                     message. The button callback functions will be called with
   *                     the checked state as an argument.
   *
   *        actionText:  An object that specifies a clickable string, a type of action,
   *                     and a bundle blob for the consumer to create a click action.
   *                     { text: <text>,
   *                       type: <type>,
   *                       bundle: <blob-object> }
   *
   * @param aCategory
   *        Doorhanger type to display (e.g., LOGIN)
   */
    show: function(aMessage, aValue, aButtons, aTabID, aOptions, aCategory) {
      if (aButtons == null) {
        aButtons = [];
      }

      if (aButtons.length > 2) {
        console.log("Doorhanger can have a maximum of two buttons!");
        aButtons.length = 2;
      }

      aButtons.forEach((function(aButton) {
        this._callbacks[this._callbacksId] = { cb: aButton.callback, prompt: this._promptId };
        aButton.callback = this._callbacksId;
        this._callbacksId++;
      }).bind(this));

      this._promptId++;
      let json = {
        type: "Doorhanger:Add",
        message: aMessage,
        value: aValue,
        buttons: aButtons,
        // use the current tab if none is provided
        tabID: aTabID || BrowserApp.selectedTab.id,
        options: aOptions || {},
        category: aCategory
      };
      WindowEventDispatcher.sendRequest(json);
    },

    hide: function(aValue, aTabID) {
      WindowEventDispatcher.sendRequest({
        type: "Doorhanger:Remove",
        value: aValue,
        tabID: aTabID
      });
    }
  },

  onEvent: function (event, data, callback) {
    if (event == "Doorhanger:Reply") {
      let reply_id = data["callback"];

      if (this.doorhanger._callbacks[reply_id]) {
        // Pass the value of the optional checkbox to the callback
        let checked = data["checked"];
        this.doorhanger._callbacks[reply_id].cb(checked, data.inputs);

        let prompt = this.doorhanger._callbacks[reply_id].prompt;
        for (let id in this.doorhanger._callbacks) {
          if (this.doorhanger._callbacks[id].prompt == prompt) {
            delete this.doorhanger._callbacks[id];
          }
        }
      }
    } else if (event == "Menu:Clicked") {
      if (this.menu._callbacks[data.item]) {
        this.menu._callbacks[data.item]();
      }
    }
  },

  contextmenus: {
    items: {}, //  a list of context menu items that we may show
    DEFAULT_HTML5_ORDER: -1, // Sort order for HTML5 context menu items

    init: function() {
      // Accessing "NativeWindow.contextmenus" initializes context menus if needed.
      BrowserApp.deck.addEventListener(
          "contextmenu", (e) => NativeWindow.contextmenus.show(e));
    },

    add: function() {
      let args;
      if (arguments.length == 1) {
        args = arguments[0];
      } else if (arguments.length == 3) {
        args = {
          label : arguments[0],
          selector: arguments[1],
          callback: arguments[2]
        };
      } else {
        throw "Incorrect number of parameters";
      }

      if (!args.label)
        throw "Menu items must have a name";

      let cmItem = new ContextMenuItem(args);
      this.items[cmItem.id] = cmItem;
      return cmItem.id;
    },

    remove: function(aId) {
      delete this.items[aId];
    },

    // Although we do not use this ourselves anymore, add-ons may still
    // need it as it has been documented, so we shouldn't remove it.
    SelectorContext: function(aSelector) {
      return {
        matches: function(aElt) {
          if (aElt.matches)
            return aElt.matches(aSelector);
          return false;
        }
      };
    },

    linkOpenableNonPrivateContext: {
      matches: function linkOpenableNonPrivateContextMatches(aElement) {
        let doc = aElement.ownerDocument;
        if (!doc || PrivateBrowsingUtils.isContentWindowPrivate(doc.defaultView)) {
          return false;
        }

        return NativeWindow.contextmenus.linkOpenableContext.matches(aElement);
      }
    },

    linkOpenableContext: {
      matches: function linkOpenableContextMatches(aElement) {
        let uri = NativeWindow.contextmenus._getLink(aElement);
        if (uri) {
          let scheme = uri.scheme;
          let dontOpen = /^(javascript|mailto|news|snews|tel)$/;
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

    linkShareableContext: {
      matches: function linkShareableContextMatches(aElement) {
        let uri = NativeWindow.contextmenus._getLink(aElement);
        if (uri) {
          let scheme = uri.scheme;
          let dontShare = /^(about|chrome|file|javascript|mailto|resource|tel)$/;
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
          let dontBookmark = /^(mailto|tel)$/;
          return (scheme && !dontBookmark.test(scheme));
        }
        return false;
      }
    },

    emailLinkContext: {
      matches: function emailLinkContextMatches(aElement) {
        let uri = NativeWindow.contextmenus._getLink(aElement);
        if (uri)
          return uri.schemeIs("mailto");
        return false;
      }
    },

    phoneNumberLinkContext: {
      matches: function phoneNumberLinkContextMatches(aElement) {
        let uri = NativeWindow.contextmenus._getLink(aElement);
        if (uri)
          return uri.schemeIs("tel");
        return false;
      }
    },

    imageLocationCopyableContext: {
      matches: function imageLinkCopyableContextMatches(aElement) {
        if (aElement instanceof Ci.nsIDOMHTMLImageElement) {
          // The image is blocked by Tap-to-load Images
          if (aElement.hasAttribute("data-ctv-src") && !aElement.hasAttribute("data-ctv-show")) {
            return false;
          }
        }
        return (aElement instanceof Ci.nsIImageLoadingContent && aElement.currentURI);
      }
    },

    imageSaveableContext: {
      matches: function imageSaveableContextMatches(aElement) {
        if (aElement instanceof Ci.nsIDOMHTMLImageElement) {
          // The image is blocked by Tap-to-load Images
          if (aElement.hasAttribute("data-ctv-src") && !aElement.hasAttribute("data-ctv-show")) {
            return false;
          }
        }
        if (aElement instanceof Ci.nsIImageLoadingContent && aElement.currentURI) {
          // The image must be loaded to allow saving
          let request = aElement.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
          return (request && (request.imageStatus & request.STATUS_SIZE_AVAILABLE));
        }
        return false;
      }
    },

    imageShareableContext: {
      matches: function imageShareableContextMatches(aElement) {
        let imgSrc = '';
        if (aElement instanceof Ci.nsIDOMHTMLImageElement) {
          imgSrc = aElement.src;
        } else if (aElement instanceof Ci.nsIImageLoadingContent &&
            aElement.currentURI &&
            aElement.currentURI.spec) {
          imgSrc = aElement.currentURI.spec;
        }

        // In order to share an image, we need to pass the image src over IPC via an Intent (in
        // `ApplicationPackageManager.queryIntentActivities`). However, the transaction has a 1MB limit
        // (shared by all transactions in progress) - otherwise we crash! (bug 1243305)
        //   https://developer.android.com/reference/android/os/TransactionTooLargeException.html
        //
        // The transaction limit is 1MB and we arbitrarily choose to cap this transaction at 1/4 of that = 250,000 bytes.
        // In Java, a UTF-8 character is 1-4 bytes so, 250,000 bytes / 4 bytes/char = 62,500 char
        let MAX_IMG_SRC_LEN = 62500;
        let isTooLong = imgSrc.length >= MAX_IMG_SRC_LEN;
        return !isTooLong && this.NativeWindow.contextmenus.imageSaveableContext.matches(aElement);
      }.bind(this)
    },

    mediaSaveableContext: {
      matches: function mediaSaveableContextMatches(aElement) {
        return (aElement instanceof HTMLVideoElement ||
               aElement instanceof HTMLAudioElement);
      }
    },

    imageBlockingPolicyContext: {
      matches: function imageBlockingPolicyContextMatches(aElement) {
        if (aElement instanceof Ci.nsIDOMHTMLImageElement && aElement.getAttribute("data-ctv-src")) {
          // Only show the menuitem if we are blocking the image
          if (aElement.getAttribute("data-ctv-show") == "true") {
            return false;
          }
          return true;
        }
        return false;
      }
    },

    mediaContext: function(aMode) {
      return {
        matches: function(aElt) {
          if (aElt instanceof Ci.nsIDOMHTMLMediaElement) {
            let hasError = aElt.error != null || aElt.networkState == aElt.NETWORK_NO_SOURCE;
            if (hasError)
              return false;

            let paused = aElt.paused || aElt.ended;
            if (paused && aMode == "media-paused")
              return true;
            if (!paused && aMode == "media-playing")
              return true;
            let controls = aElt.controls;
            if (!controls && aMode == "media-hidingcontrols")
              return true;

            let muted = aElt.muted;
            if (muted && aMode == "media-muted")
              return true;
            else if (!muted && aMode == "media-unmuted")
              return true;
          }
          return false;
        }
      };
    },

    videoContext: function(aMode) {
      return {
        matches: function(aElt) {
          if (aElt instanceof HTMLVideoElement) {
            if (!aMode) {
              return true;
            }
            var isFullscreen = aElt.ownerDocument.fullscreenElement == aElt;
            if (aMode == "not-fullscreen") {
              return !isFullscreen;
            }
            if (aMode == "fullscreen") {
              return isFullscreen;
            }
          }
          return false;
        }
      };
    },

    /* Holds a WeakRef to the original target element this context menu was shown for.
     * Most API's will have to walk up the tree from this node to find the correct element
     * to act on
     */
    get _target() {
      if (this._targetRef)
        return this._targetRef.get();
      return null;
    },

    set _target(aTarget) {
      if (aTarget)
        this._targetRef = Cu.getWeakReference(aTarget);
      else this._targetRef = null;
    },

    get defaultContext() {
      delete this.defaultContext;
      return this.defaultContext = Strings.browser.GetStringFromName("browser.menu.context.default");
    },

    /* Gets menuitems for an arbitrary node
     * Parameters:
     *   element - The element to look at. If this element has a contextmenu attribute, the
     *             corresponding contextmenu will be used.
     */
    _getHTMLContextMenuItemsForElement: function(element) {
      let htmlMenu = element.contextMenu;
      if (!htmlMenu) {
        return [];
      }

      htmlMenu.sendShowEvent();

      return this._getHTMLContextMenuItemsForMenu(htmlMenu, element);
    },

    /* Add a menuitem for an HTML <menu> node
     * Parameters:
     *   menu - The <menu> element to iterate through for menuitems
     *   target - The target element these context menu items are attached to
     */
    _getHTMLContextMenuItemsForMenu: function(menu, target) {
      let items = [];
      for (let i = 0; i < menu.childNodes.length; i++) {
        let elt = menu.childNodes[i];
        if (!elt.label)
          continue;

        items.push(new HTMLContextMenuItem(elt, target));
      }

      return items;
    },

    // Searches the current list of menuitems to show for any that match this id
    _findMenuItem: function(aId) {
      if (!this.menus) {
        return null;
      }

      for (let context in this.menus) {
        let menu = this.menus[context];
        for (let i = 0; i < menu.length; i++) {
          if (menu[i].id === aId) {
            return menu[i];
          }
        }
      }
      return null;
    },

    // Returns true if there are any context menu items to show
    _shouldShow: function() {
      for (let context in this.menus) {
        let menu = this.menus[context];
        if (menu.length > 0) {
          return true;
        }
      }
      return false;
    },

    /* Returns a label to be shown in a tabbed ui if there are multiple "contexts". For instance, if this
     * is an image inside an <a> tag, we may have a "link" context and an "image" one.
     */
    _getContextType: function(element) {
      // For anchor nodes, we try to use the scheme to pick a string
      if (element instanceof Ci.nsIDOMHTMLAnchorElement) {
        let uri = this.makeURI(this._getLinkURL(element));
        try {
          return Strings.browser.GetStringFromName("browser.menu.context." + uri.scheme);
        } catch(ex) { }
      }

      // Otherwise we try the nodeName
      try {
        return Strings.browser.GetStringFromName("browser.menu.context." + element.nodeName.toLowerCase());
      } catch(ex) { }

      // Fallback to the default
      return this.defaultContext;
    },

    // Adds context menu items added through the add-on api
    _getNativeContextMenuItems: function(element, x, y) {
      let res = [];
      for (let itemId of Object.keys(this.items)) {
        let item = this.items[itemId];

        if (!this._findMenuItem(item.id) && item.matches(element, x, y)) {
          res.push(item);
        }
      }

      return res;
    },

    /* Checks if there are context menu items to show, and if it finds them
     * sends a contextmenu event to content. We also send showing events to
     * any html5 context menus we are about to show, and fire some local notifications
     * for chrome consumers to do lazy menuitem construction
     */
    show: function(event) {
      // Android Long-press / contextmenu event provides clientX/Y data. This is not provided
      // by mochitest: test_browserElement_inproc_ContextmenuEvents.html.
      if (!event.clientX || !event.clientY) {
        return;
      }

      // If the event was already defaultPrevented by somebody (web content, or
      // some other part of gecko), then don't do anything with it.
      if (event.defaultPrevented) {
        return;
      }

      // Use the highlighted element for the context menu target. When accessibility is
      // enabled, elements may not be highlighted so use the event target instead.
      this._target = BrowserEventHandler._highlightElement || event.target;
      if (!this._target) {
        return;
      }

      // Try to build a list of contextmenu items. If successful, actually show the
      // native context menu by passing the list to Java.
      this._buildMenu(event.clientX, event.clientY);
      if (this._shouldShow()) {
        BrowserEventHandler._cancelTapHighlight();

        // Consume / preventDefault the event, and show the contextmenu.
        event.preventDefault();
        this._innerShow(this._target, event.clientX, event.clientY);
        this._target = null;

        return;
      }

      // If no context-menu for long-press event, it may be meant to trigger text-selection.
      this.menus = null;
      Services.obs.notifyObservers(
        {target: this._target, x: event.clientX, y: event.clientY}, "context-menu-not-shown", "");
    },

    // Returns a title for a context menu. If no title attribute exists, will fall back to looking for a url
    _getTitle: function(node) {
      if (node.hasAttribute && node.hasAttribute("title")) {
        return node.getAttribute("title");
      }
      return this._getUrl(node);
    },

    // Returns a url associated with a node
    _getUrl: function(node) {
      if ((node instanceof Ci.nsIDOMHTMLAnchorElement && node.href) ||
          (node instanceof Ci.nsIDOMHTMLAreaElement && node.href)) {
        return this._getLinkURL(node);
      } else if (node instanceof Ci.nsIImageLoadingContent && node.currentURI) {
        // The image is blocked by Tap-to-load Images
        let originalURL = node.getAttribute("data-ctv-src");
        if (originalURL) {
          return originalURL;
        }
        return node.currentURI.spec;
      } else if (node instanceof Ci.nsIDOMHTMLMediaElement) {
        let srcUrl = node.currentSrc || node.src;
        // If URL prepended with blob or mediasource, we'll remove it.
        return srcUrl.replace(/^(?:blob|mediasource):/, '');
      }

      return "";
    },

    // Adds an array of menuitems to the current list of items to show, in the correct context
    _addMenuItems: function(items, context) {
        if (!this.menus[context]) {
          this.menus[context] = [];
        }
        this.menus[context] = this.menus[context].concat(items);
    },

    /* Does the basic work of building a context menu to show. Will combine HTML and Native
     * context menus items, as well as sorting menuitems into different menus based on context.
     */
    _buildMenu: function(x, y) {
      // now walk up the tree and for each node look for any context menu items that apply
      let element = this._target;

      // this.menus holds a hashmap of "contexts" to menuitems associated with that context
      // For instance, if the user taps an image inside a link, we'll have something like:
      // {
      //   link:  [ ContextMenuItem, ContextMenuItem ]
      //   image: [ ContextMenuItem, ContextMenuItem ]
      // }
      this.menus = {};

      while (element) {
        let context = this._getContextType(element);

        // First check for any html5 context menus that might exist...
        var items = this._getHTMLContextMenuItemsForElement(element);
        if (items.length > 0) {
          this._addMenuItems(items, context);
        }

        // then check for any context menu items registered in the ui.
        items = this._getNativeContextMenuItems(element, x, y);
        if (items.length > 0) {
          this._addMenuItems(items, context);
        }

        // walk up the tree and find more items to show
        element = element.parentNode;
      }
    },

    // Walks the DOM tree to find a title from a node
    _findTitle: function(node) {
      let title = "";
      while(node && !title) {
        title = this._getTitle(node);
        node = node.parentNode;
      }
      return title;
    },

    /* Reformats the list of menus to show into an object that can be sent to Prompt.jsm
     * If there is one menu, will return a flat array of menuitems. If there are multiple
     * menus, will return an array with appropriate tabs/items inside it. i.e. :
     * [
     *    { label: "link", items: [...] },
     *    { label: "image", items: [...] }
     * ]
     */
    _reformatList: function(target) {
      let contexts = Object.keys(this.menus);

      if (contexts.length === 1) {
        // If there's only one context, we'll only show a single flat single select list
        return this._reformatMenuItems(target, this.menus[contexts[0]]);
      }

      // If there are multiple contexts, we'll only show a tabbed ui with multiple lists
      return this._reformatListAsTabs(target, this.menus);
    },

    /* Reformats the list of menus to show into an object that can be sent to Prompt.jsm's
     * addTabs method. i.e. :
     * { link: [...], image: [...] } becomes
     * [ { label: "link", items: [...] } ]
     *
     * Also reformats items and resolves any parmaeters that aren't known until display time
     * (for instance Helper app menu items adjust their title to reflect what Helper App can be used for this link).
     */
    _reformatListAsTabs: function(target, menus) {
      let itemArray = [];

      // Sort the keys so that "link" is always first
      let contexts = Object.keys(this.menus);
      contexts.sort((context1, context2) => {
        if (context1 === this.defaultContext) {
          return -1;
        } else if (context2 === this.defaultContext) {
          return 1;
        }
        return 0;
      });

      contexts.forEach(context => {
        itemArray.push({
          label: context,
          items: this._reformatMenuItems(target, menus[context])
        });
      });

      return itemArray;
    },

    /* Reformats an array of ContextMenuItems into an array that can be handled by Prompt.jsm. Also reformats items
     * and resolves any parmaeters that aren't known until display time
     * (for instance Helper app menu items adjust their title to reflect what Helper App can be used for this link).
     */
    _reformatMenuItems: function(target, menuitems) {
      let itemArray = [];

      for (let i = 0; i < menuitems.length; i++) {
        let t = target;
        while(t) {
          if (menuitems[i].matches(t)) {
            let val = menuitems[i].getValue(t);

            // hidden menu items will return null from getValue
            if (val) {
              itemArray.push(val);
              break;
            }
          }

          t = t.parentNode;
        }
      }

      return itemArray;
    },

    // Called where we're finally ready to actually show the contextmenu. Sorts the items and shows a prompt.
    _innerShow: function(target, x, y) {
      Haptic.performSimpleAction(Haptic.LongPress);

      // spin through the tree looking for a title for this context menu
      let title = this._findTitle(target);

      for (let context in this.menus) {
        let menu = this.menus[context];
        menu.sort((a,b) => {
          if (a.order === b.order) {
            return 0;
          }
          return (a.order > b.order) ? 1 : -1;
        });
      }

      let useTabs = Object.keys(this.menus).length > 1;
      let prompt = new Prompt({
        window: target.ownerGlobal,
        title: useTabs ? undefined : title
      });

      let items = this._reformatList(target);
      if (useTabs) {
        prompt.addTabs({
          id: "tabs",
          items: items
        });
      } else {
        prompt.setSingleChoiceItems(items);
      }

      prompt.show(this._promptDone.bind(this, target, x, y, items));
    },

    // Called when the contextmenu prompt is closed
    _promptDone: function(target, x, y, items, data) {
      if (data.button == -1) {
        // Prompt was cancelled, or an ActionView was used.
        return;
      }

      let selectedItemId;
      if (data.tabs) {
        let menu = items[data.tabs.tab];
        selectedItemId = menu.items[data.tabs.item].id;
      } else {
        selectedItemId = items[data.list[0]].id
      }

      let selectedItem = this._findMenuItem(selectedItemId);
      this.menus = null;

      if (!selectedItem || !selectedItem.matches || !selectedItem.callback) {
        return;
      }

      // for menuitems added using the native UI, pass the dom element that matched that item to the callback
      while (target) {
        if (selectedItem.matches(target, x, y)) {
          selectedItem.callback(target, x, y);
          break;
        }
        target = target.parentNode;
      }
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
          let url = this._getLinkURL(aElement);
          return Services.io.newURI(url);
        } catch (e) {}
      }
      return null;
    },

    _disableRestricted: function _disableRestricted(restriction, selector) {
      return {
        matches: function _disableRestrictedMatches(aElement, aX, aY) {
          if (!ParentalControls.isAllowed(ParentalControls[restriction])) {
            return false;
          }

          return selector.matches(aElement, aX, aY);
        }
      };
    },

    _getLinkURL: function ch_getLinkURL(aLink) {
      let href = aLink.href;
      if (href)
        return href;

      href = aLink.getAttribute("href") ||
             aLink.getAttributeNS(kXLinkNamespace, "href");
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
      Snackbars.show(Strings.browser.GetStringFromName("selectionHelper.textCopied"), Snackbars.LENGTH_LONG);
    },

    _stripScheme: function(aString) {
      let index = aString.indexOf(":");
      return aString.slice(index + 1);
    }
  }
};

XPCOMUtils.defineLazyModuleGetter(this, "PageActions",
                                  "resource://gre/modules/PageActions.jsm");

// These alias to the old, deprecated NativeWindow interfaces
[
  ["pageactions", "resource://gre/modules/PageActions.jsm", "PageActions"],
  ["toast", "resource://gre/modules/Snackbars.jsm", "Snackbars"]
].forEach(item => {
  let [name, script, exprt] = item;

  XPCOMUtils.defineLazyGetter(NativeWindow, name, () => {
    var err = Strings.browser.formatStringFromName("nativeWindow.deprecated", ["NativeWindow." + name, script], 2);
    Cu.reportError(err);

    let sandbox = {};
    Cu.import(script, sandbox);
    return sandbox[exprt];
  });
});

var LightWeightThemeWebInstaller = {
  init: function sh_init() {
    let temp = {};
    Cu.import("resource://gre/modules/LightweightThemeConsumer.jsm", temp);
    let theme = new temp.LightweightThemeConsumer(document);
    BrowserApp.deck.addEventListener("InstallBrowserTheme", this, false, true);
    BrowserApp.deck.addEventListener("PreviewBrowserTheme", this, false, true);
    BrowserApp.deck.addEventListener("ResetBrowserThemePreview", this, false, true);

    if (ParentalControls.parentalControlsEnabled &&
        !this._manager.currentTheme &&
        ParentalControls.isAllowed(ParentalControls.DEFAULT_THEME)) {
      // We are using the DEFAULT_THEME restriction to differentiate between restricted profiles & guest mode - Bug 1199596
      this._installParentalControlsTheme();
    }
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "InstallBrowserTheme":
      case "PreviewBrowserTheme":
      case "ResetBrowserThemePreview":
        // ignore requests from background tabs
        if (event.target.ownerGlobal.top != content)
          return;
    }

    switch (event.type) {
      case "InstallBrowserTheme":
        this._installRequest(event);
        break;
      case "PreviewBrowserTheme":
        this._preview(event);
        break;
      case "ResetBrowserThemePreview":
        this._resetPreview(event);
        break;
      case "pagehide":
      case "TabSelect":
        this._resetPreview();
        break;
    }
  },

  get _manager () {
    let temp = {};
    Cu.import("resource://gre/modules/LightweightThemeManager.jsm", temp);
    delete this._manager;
    return this._manager = temp.LightweightThemeManager;
  },

  _installParentalControlsTheme: function() {
    let mgr = this._manager;
    let parentalControlsTheme = {
      "headerURL": "resource://android/assets/parental_controls_theme.png",
      "name": "Parental Controls Theme",
      "id": "parental-controls-theme@mozilla.org"
    };

    mgr.addBuiltInTheme(parentalControlsTheme);
    mgr.themeChanged(parentalControlsTheme);
  },

  _installRequest: function (event) {
    let node = event.target;
    let data = this._getThemeFromNode(node);
    if (!data)
      return;

    if (this._isAllowed(node)) {
      this._install(data);
      return;
    }

    let allowButtonText = Strings.browser.GetStringFromName("lwthemeInstallRequest.allowButton");
    let message = Strings.browser.formatStringFromName("lwthemeInstallRequest.message", [node.ownerDocument.location.hostname], 1);
    let buttons = [{
      label: allowButtonText,
      callback: function () {
        LightWeightThemeWebInstaller._install(data);
      },
      positive: true
    }];

    NativeWindow.doorhanger.show(message, "Personas", buttons, BrowserApp.selectedTab.id);
  },

  _install: function (newLWTheme) {
    this._manager.currentTheme = newLWTheme;
  },

  _previewWindow: null,
  _preview: function (event) {
    if (!this._isAllowed(event.target))
      return;
    let data = this._getThemeFromNode(event.target);
    if (!data)
      return;
    this._resetPreview();

    this._previewWindow = event.target.ownerGlobal;
    this._previewWindow.addEventListener("pagehide", this, true);
    BrowserApp.deck.addEventListener("TabSelect", this);
    this._manager.previewTheme(data);
  },

  _resetPreview: function (event) {
    if (!this._previewWindow ||
        event && !this._isAllowed(event.target))
      return;

    this._previewWindow.removeEventListener("pagehide", this, true);
    this._previewWindow = null;
    BrowserApp.deck.removeEventListener("TabSelect", this);

    this._manager.resetPreview();
  },

  _isAllowed: function (node) {
    // Make sure the whitelist has been imported to permissions
    PermissionsUtils.importFromPrefs("xpinstall.", "install");

    let pm = Services.perms;

    let uri = node.ownerDocument.documentURIObject;
    if (!uri.schemeIs("https")) {
      return false;
    }

    return pm.testPermission(uri, "install") == pm.ALLOW_ACTION;
  },

  _getThemeFromNode: function (node) {
    return this._manager.parseTheme(node.getAttribute("data-browsertheme"), node.baseURI);
  }
};

var DesktopUserAgent = {
  DESKTOP_UA: null,
  TCO_DOMAIN: "t.co",
  TCO_REPLACE: / Gecko.*/,

  init: function ua_init() {
    GlobalEventDispatcher.registerListener(this, "DesktopMode:Change");
    UserAgentOverrides.addComplexOverride(this.onRequest.bind(this));

    // See https://developer.mozilla.org/en/Gecko_user_agent_string_reference
    this.DESKTOP_UA = Cc["@mozilla.org/network/protocol;1?name=http"]
                        .getService(Ci.nsIHttpProtocolHandler).userAgent
                        .replace(/Android \d.+?; [a-zA-Z]+/, "X11; Linux x86_64")
                        .replace(/Gecko\/[0-9\.]+/, "Gecko/20100101");
  },

  onRequest: function(channel, defaultUA) {
    if (AppConstants.NIGHTLY_BUILD && this.TCO_DOMAIN == channel.URI.host) {
      // Force the referrer
      channel.referrer = channel.URI;

      // Send a bot-like UA to t.co to get a real redirect. We strip off the
      // "Gecko/x.y Firefox/x.y" part
      return defaultUA.replace(this.TCO_REPLACE, "");
    }

    let channelWindow = this._getWindowForRequest(channel);
    let tab = BrowserApp.getTabForWindow(channelWindow);
    if (tab) {
      return this.getUserAgentForTab(tab);
    }

    return null;
  },

  getUserAgentForTab: function ua_getUserAgentForTab(aTab) {
    // Send desktop UA if "Request Desktop Site" is enabled.
    if (aTab.desktopMode) {
      return this.DESKTOP_UA;
    }

    return null;
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
    if (loadContext) {
      try {
        return loadContext.associatedWindow;
      } catch (e) {
        // loadContext.associatedWindow can throw when there's no window
      }
    }
    return null;
  },

  onEvent: function ua_onEvent(event, data, callback) {
    if (event === "DesktopMode:Change") {
      let tab = BrowserApp.getTabForId(data.tabId);
      if (tab) {
        tab.reloadWithMode(data.desktopMode);
      }
    }
  },
};


function nsBrowserAccess() {
}

nsBrowserAccess.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBrowserDOMWindow]),

  _getBrowser: function _getBrowser(aURI, aOpener, aWhere, aFlags) {
    let isExternal = !!(aFlags & Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);
    if (isExternal && aURI && aURI.schemeIs("chrome"))
      return null;

    let loadflags = isExternal ?
                      Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL :
                      Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW) {
      if (isExternal) {
        aWhere = Services.prefs.getIntPref("browser.link.open_external");
      } else {
        aWhere = Services.prefs.getIntPref("browser.link.open_newwindow");
      }
    }

    Services.io.offline = false;

    let referrer;
    if (aOpener) {
      try {
        let location = aOpener.location;
        referrer = Services.io.newURI(location);
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

    // If OPEN_SWITCHTAB was not handled above, we need to open a new tab,
    // along with other OPEN_ values that create a new tab.
    let newTab = (aWhere == Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW ||
                  aWhere == Ci.nsIBrowserDOMWindow.OPEN_NEWTAB ||
                  aWhere == Ci.nsIBrowserDOMWindow.OPEN_SWITCHTAB);
    let isPrivate = false;

    if (aOpener != null) {
      let parent = BrowserApp.getTabForWindow(aOpener.top);
      if (parent != null) {
        newTab = newTab && parent.tabType != "CUSTOMTAB";
      }
    }

    if (newTab) {
      let parentId = -1;
      if (!isExternal && aOpener) {
        let parent = BrowserApp.getTabForWindow(aOpener.top);
        if (parent) {
          parentId = parent.id;
          isPrivate = PrivateBrowsingUtils.isBrowserPrivate(parent.browser);
        }
      }

      let openerWindow = (aFlags & Ci.nsIBrowserDOMWindow.OPEN_NO_OPENER) ? null : aOpener;
      // BrowserApp.addTab calls loadURIWithFlags with the appropriate params
      let tab = BrowserApp.addTab(aURI ? aURI.spec : "about:blank", { flags: loadflags,
                                                                      referrerURI: referrer,
                                                                      external: isExternal,
                                                                      parentId: parentId,
                                                                      opener: openerWindow,
                                                                      selected: true,
                                                                      isPrivate: isPrivate,
                                                                      pinned: pinned });

      return tab.browser;
    }

    // OPEN_CURRENTWINDOW and illegal values
    let browser = BrowserApp.selectedBrowser;
    if (aURI && browser) {
      browser.loadURIWithFlags(aURI.spec, loadflags, referrer, null, null);
    }

    return browser;
  },

  openURI: function browser_openURI(aURI, aOpener, aWhere, aFlags) {
    let browser = this._getBrowser(aURI, aOpener, aWhere, aFlags);
    return browser ? browser.contentWindow : null;
  },

  openURIInFrame: function browser_openURIInFrame(aURI, aParams, aWhere, aFlags) {
    let browser = this._getBrowser(aURI, null, aWhere, aFlags);
    return browser ? browser.QueryInterface(Ci.nsIFrameLoaderOwner) : null;
  },

  isTabContentWindow: function(aWindow) {
    return BrowserApp.getBrowserForWindow(aWindow) != null;
  },

  canClose() {
    return BrowserUtils.canCloseWindow(window);
  },
};


function Tab(aURL, aParams) {
  this.filter = null;
  this.browser = null;
  this.id = 0;
  this._parentId = -1;
  this.lastTouchedAt = Date.now();
  this.contentDocumentIsDisplayed = true;
  this.pluginDoorhangerTimeout = null;
  this.shouldShowPluginDoorhanger = true;
  this.clickToPlayPluginsActivated = false;
  this.desktopMode = false;
  this.originalURI = null;
  this.hasTouchListener = false;
  this.playingAudio = false;

  this.create(aURL, aParams);
}

/*
 * Sanity limit for URIs passed to UI code.
 *
 * 2000 is the typical industry limit, largely due to older IE versions.
 *
 * We use 25000, so we'll allow almost any value through.
 *
 * Still, this truncation doesn't affect history, so this is only a practical
 * concern in two ways: the truncated value is used when editing URIs, and as
 * the key for favicon fetches.
 */
const MAX_URI_LENGTH = 25000;

/*
 * Similar restriction for titles. This is only a display concern.
 */
const MAX_TITLE_LENGTH = 255;

/**
 * Ensure that a string is of a sane length.
 */
function truncate(text, max) {
  if (!text || !max) {
    return text;
  }

  if (text.length <= max) {
    return text;
  }

  return text.slice(0, max) + "…";
}

Tab.prototype = {
  create: function(aURL, aParams) {
    if (this.browser)
      return;

    aParams = aParams || {};

    this.browser = document.createElement("browser");
    this.browser.setAttribute("type", "content");
    this.browser.setAttribute("messagemanagergroup", "browsers");

    if (Preferences.get("browser.tabs.remote.force-enable", false)) {
      this.browser.setAttribute("remote", "true");
    }

    this.browser.permanentKey = {};

    // Check if we have a "parent" window which we need to set as our opener
    if ("opener" in aParams) {
      this.browser.presetOpenerWindow(aParams.opener);
    }

    // Make sure the previously selected panel remains selected. The selected panel of a deck is
    // not stable when panels are added.
    let selectedPanel = BrowserApp.deck.selectedPanel;
    BrowserApp.deck.appendChild(this.browser);
    BrowserApp.deck.selectedPanel = selectedPanel;

    let attrs = {};

    // Must be called after appendChild so the docShell has been created.
    this.setActive(false);

    let isPrivate = ("isPrivate" in aParams) && aParams.isPrivate;
    if (isPrivate) {
      attrs['privateBrowsingId'] = 1;
    }

    this.browser.docShell.setOriginAttributes(attrs);

    // Set the new docShell load flags based on network state.
    if (Tabs.useCache) {
      this.browser.docShell.defaultLoadFlags |= Ci.nsIRequest.LOAD_FROM_CACHE;
    }

    this.browser.stop();

    // Only set tab uri if uri is valid
    let uri = null;
    let title = aParams.title || aURL;
    try {
      uri = Services.io.newURI(aURL).spec;
    } catch (e) {}

    // When the tab is stubbed from Java, there's a window between the stub
    // creation and the tab creation in Gecko where the stub could be removed
    // or the selected tab can change (which is easiest to hit during startup).
    // To prevent these races, we need to differentiate between tab stubs from
    // Java and new tabs from Gecko.
    let stub = false;

    // The authoritative list of possible tab types is the TabType enum in Tab.java.
    this.type = "tabType" in aParams ? aParams.tabType : "BROWSING";

    if (!aParams.zombifying) {
      if ("tabID" in aParams) {
        this.id = aParams.tabID;
        stub = true;
      } else {
        let jenv = JNI.GetForThread();
        let jTabs = JNI.LoadClass(jenv, "org.mozilla.gecko.Tabs", {
          static_methods: [
            { name: "getNextTabId", sig: "()I" }
          ],
        });
        this.id = jTabs.getNextTabId();
        JNI.UnloadClasses(jenv);
      }

      this.desktopMode = ("desktopMode" in aParams) ? aParams.desktopMode : false;
      this._parentId = ("parentId" in aParams && typeof aParams.parentId == "number")
                      ? aParams.parentId : -1;

      let message = {
        type: "Tab:Added",
        tabID: this.id,
        tabType: this.type,
        uri: truncate(uri, MAX_URI_LENGTH),
        parentId: this.parentId,
        tabIndex: ("tabIndex" in aParams) ? aParams.tabIndex : -1,
        external: ("external" in aParams) ? aParams.external : false,
        selected: ("selected" in aParams || aParams.cancelEditMode === true)
                  ? aParams.selected !== false || aParams.cancelEditMode === true : true,
        cancelEditMode: aParams.cancelEditMode === true,
        title: truncate(title, MAX_TITLE_LENGTH),
        delayLoad: aParams.delayLoad || false,
        desktopMode: this.desktopMode,
        isPrivate: isPrivate,
        stub: stub
      };
      GlobalEventDispatcher.sendRequest(message);
    }

    let flags = Ci.nsIWebProgress.NOTIFY_STATE_ALL |
                Ci.nsIWebProgress.NOTIFY_LOCATION |
                Ci.nsIWebProgress.NOTIFY_SECURITY;
    this.filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"].createInstance(Ci.nsIWebProgress);
    this.filter.addProgressListener(this, flags)
    this.browser.addProgressListener(this.filter, flags);
    this.browser.sessionHistory.addSHistoryListener(this);

    this.browser.addEventListener("DOMContentLoaded", this, true);
    this.browser.addEventListener("DOMFormHasPassword", this, true);
    this.browser.addEventListener("DOMInputPasswordAdded", this, true);
    this.browser.addEventListener("DOMLinkAdded", this, true);
    this.browser.addEventListener("DOMLinkChanged", this, true);
    this.browser.addEventListener("DOMMetaAdded", this);
    this.browser.addEventListener("DOMTitleChanged", this, true);
    this.browser.addEventListener("DOMAudioPlaybackStarted", this, true);
    this.browser.addEventListener("DOMAudioPlaybackStopped", this, true);
    this.browser.addEventListener("DOMWindowClose", this, true);
    this.browser.addEventListener("DOMWillOpenModalDialog", this, true);
    this.browser.addEventListener("DOMAutoComplete", this, true);
    this.browser.addEventListener("blur", this, true);
    this.browser.addEventListener("pageshow", this, true);
    this.browser.addEventListener("MozApplicationManifest", this, true);
    this.browser.addEventListener("TabPreZombify", this, true);
    this.browser.addEventListener("DOMWindowFocus", this, true);

    // Note that the XBL binding is untrusted
    this.browser.addEventListener("PluginBindingAttached", this, true, true);
    this.browser.addEventListener("VideoBindingAttached", this, true, true);
    this.browser.addEventListener("VideoBindingCast", this, true, true);

    Services.obs.addObserver(this, "before-first-paint");
    Services.obs.addObserver(this, "media-playback");
    Services.obs.addObserver(this, "media-playback-resumed");

    // Always initialise new tabs with basic session store data to avoid
    // problems with functions that always expect it to be present
    this.browser.__SS_data = {
      entries: [{
        url: aURL,
        title: truncate(title, MAX_TITLE_LENGTH)
      }],
      index: 1,
      desktopMode: this.desktopMode,
      isPrivate: isPrivate,
      tabId: this.id,
      parentId: this.parentId
    };

    if (aParams.delayLoad) {
      // If this is a zombie tab, mark the browser for delay loading, which will
      // restore the tab when selected using the session data added above
      this.browser.__SS_restore = true;
      this.browser.setAttribute("pending", "true");
    } else {
      let flags = "flags" in aParams ? aParams.flags : Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
      let postData = ("postData" in aParams && aParams.postData) ? aParams.postData.value : null;
      let referrerURI = "referrerURI" in aParams ? aParams.referrerURI : null;
      let charset = "charset" in aParams ? aParams.charset : null;

      // The search term the user entered to load the current URL
      this.userRequested = "userRequested" in aParams ? aParams.userRequested : "";
      this.isSearch = "isSearch" in aParams ? aParams.isSearch : false;

      try {
        this.browser.loadURIWithFlags(aURL, flags, referrerURI, charset, postData);
      } catch(e) {
        let message = {
          type: "Content:LoadError",
          tabID: this.id
        };
        GlobalEventDispatcher.sendRequest(message);
        dump("Handled load error: " + e);
      }
    }
  },

  /**
   * Reloads the tab with the desktop mode setting.
   */
  reloadWithMode: function (aDesktopMode) {
    // notify desktopmode for PIDOMWindow
    let win = this.browser.contentWindow;
    let dwi = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    dwi.setDesktopModeViewport(aDesktopMode);

    // Set desktop mode for tab and send change to Java
    if (this.desktopMode != aDesktopMode) {
      this.desktopMode = aDesktopMode;
      GlobalEventDispatcher.sendRequest({
        type: "DesktopMode:Changed",
        desktopMode: aDesktopMode,
        tabID: this.id
      });
    }

    // Only reload the page for http/https schemes
    let currentURI = this.browser.currentURI;
    if (!currentURI.schemeIs("http") && !currentURI.schemeIs("https"))
      return;

    let url = currentURI.spec;
    // We need LOAD_FLAGS_BYPASS_CACHE here since we're changing the User-Agent
    // string, and servers typically don't use the Vary: User-Agent header, so
    // not doing this means that we'd get some of the previously cached content.
    let flags = Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE |
                Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY;
    if (this.originalURI && !this.originalURI.equals(currentURI)) {
      // We were redirected; reload the original URL
      url = this.originalURI.spec;
    }

    this.browser.docShell.loadURI(url, flags, null, null, null);
  },

  destroy: function() {
    if (!this.browser)
      return;

    this.browser.removeProgressListener(this.filter);
    this.filter.removeProgressListener(this);
    this.filter = null;
    this.browser.sessionHistory.removeSHistoryListener(this);

    this.browser.removeEventListener("DOMContentLoaded", this, true);
    this.browser.removeEventListener("DOMFormHasPassword", this, true);
    this.browser.removeEventListener("DOMInputPasswordAdded", this, true);
    this.browser.removeEventListener("DOMLinkAdded", this, true);
    this.browser.removeEventListener("DOMLinkChanged", this, true);
    this.browser.removeEventListener("DOMMetaAdded", this);
    this.browser.removeEventListener("DOMTitleChanged", this, true);
    this.browser.removeEventListener("DOMAudioPlaybackStarted", this, true);
    this.browser.removeEventListener("DOMAudioPlaybackStopped", this, true);
    this.browser.removeEventListener("DOMWindowClose", this, true);
    this.browser.removeEventListener("DOMWillOpenModalDialog", this, true);
    this.browser.removeEventListener("DOMAutoComplete", this, true);
    this.browser.removeEventListener("blur", this, true);
    this.browser.removeEventListener("pageshow", this, true);
    this.browser.removeEventListener("MozApplicationManifest", this, true);
    this.browser.removeEventListener("TabPreZombify", this, true);
    this.browser.removeEventListener("DOMWindowFocus", this, true);

    this.browser.removeEventListener("PluginBindingAttached", this, true, true);
    this.browser.removeEventListener("VideoBindingAttached", this, true, true);
    this.browser.removeEventListener("VideoBindingCast", this, true, true);

    Services.obs.removeObserver(this, "before-first-paint");
    Services.obs.removeObserver(this, "media-playback");
    Services.obs.removeObserver(this, "media-playback-resumed");

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

    this.lastTouchedAt = Date.now();

    if (aActive) {
      this.browser.setAttribute("primary", "true");
      this.browser.focus();
      this.browser.docShellIsActive = true;
      Reader.updatePageAction(this);
      ExternalApps.updatePageAction(this.browser.currentURI, this.browser.contentDocument);
    } else {
      this.browser.removeAttribute("primary");
      this.browser.docShellIsActive = false;
      this.browser.blur();
    }
  },

  getActive: function getActive() {
    return this.browser.docShellIsActive;
  },

  /**
   * Unloads the tab from memory to free up resources. The tab will be restored from its session
   * store data either automatically when it gets selected or after calling unzombify().
   */
  zombify: function zombify() {
    let browser = this.browser;
    let data = browser.__SS_data;
    let extra = browser.__SS_extdata;

    // Notify any interested parties (e.g. the session store)
    // that the original tab object is going to be destroyed
    let evt = new UIEvent("TabPreZombify", {"bubbles":true, "cancelable":false, "view":window});
    browser.dispatchEvent(evt);

    // We need this data to correctly create the new browser
    // If this browser is already a zombie, fallback to the session data
    let currentURL = browser.__SS_restore ? data.entries[0].url : browser.currentURI.spec;
    let isPrivate = PrivateBrowsingUtils.isBrowserPrivate(browser);

    this.destroy();
    this.create(currentURL, { zombifying: true, delayLoad: true, isPrivate: isPrivate });

    // Reattach session store data and flag this browser so it is restored on select
    browser = this.browser;
    browser.__SS_data = data;
    browser.__SS_extdata = extra;
    browser.__SS_restore = true;
    browser.setAttribute("pending", "true");

    // Notify the session store to reattach its listeners to the recreated tab object
    evt = new UIEvent("TabPostZombify", {"bubbles":true, "cancelable":false, "view":window});
    browser.dispatchEvent(evt);
  },

  /**
   * Restores the tab and reloads its contents if it is unloaded from memory and set for delay
   * loading ("zombified").
   */
  unzombify: function unzombify() {
    let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
    ss.restoreZombieTab(this);
  },

  // These constants are used to prioritize high quality metadata over low quality data, so that
  // we can collect data as we find meta tags, and replace low quality metadata with higher quality
  // matches. For instance a msApplicationTile icon is a better tile image than an og:image tag.
  METADATA_GOOD_MATCH: 10,
  METADATA_NORMAL_MATCH: 1,

  addMetadata: function(type, value, quality = 1) {
    if (!this.metatags) {
      this.metatags = {
        url: this.browser.currentURI.specIgnoringRef
      };
    }

    if (type == "touchIconList") {
      if (!this.metatags['touchIconList']) {
        this.metatags['touchIconList'] = {};
      }
      this.metatags.touchIconList[quality] = value;
    } else if (!this.metatags[type] || this.metatags[type + "_quality"] < quality) {
      this.metatags[type] = value;
      this.metatags[type + "_quality"] = quality;
    }
  },

  sanitizeRelString: function(linkRel) {
    // Sanitize the rel string
    let list = [];
    if (linkRel) {
      list = linkRel.toLowerCase().split(/\s+/);
      let hash = {};
      list.forEach(function(value) { hash[value] = true; });
      list = [];
      for (let rel in hash)
      list.push("[" + rel + "]");
    }
    return list;
  },

  makeFaviconMessage: function(eventTarget) {
    // We want to get the largest icon size possible for our UI.
    let maxSize = 0;

    // We use the sizes attribute if available
    // see http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#rel-icon
    if (eventTarget.hasAttribute("sizes")) {
      let sizes = eventTarget.getAttribute("sizes").toLowerCase();

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
    return {
      type: "Link:Favicon",
      tabID: this.id,
      href: resolveGeckoURI(eventTarget.href),
      size: maxSize,
      mime: eventTarget.getAttribute("type") || ""
    };
  },

  makeFeedMessage: function(eventTarget, targetType) {
    try {
      // urlSecurityCeck will throw if things are not OK
      ContentAreaUtils.urlSecurityCheck(eventTarget.href,
            eventTarget.ownerDocument.nodePrincipal,
            Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL);

      if (!this.browser.feeds)
        this.browser.feeds = [];

      this.browser.feeds.push({
        href: eventTarget.href,
        title: eventTarget.title,
        type: targetType
      });

      return {
        type: "Link:Feed",
        tabID: this.id
      };
    } catch (e) {
        return null;
    }
  },

  makeManifestMessage: function(target) {
    return {
      type: "Link:Manifest",
      href: target.href,
      tabID: this.id
    };
  },

  sendOpenSearchMessage: function(eventTarget) {
    let type = eventTarget.type && eventTarget.type.toLowerCase();
    // Replace all starting or trailing spaces or spaces before "*;" globally w/ "".
    type = type.replace(/^\s+|\s*(?:;.*)?$/g, "");

    // Check that type matches opensearch.
    let isOpenSearch = (type == "application/opensearchdescription+xml");
    if (isOpenSearch && eventTarget.title && /^(?:https?|ftp):/i.test(eventTarget.href)) {
      Services.search.init(() => {
        let visibleEngines = Services.search.getVisibleEngines();
        // NOTE: Engines are currently identified by name, but this can be changed
        // when Engines are identified by URL (see bug 335102).
        if (visibleEngines.some(function(e) {
          return e.name == eventTarget.title;
        })) {
          // This engine is already present, do nothing.
          return null;
        }

        if (this.browser.engines) {
          // This engine has already been handled, do nothing.
          if (this.browser.engines.some(function(e) {
            return e.url == eventTarget.href;
          })) {
            return null;
          }
        } else {
            this.browser.engines = [];
        }

        // Get favicon.
        let iconURL = eventTarget.ownerDocument.documentURIObject.prePath + "/favicon.ico";

        let newEngine = {
          title: eventTarget.title,
          url: eventTarget.href,
          iconURL: iconURL
        };

        this.browser.engines.push(newEngine);

        // Don't send a message to display engines if we've already handled an engine.
        if (this.browser.engines.length > 1)
          return null;

        // Broadcast message that this tab contains search engines that should be visible.
        GlobalEventDispatcher.sendRequest({
          type: "Link:OpenSearch",
          tabID: this.id,
          visible: true
        });
      });
    }
  },

  get parentId() {
    return this._parentId;
  },

  set parentId(aParentId) {
    let newParentId = (typeof aParentId == "number") ? aParentId : -1;
    this._parentId = newParentId;
    GlobalEventDispatcher.sendRequest({
      type: "Tab:SetParentId",
      tabID: this.id,
      parentID: newParentId
    });
  },

  get currentURI() {
    if (!this.browser.__SS_restore) {
      return this.browser.currentURI;
    } else {
      // For zombie tabs we need to fall back to the session store data.
      let data = this.browser.__SS_data;
      let url = data.entries[data.index - 1].url;
      return Services.io.newURI(url);
    }
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "DOMContentLoaded": {
        let target = aEvent.originalTarget;

        // ignore on frames and other documents
        if (target != this.browser.contentDocument)
          return;

        // Sample the background color of the page and pass it along. (This is used to draw the
        // checkerboard.) Right now we don't detect changes in the background color after this
        // event fires; it's not clear that doing so is worth the effort.
        var backgroundColor = null;
        try {
          let { contentDocument, contentWindow } = this.browser;
          let computedStyle = contentWindow.getComputedStyle(contentDocument.body);
          backgroundColor = computedStyle.backgroundColor;
        } catch (e) {
          // Ignore. Catching and ignoring exceptions here ensures that Talos succeeds.
        }

        let docURI = target.documentURI;
        let errorType = "";
        if (docURI.startsWith("about:certerror")) {
          errorType = "certerror";
        }
        else if (docURI.startsWith("about:blocked")) {
          errorType = "blocked";
        }
        else if (docURI.startsWith("about:neterror")) {
          let error = docURI.search(/e\=/);
          let duffUrl = docURI.search(/\&u\=/);
          let errorExtra = decodeURIComponent(docURI.slice(error + 2, duffUrl));
          // Here is a list of errorExtra types (et_*)
          // http://mxr.mozilla.org/mozilla-central/source/mobile/android/chrome/content/netError.xhtml#287
          if (errorExtra == "fileAccessDenied") {
            // Check if we already have the permissions, then - if we do not have them, show the prompt and reload the page.
            // If we already have them, it means access to file was denied.
            RuntimePermissions.checkPermission(RuntimePermissions.WRITE_EXTERNAL_STORAGE).then((permissionAlreadyGranted) => {
              if (!permissionAlreadyGranted) {
                RuntimePermissions.waitForPermissions(RuntimePermissions.WRITE_EXTERNAL_STORAGE).then((permissionGranted) => {
                  if (permissionGranted) {
                    this.browser.reload();
                  }
                });
              }
            });
          }

          UITelemetry.addEvent("neterror.1", "content", null, errorExtra);
          errorType = "neterror";
        }

        // Attach a listener to watch for "click" events bubbling up from error
        // pages and other similar page. This lets us fix bugs like 401575 which
        // require error page UI to do privileged things, without letting error
        // pages have any privilege themselves.
        if (docURI.startsWith("about:neterror")) {
          NetErrorHelper.attachToBrowser(this.browser);
        }

        GlobalEventDispatcher.sendRequest({
          type: "Content:DOMContentLoaded",
          tabID: this.id,
          bgColor: backgroundColor,
          errorType: errorType,
          metadata: this.metatags,
        });

        // Reset isSearch so that the userRequested term will be erased on next page load
        this.metatags = null;

        if (docURI.startsWith("about:certerror") || docURI.startsWith("about:blocked")) {
          this.browser.addEventListener("click", ErrorPageEventHandler, true);
          let listener = function() {
            this.browser.removeEventListener("click", ErrorPageEventHandler, true);
            this.browser.removeEventListener("pagehide", listener, true);
          }.bind(this);

          this.browser.addEventListener("pagehide", listener, true);
        }

        if (!docURI.startsWith("about:")) {
          WebsiteMetadata.parseAsynchronously(this.browser.contentDocument);
        }

        break;
      }

      case "DOMFormHasPassword": {
        LoginManagerContent.onDOMFormHasPassword(aEvent,
                                                 this.browser.contentWindow);

        // Send logins for this hostname to Java.
        let hostname = aEvent.target.baseURIObject.prePath;
        let foundLogins = Services.logins.findLogins({}, hostname, "", "");
        if (foundLogins.length > 0) {
          let displayHost = IdentityHandler.getEffectiveHost();
          let title = { text: displayHost, resource: hostname };
          let selectObj = { title: title, logins: foundLogins };
          GlobalEventDispatcher.sendRequest({
            type: "Doorhanger:Logins",
            data: selectObj
          });
        }
        break;
      }

      case "DOMInputPasswordAdded": {
        LoginManagerContent.onDOMInputPasswordAdded(aEvent,
                                                    this.browser.contentWindow);
      }

      case "DOMMetaAdded":
        let target = aEvent.originalTarget;
        let browser = BrowserApp.getBrowserForDocument(target.ownerDocument);

        switch (target.name) {
          case "msapplication-TileImage":
            this.addMetadata("tileImage", browser.currentURI.resolve(target.content), this.METADATA_GOOD_MATCH);
            break;
          case "msapplication-TileColor":
            this.addMetadata("tileColor", target.content, this.METADATA_GOOD_MATCH);
            break;
        }

        break;

      case "DOMLinkAdded":
      case "DOMLinkChanged": {
        let jsonMessage = null;
        let target = aEvent.originalTarget;
        if (!target.href || target.disabled)
          return;

        // Ignore on frames and other documents
        if (target.ownerDocument != this.browser.contentDocument)
          return;

        // Sanitize rel link
        let list = this.sanitizeRelString(target.rel);
        if (list.indexOf("[icon]") != -1) {
          jsonMessage = this.makeFaviconMessage(target);
        } else if (list.indexOf("[apple-touch-icon]") != -1 ||
            list.indexOf("[apple-touch-icon-precomposed]") != -1) {
          jsonMessage = this.makeFaviconMessage(target);
          jsonMessage['type'] = 'Link:Touchicon';
          this.addMetadata("touchIconList", jsonMessage.href, jsonMessage.size);
        } else if (list.indexOf("[alternate]") != -1 && aEvent.type == "DOMLinkAdded") {
          let type = target.type.toLowerCase().replace(/^\s+|\s*(?:;.*)?$/g, "");
          let isFeed = (type == "application/rss+xml" || type == "application/atom+xml");

          if (!isFeed)
            return;

          jsonMessage = this.makeFeedMessage(target, type);
        } else if (list.indexOf("[search]") != -1 && aEvent.type == "DOMLinkAdded") {
          this.sendOpenSearchMessage(target);
        } else if (list.indexOf("[manifest]") != -1 &&
                   aEvent.type == "DOMLinkAdded" &&
                   Services.prefs.getBoolPref("manifest.install.enabled")) {
          jsonMessage = this.makeManifestMessage(target);
        }
        if (!jsonMessage)
         return;

        GlobalEventDispatcher.sendRequest(jsonMessage);
        break;
      }

      case "DOMTitleChanged": {
        if (!aEvent.isTrusted)
          return;

        // ignore on frames and other documents
        if (aEvent.originalTarget != this.browser.contentDocument)
          return;

        GlobalEventDispatcher.sendRequest({
          type: "Content:DOMTitleChanged",
          tabID: this.id,
          title: truncate(aEvent.target.title, MAX_TITLE_LENGTH)
        });
        break;
      }

      case "TabPreZombify": {
        if (!this.playingAudio) {
          return;
        }
        // Fall through to the DOMAudioPlayback events, so the
        // audio playback indicator gets reset upon zombification.
      }
      case "DOMAudioPlaybackStarted":
      case "DOMAudioPlaybackStopped": {
        if (!Services.prefs.getBoolPref("browser.tabs.showAudioPlayingIcon") ||
            !aEvent.isTrusted) {
          return;
        }

        let browser = aEvent.originalTarget;
        if (browser != this.browser) {
          return;
        }

        this.playingAudio = aEvent.type === "DOMAudioPlaybackStarted";

        GlobalEventDispatcher.sendRequest({
          type: "Tab:AudioPlayingChange",
          tabID: this.id,
          isAudioPlaying: this.playingAudio
        });
        return;
      }

      case "DOMWindowClose": {
        if (!aEvent.isTrusted)
          return;

        // Find the relevant tab, and close it from Java
        if (this.browser.contentWindow == aEvent.target) {
          aEvent.preventDefault();

          GlobalEventDispatcher.sendRequest({
            type: "Tab:Close",
            tabID: this.id
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

      case "DOMAutoComplete":
      case "blur": {
        LoginManagerContent.onUsernameInput(aEvent);
        break;
      }

      case "PluginBindingAttached": {
        PluginHelper.handlePluginBindingAttached(this, aEvent);
        break;
      }

      case "VideoBindingAttached": {
        CastingApps.handleVideoBindingAttached(this, aEvent);
        break;
      }

      case "VideoBindingCast": {
        CastingApps.handleVideoBindingCast(this, aEvent);
        break;
      }

      case "MozApplicationManifest": {
        OfflineApps.offlineAppRequested(aEvent.originalTarget.defaultView);
        break;
      }

      case "DOMWindowFocus": {
        GlobalEventDispatcher.sendRequest({
          type: "Tab:Select",
          tabID: this.id,
          foreground: true,
        });
        break;
      }

      case "pageshow": {
        LoginManagerContent.onPageShow(aEvent, this.browser.contentWindow);

        // The rest of this only handles pageshow for the top-level document.
        if (aEvent.originalTarget.defaultView != this.browser.contentWindow)
          return;

        let target = aEvent.originalTarget;
        let docURI = target.documentURI;
        if (!docURI.startsWith("about:neterror") && !this.isSearch) {
          // If this wasn't an error page and the user isn't search, don't retain the typed entry
          this.userRequested = "";
        }

        GlobalEventDispatcher.sendRequest({
          type: "Content:PageShow",
          tabID: this.id,
          userRequested: this.userRequested,
          fromCache: Tabs.useCache
        });

        this.isSearch = false;

        if (!aEvent.persisted && Services.prefs.getBoolPref("browser.ui.linkify.phone")) {
          if (!this._linkifier)
            this._linkifier = new Linkifier();
          this._linkifier.linkifyNumbers(this.browser.contentWindow.document);
        }

        // Update page actions for helper apps.
        let uri = this.browser.currentURI;
        if (BrowserApp.selectedTab == this) {
          if (ExternalApps.shouldCheckUri(uri)) {
            ExternalApps.updatePageAction(uri, this.browser.contentDocument);
          } else {
            ExternalApps.clearPageAction();
          }
        }
      }
    }
  },

  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    let contentWin = aWebProgress.DOMWindow;
    if (contentWin != contentWin.top)
        return;

    // Filter optimization: Only really send NETWORK state changes to Java listener
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
      if (AppConstants.MOZ_ENABLE_PROFILER_SPS && AppConstants.NIGHTLY_BUILD) {
        if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
          Profiler.AddMarker("Load start: " + aRequest.QueryInterface(Ci.nsIChannel).originalURI.spec);
        } else if ((aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) && !aWebProgress.isLoadingDocument) {
          Profiler.AddMarker("Load stop: " + aRequest.QueryInterface(Ci.nsIChannel).originalURI.spec);
        }
      }

      if ((aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) && aWebProgress.isLoadingDocument) {
        // We may receive a document stop event while a document is still loading
        // (such as when doing URI fixup). Don't notify Java UI in these cases.
        return;
      }

      // Clear page-specific opensearch engines and feeds for a new request.
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_START && aRequest && aWebProgress.isTopLevel) {
        this.browser.engines = null;
        this.browser.feeds = null;
      }

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
      } catch (e) {
        // If the request does not handle the nsIHttpChannel interface, use nsIRequest's success
        // status. Used for local files. See bug 948849.
        success = aRequest.status == 0;
      }

      // Check to see if we restoring the content from a previous presentation (session)
      // since there should be no real network activity
      let restoring = (aStateFlags & Ci.nsIWebProgressListener.STATE_RESTORING) > 0;

      let message = {
        type: "Content:StateChange",
        tabID: this.id,
        uri: truncate(uri, MAX_URI_LENGTH),
        state: aStateFlags,
        restoring: restoring,
        success: success
      };
      GlobalEventDispatcher.sendRequest(message);
    }
  },

  onLocationChange: function(aWebProgress, aRequest, aLocationURI, aFlags) {
    let contentWin = aWebProgress.DOMWindow;
    let webNav = contentWin.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebNavigation);

    // Browser webapps may load content inside iframes that can not reach across the app/frame boundary
    // i.e. even though the page is loaded in an iframe window.top != webapp
    // Make cure this window is a top level tab before moving on.
    if (BrowserApp.getBrowserForWindow(contentWin) == null) {
      // We still need to update the back/forward button state, though.
      let message = {
        type: "Content:SubframeNavigation",
        tabID: this.id,
        canGoBack: webNav.canGoBack,
        canGoForward: webNav.canGoForward,
      };

      GlobalEventDispatcher.sendRequest(message);
      return;
    }

    this._hostChanged = true;

    let fixedURI = aLocationURI;
    try {
      fixedURI = URIFixup.createExposableURI(aLocationURI);
    } catch (ex) { }

    // In restricted profiles, we refuse to let you open various urls.
    if (!ParentalControls.isAllowed(ParentalControls.BROWSE, fixedURI)) {
      aRequest.cancel(Cr.NS_BINDING_ABORTED);

      this.browser.docShell.displayLoadError(Cr.NS_ERROR_UNKNOWN_PROTOCOL, fixedURI, null);
    }

    let contentType = contentWin.document.contentType;

    // If fixedURI matches browser.lastURI, we assume this isn't a real location
    // change but rather a spurious addition like a wyciwyg URI prefix. See Bug 747883.
    // Note that we have to ensure fixedURI is not the same as aLocationURI so we
    // don't false-positive page reloads as spurious additions.
    let sameDocument = (aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) != 0 ||
                       ((this.browser.lastURI != null) && fixedURI.equals(this.browser.lastURI) && !fixedURI.equals(aLocationURI));
    this.browser.lastURI = fixedURI;

    // Let the reader logic know about same document changes because we won't get a DOMContentLoaded
    // or pageshow event, but we'll still want to update the reader view button to account for this change.
    // This mirrors the desktop logic in TabsProgressListener.
    if (aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
      this.browser.messageManager.sendAsyncMessage("Reader:PushState", {isArticle: this.browser.isArticle});
    }

    // Reset state of click-to-play plugin notifications.
    clearTimeout(this.pluginDoorhangerTimeout);
    this.pluginDoorhangerTimeout = null;
    this.shouldShowPluginDoorhanger = true;
    this.clickToPlayPluginsActivated = false;

    let baseDomain = "";
    // For recognized scheme, get base domain from host.
    let principalURI = contentWin.document.nodePrincipal.URI;
    if (principalURI && ["http", "https", "ftp"].includes(principalURI.scheme) && principalURI.host) {
      try {
        baseDomain = Services.eTLD.getBaseDomainFromHost(principalURI.host);
        if (!principalURI.host.endsWith(baseDomain)) {
          // getBaseDomainFromHost converts its resultant to ACE.
          let IDNService = Cc["@mozilla.org/network/idn-service;1"].getService(Ci.nsIIDNService);
          baseDomain = IDNService.convertACEtoUTF8(baseDomain);
        }
      } catch (e) {}
    }

    // If we are navigating to a new location with a different host,
    // clear any URL origin that might have been pinned to this tab.
    let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
    let appOrigin = ss.getTabValue(this, "appOrigin");
    if (appOrigin) {
      let originHost = "";
      try {
        originHost = Services.io.newURI(appOrigin).host;
      } catch (e if (e.result == Cr.NS_ERROR_FAILURE)) {
        // NS_ERROR_FAILURE can be thrown by nsIURI.host if the URI scheme does not possess a host - in this case
        // we just act as if we have an empty host.
      }
      if (originHost != aLocationURI.host) {
        // Note: going 'back' will not make this tab pinned again
        ss.deleteTabValue(this, "appOrigin");
      }
    }

    // Update the page actions URI for helper apps.
    if (BrowserApp.selectedTab == this) {
      ExternalApps.updatePageActionUri(fixedURI);
    }

    // Strip reader mode URI and also make it exposable if needed
    fixedURI = this._stripAboutReaderURL(fixedURI);

    let message = {
      type: "Content:LocationChange",
      tabID: this.id,
      uri: truncate(fixedURI.spec, MAX_URI_LENGTH),
      userRequested: this.userRequested || "",
      baseDomain: baseDomain,
      contentType: (contentType ? contentType : ""),
      sameDocument: sameDocument,

      canGoBack: webNav.canGoBack,
      canGoForward: webNav.canGoForward,
    };

    GlobalEventDispatcher.sendRequest(message);

    BrowserEventHandler.closeZoomedView();

    notifyManifestStatus(this.browser);

    if (!sameDocument) {
      // XXX This code assumes that this is the earliest hook we have at which
      // browser.contentDocument is changed to the new document we're loading
      this.contentDocumentIsDisplayed = false;
      this.hasTouchListener = false;
      Services.obs.notifyObservers(this.browser, "Session:NotifyLocationChange", null);
    }
  },

  _stripAboutReaderURL: function (originalURI) {
    try {
      let strippedURL = ReaderMode.getOriginalUrl(originalURI.spec);
      if(strippedURL){
        // Continue to create exposable uri if original uri is stripped.
        originalURI = URIFixup.createExposableURI(Services.io.newURI(strippedURL));
      }
    } catch (ex) { }
    return originalURI;
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
      type: "Content:SecurityChange",
      tabID: this.id,
      identity: identity
    };

    GlobalEventDispatcher.sendRequest(message);
  },

  OnHistoryNewEntry: function(newURI, oldIndex) {
    Services.obs.notifyObservers(this.browser, "Content:HistoryChange", null);
  },

  OnHistoryGoBack: function(backURI) {
    Services.obs.notifyObservers(this.browser, "Content:HistoryChange", null);
    return true;
  },

  OnHistoryGoForward: function(forwardURI) {
    Services.obs.notifyObservers(this.browser, "Content:HistoryChange", null);
    return true;
  },

  OnHistoryReload: function(reloadURI, reloadFlags) {
    Services.obs.notifyObservers(this.browser, "Content:HistoryChange", null);
    return true;
  },

  OnHistoryGotoIndex: function(index, gotoURI) {
    Services.obs.notifyObservers(this.browser, "Content:HistoryChange", null);
    return true;
  },

  OnHistoryPurge: function(numEntries) {
    Services.obs.notifyObservers(this.browser, "Content:HistoryChange", null);
    return true;
  },

  OnHistoryReplaceEntry: function(index) {
    Services.obs.notifyObservers(this.browser, "Content:HistoryChange", null);
  },

  ShouldNotifyMediaPlaybackChange: function(activeState) {
    // If the media is active, we would check it's duration, because we don't
    // want to show the media control interface for the short sound which
    // duration is smaller than the threshold. The basic unit is second.
    // Note : the streaming format's duration is infinite.
    if (activeState === "inactive") {
      return true;
    }

    const mediaDurationThreshold = 1.0;

    let audioElements = this.browser.contentDocument.getElementsByTagName("audio");
    for (let audio of audioElements) {
      if (!audio.paused && audio.duration < mediaDurationThreshold) {
        return false;
      }
    }

    let videoElements = this.browser.contentDocument.getElementsByTagName("video");
    for (let video of videoElements) {
      if (!video.paused && video.duration < mediaDurationThreshold) {
        return false;
      }
    }

    return true;
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "before-first-paint":
        // Is it on the top level?
        let contentDocument = aSubject;
        if (contentDocument == this.browser.contentDocument) {
          if (BrowserApp.selectedTab == this) {
            BrowserApp.contentDocumentChanged();
          }
          this.contentDocumentIsDisplayed = true;

          if (contentDocument instanceof Ci.nsIImageDocument) {
            contentDocument.shrinkToFit();
          }
        }
        break;

      case "media-playback":
      case "media-playback-resumed":
        if (!aSubject) {
          return;
        }

        let winId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
        if (this.browser.outerWindowID != winId) {
          return;
        }

        if (!this.ShouldNotifyMediaPlaybackChange(aData)) {
          return;
        }

        let status;
        if (aTopic == "media-playback") {
          status = (aData === "inactive") ? "end" : "start";
        } else if (aTopic == "media-playback-resumed") {
          status = "resume";
        }

        GlobalEventDispatcher.sendRequest({
          type: "Tab:MediaPlaybackChange",
          tabID: this.id,
          status: status
        });
        break;
    }
  },

  // nsIBrowserTab
  get window() {
    if (!this.browser)
      return null;
    return this.browser.contentWindow;
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIWebProgressListener,
    Ci.nsISHistoryListener,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
    Ci.nsIBrowserTab
  ])
};

var BrowserEventHandler = {
  init: function init() {
    this._clickInZoomedView = false;
    Services.obs.addObserver(this, "Gesture:SingleTap");
    Services.obs.addObserver(this, "Gesture:ClickInZoomedView");

    BrowserApp.deck.addEventListener("touchend", this, true);

    BrowserApp.deck.addEventListener("DOMUpdatePageReport", PopupBlockerObserver.onUpdatePageReport);
    BrowserApp.deck.addEventListener("MozMouseHittest", this, true);
    BrowserApp.deck.addEventListener("OpenMediaWithExternalApp", this, true);

    InitLater(() => BrowserApp.deck.addEventListener("click", InputWidgetHelper, true));
    InitLater(() => BrowserApp.deck.addEventListener("click", SelectHelper, true));

    // ReaderViews support backPress listeners.
    WindowEventDispatcher.registerListener((event, data, callback) => {
      callback.onSuccess(Reader.onBackPress(BrowserApp.selectedTab.id));
    }, "Browser:OnBackPressed");
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case 'touchend':
        if (this._inCluster) {
          aEvent.preventDefault();
        }
        break;
      case 'MozMouseHittest':
        this._handleRetargetedTouchStart(aEvent);
        break;
      case 'OpenMediaWithExternalApp': {
        let mediaSrc = aEvent.target.currentSrc || aEvent.target.src;
        let uuid = uuidgen.generateUUID().toString();
        GlobalEventDispatcher.sendRequest({
          type: "Video:Play",
          uri: mediaSrc,
          uuid: uuid
        });
        break;
      }
    }
  },

  _handleRetargetedTouchStart: function(aEvent) {
    // we should only get this called just after a new touchstart with a single
    // touch point.
    if (!BrowserApp.isBrowserContentDocumentDisplayed() || aEvent.defaultPrevented) {
      return;
    }

    let target = aEvent.target;
    if (!target) {
      return;
    }

    this._inCluster = aEvent.hitCluster;
    if (this._inCluster) {
      return;  // No highlight for a cluster of links
    }

    let uri = this._getLinkURI(target);
    if (uri) {
      try {
        Services.io.QueryInterface(Ci.nsISpeculativeConnect).speculativeConnect2(uri,
                                                                                 target.ownerDocument.nodePrincipal,
                                                                                 null);
      } catch (e) {}
    }
    this._doTapHighlight(target);
  },

  _getLinkURI: function(aElement) {
    if (aElement.nodeType == Ci.nsIDOMNode.ELEMENT_NODE &&
        ((aElement instanceof Ci.nsIDOMHTMLAnchorElement && aElement.href) ||
        (aElement instanceof Ci.nsIDOMHTMLAreaElement && aElement.href))) {
      try {
        return Services.io.newURI(aElement.href);
      } catch (e) {}
    }
    return null;
  },

  observe: function(aSubject, aTopic, aData) {
    // the remaining events are all dependent on the browser content document being the
    // same as the browser displayed document. if they are not the same, we should ignore
    // the event.
    if (BrowserApp.isBrowserContentDocumentDisplayed()) {
      this.handleUserEvent(aTopic, aData);
    }
  },

  handleUserEvent: function(aTopic, aData) {
    switch (aTopic) {

      case "Gesture:ClickInZoomedView":
        this._clickInZoomedView = true;
        break;

      case "Gesture:SingleTap": {
        let focusedElement = BrowserApp.getFocusedInput(BrowserApp.selectedBrowser);
        let data = JSON.parse(aData);
        let {x, y} = data;

        if (this._inCluster && this._clickInZoomedView != true) {
          // If there is a focused element, the display of the zoomed view won't remove the focus.
          // In this case, the form assistant linked to the focused element will never be closed.
          // To avoid this situation, the focus is moved and the form assistant is closed.
          if (focusedElement) {
            try {
              Services.focus.moveFocus(BrowserApp.selectedBrowser.contentWindow, null, Services.focus.MOVEFOCUS_ROOT, 0);
            } catch(e) {
              Cu.reportError(e);
            }
            WindowEventDispatcher.sendRequest({ type: "FormAssist:Hide" });
          }
          this._clusterClicked(x, y);
        } else {
          if (this._clickInZoomedView != true) {
            this.closeZoomedView(/* animate */ true);
          }
        }
        this._clickInZoomedView = false;
        this._cancelTapHighlight();
        break;
      }

      default:
        dump('BrowserEventHandler.handleUserEvent: unexpected topic "' + aTopic + '"');
        break;
    }
  },

  closeZoomedView: function(aAnimate) {
    WindowEventDispatcher.sendRequest({
      type: "Gesture:CloseZoomedView",
      animate: !!aAnimate,
    });
  },

  _clusterClicked: function(aX, aY) {
    WindowEventDispatcher.sendRequest({
      type: "Gesture:ClusteredLinksClicked",
      clickPosition: {
        x: aX,
        y: aY
      }
    });
  },

  _highlightElement: null,

  _doTapHighlight: function _doTapHighlight(aElement) {
    this._highlightElement = aElement;
  },

  _cancelTapHighlight: function _cancelTapHighlight() {
    if (!this._highlightElement)
      return;

    this._highlightElement = null;
  }
};

const ElementTouchHelper = {
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
    for (let frame = aElement.ownerGlobal; frame.frameElement && frame != content; frame = frame.parent) {
      // adjust client coordinates' origin to be top left of iframe viewport
      let rect = frame.frameElement.getBoundingClientRect();
      let left = frame.getComputedStyle(frame.frameElement).borderLeftWidth;
      let top = frame.getComputedStyle(frame.frameElement).borderTopWidth;
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
        if (errorDoc.documentURI.startsWith("about:certerror?e=nssBadCert")) {
          let perm = errorDoc.getElementById("permanentExceptionButton");
          let temp = errorDoc.getElementById("temporaryExceptionButton");
          if (target == temp || target == perm) {
            // Handle setting an cert exception and reloading the page
            try {
              // Add a new SSL exception for this URL
              let uri = Services.io.newURI(errorDoc.location.href);
              let sslExceptions = new SSLExceptions();

              if (target == perm)
                sslExceptions.addPermanentException(uri, errorDoc.defaultView);
              else
                sslExceptions.addTemporaryException(uri, errorDoc.defaultView);
            } catch (e) {
              dump("Failed to set cert exception: " + e + "\n");
            }
            errorDoc.location.reload();
          } else if (target == errorDoc.getElementById("getMeOutOfHereButton")) {
            errorDoc.location = "about:home";
          }
        } else if (errorDoc.documentURI.startsWith("about:blocked")) {
          // The event came from a button on a malware/phishing block page
          // First check whether it's malware, phishing or unwanted, so that we
          // can use the right strings/links
          let bucketName = "";
          let sendTelemetry = false;
          if (errorDoc.documentURI.includes("e=malwareBlocked")) {
            sendTelemetry = true;
            bucketName = "WARNING_MALWARE_PAGE_";
          } else if (errorDoc.documentURI.includes("e=deceptiveBlocked")) {
            sendTelemetry = true;
            bucketName = "WARNING_PHISHING_PAGE_";
          } else if (errorDoc.documentURI.includes("e=unwantedBlocked")) {
            sendTelemetry = true;
            bucketName = "WARNING_UNWANTED_PAGE_";
          }
          let nsISecTel = Ci.nsISecurityUITelemetry;
          let isIframe = (errorDoc.defaultView.parent === errorDoc.defaultView);
          bucketName += isIframe ? "TOP_" : "FRAME_";

          let formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);

          if (target == errorDoc.getElementById("getMeOutButton")) {
            if (sendTelemetry) {
              Telemetry.addData("SECURITY_UI", nsISecTel[bucketName + "GET_ME_OUT_OF_HERE"]);
            }
            errorDoc.location = "about:home";
          } else if (target == errorDoc.getElementById("reportButton")) {
            // We log even if malware/phishing info URL couldn't be found:
            // the measurement is for how many users clicked the WHY BLOCKED button
            if (sendTelemetry) {
              Telemetry.addData("SECURITY_UI", nsISecTel[bucketName + "WHY_BLOCKED"]);
            }

            // This is the "Why is this site blocked" button. We redirect
            // to the generic page describing phishing/malware protection.
            let url = Services.urlFormatter.formatURLPref("app.support.baseURL");
            BrowserApp.selectedBrowser.loadURI(url + "phishing-malware");
          } else if (target == errorDoc.getElementById("ignoreWarningButton") &&
                     Services.prefs.getBoolPref("browser.safebrowsing.allowOverride")) {
            if (sendTelemetry) {
              Telemetry.addData("SECURITY_UI", nsISecTel[bucketName + "IGNORE_WARNING"]);
            }

            // Allow users to override and continue through to the site,
            let webNav = BrowserApp.selectedBrowser.docShell.QueryInterface(Ci.nsIWebNavigation);
            let location = BrowserApp.selectedBrowser.contentWindow.location;
            webNav.loadURI(location, Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CLASSIFIER, null, null, null);

            // ....but add a notify bar as a reminder, so that they don't lose
            // track after, e.g., tab switching.
            NativeWindow.doorhanger.show(Strings.browser.GetStringFromName("safeBrowsingDoorhanger"), "safebrowsing-warning", [], BrowserApp.selectedTab.id);
          }
        }
        break;
      }
    }
  }
};

var FormAssistant = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFormSubmitObserver]),

  // Used to keep track of the element that corresponds to the current
  // autocomplete suggestions
  _currentInputElement: null,

  // The value of the currently focused input
  _currentInputValue: null,

  // Whether we're in the middle of an autocomplete
  _doingAutocomplete: false,

  // Keep track of whether or not an invalid form has been submitted
  _invalidSubmit: false,

  init: function() {
    WindowEventDispatcher.registerListener(this, [
      "FormAssist:AutoComplete",
      "FormAssist:Hidden",
      "FormAssist:Remove",
    ]);

    Services.obs.addObserver(this, "invalidformsubmit");
    Services.obs.addObserver(this, "PanZoom:StateChange");

    // We need to use a capturing listener for focus events
    BrowserApp.deck.addEventListener("focus", this, true);
    BrowserApp.deck.addEventListener("blur", this, true);
    BrowserApp.deck.addEventListener("click", this, true);
    BrowserApp.deck.addEventListener("input", this);
    BrowserApp.deck.addEventListener("pageshow", this);
  },

  onEvent: function(event, message, callback) {
    switch (event) {
      case "FormAssist:AutoComplete":
        if (!this._currentInputElement)
          break;

        let editableElement = this._currentInputElement.QueryInterface(Ci.nsIDOMNSEditableElement);

        this._doingAutocomplete = true;

        // If we have an active composition string, commit it before sending
        // the autocomplete event with the text that will replace it.
        try {
          if (editableElement.editor.composing)
            editableElement.editor.forceCompositionEnd();
        } catch (e) {}

        editableElement.setUserInput(message.value);
        this._currentInputValue = message.value;

        let event = this._currentInputElement.ownerDocument.createEvent("Events");
        event.initEvent("DOMAutoComplete", true, true);
        this._currentInputElement.dispatchEvent(event);

        this._doingAutocomplete = false;

        break;

      case "FormAssist:Hidden":
        this._currentInputElement = null;
        break;

      case "FormAssist:Remove":
        if (!this._currentInputElement) {
          break;
        }

        FormHistory.update({
          op: "remove",
          fieldname: this._currentInputElement.name,
          value: message.value,
        });
        break;
    }
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "PanZoom:StateChange":
        // If the user is just touching the screen and we haven't entered a pan or zoom state yet do nothing
        if (aData == "TOUCHING" || aData == "WAITING_LISTENERS")
          break;
        if (aData == "NOTHING") {
          // only look for input elements, not contentEditable or multiline text areas
          let focused = BrowserApp.getFocusedInput(BrowserApp.selectedBrowser, true);
          if (!focused)
            break;

          if (this._showValidationMessage(focused))
            break;
          let checkResultsClick = hasResults => {
            if (!hasResults) {
              this._hideFormAssistPopup();
            }
          };
          this._showAutoCompleteSuggestions(focused, checkResultsClick);
        } else {
          // temporarily hide the form assist popup while we're panning or zooming the page
          this._hideFormAssistPopup();
        }
        break;
    }
  },

  notifyInvalidSubmit: function notifyInvalidSubmit(aFormElement, aInvalidElements) {
    if (!aInvalidElements.length)
      return;

    // Ignore this notificaiton if the current tab doesn't contain the invalid element
    let currentElement = aInvalidElements.queryElementAt(0, Ci.nsISupports);
    if (BrowserApp.selectedBrowser.contentDocument !=
        currentElement.ownerGlobal.top.document)
      return;

    this._invalidSubmit = true;

    // Our focus listener will show the element's validation message
    currentElement.focus();
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "focus": {
        let currentElement = aEvent.target;

        // Only show a validation message on focus.
        this._showValidationMessage(currentElement);
        break;
      }

      case "blur": {
        this._currentInputValue = null;
        break;
      }

      case "click": {
        let currentElement = aEvent.target;

        // Prioritize a form validation message over autocomplete suggestions
        // when the element is first focused (a form validation message will
        // only be available if an invalid form was submitted)
        if (this._showValidationMessage(currentElement))
          break;

        let checkResultsClick = hasResults => {
          if (!hasResults) {
            this._hideFormAssistPopup();
          }
        };

        this._showAutoCompleteSuggestions(currentElement, checkResultsClick);
        break;
      }

      case "input": {
        let currentElement = aEvent.target;

        // If this element isn't focused, we're already in middle of an
        // autocomplete, or its value hasn't changed, don't show the
        // autocomplete popup.
        if (currentElement !== BrowserApp.getFocusedInput(BrowserApp.selectedBrowser) ||
            this._doingAutocomplete ||
            currentElement.value === this._currentInputValue) {
          break;
        }

        this._currentInputValue = currentElement.value;

        // Since we can only show one popup at a time, prioritze autocomplete
        // suggestions over a form validation message
        let checkResultsInput = hasResults => {
          if (hasResults)
            return;

          if (this._showValidationMessage(currentElement))
            return;

          // If we're not showing autocomplete suggestions, hide the form assist popup
          this._hideFormAssistPopup();
        };

        this._showAutoCompleteSuggestions(currentElement, checkResultsInput);
        break;
      }

      // Reset invalid submit state on each pageshow
      case "pageshow": {
        if (!this._invalidSubmit)
          return;

        let selectedBrowser = BrowserApp.selectedBrowser;
        if (selectedBrowser) {
          let selectedDocument = selectedBrowser.contentDocument;
          let target = aEvent.originalTarget;
          if (target == selectedDocument || target.ownerDocument == selectedDocument)
            this._invalidSubmit = false;
        }
        break;
      }
    }
  },

  // We only want to show autocomplete suggestions for certain elements
  _isAutoComplete: function _isAutoComplete(aElement) {
    if (!(aElement instanceof HTMLInputElement) || aElement.readOnly || aElement.disabled ||
        (aElement.getAttribute("type") == "password") ||
        (aElement.hasAttribute("autocomplete") &&
         aElement.getAttribute("autocomplete").toLowerCase() == "off"))
      return false;

    return true;
  },

  // Retrieves autocomplete suggestions for an element from the form autocomplete service.
  // aCallback(array_of_suggestions) is called when results are available.
  _getAutoCompleteSuggestions: function _getAutoCompleteSuggestions(aSearchString, aElement, aCallback) {
    // Cache the form autocomplete service for future use
    if (!this._formAutoCompleteService) {
      this._formAutoCompleteService = Cc["@mozilla.org/satchel/form-autocomplete;1"]
          .getService(Ci.nsIFormAutoComplete);
    }

    let resultsAvailable = function (results) {
      let suggestions = [];
      for (let i = 0; i < results.matchCount; i++) {
        let value = results.getValueAt(i);

        // Do not show the value if it is the current one in the input field
        if (value == aSearchString)
          continue;

        // Supply a label and value, since they can differ for datalist suggestions
        suggestions.push({ label: value, value: value });
      }
      aCallback(suggestions);
    };

    this._formAutoCompleteService.autoCompleteSearchAsync(aElement.name || aElement.id,
                                                          aSearchString, aElement, null,
                                                          null, resultsAvailable);
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

      if (filter && !(label.toLowerCase().includes(lowerFieldValue)) )
        continue;
      suggestions.push({ label: label, value: item.value });
    }

    return suggestions;
  },

  // Retrieves autocomplete suggestions for an element from the form autocomplete service
  // and sends the suggestions to the Java UI, along with element position data. As
  // autocomplete queries are asynchronous, calls aCallback when done with a true
  // argument if results were found and false if no results were found.
  _showAutoCompleteSuggestions: function _showAutoCompleteSuggestions(aElement, aCallback) {
    if (!this._isAutoComplete(aElement)) {
      aCallback(false);
      return;
    }
    if (this._isDisabledElement(aElement)) {
      aCallback(false);
      return;
    }

    let isEmpty = (aElement.value.length === 0);

    let resultsAvailable = autoCompleteSuggestions => {
      // On desktop, we show datalist suggestions below autocomplete suggestions,
      // without duplicates removed.
      let listSuggestions = this._getListSuggestions(aElement);
      let suggestions = autoCompleteSuggestions.concat(listSuggestions);

      // Return false if there are no suggestions to show
      if (!suggestions.length) {
        aCallback(false);
        return;
      }

      WindowEventDispatcher.sendRequest({
        type:  "FormAssist:AutoCompleteResult",
        suggestions: suggestions,
        rect: ElementTouchHelper.getBoundingContentRect(aElement),
        isEmpty: isEmpty,
      });

      // Keep track of input element so we can fill it in if the user
      // selects an autocomplete suggestion
      this._currentInputElement = aElement;
      aCallback(true);
    };

    this._getAutoCompleteSuggestions(aElement.value, aElement, resultsAvailable);
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

    WindowEventDispatcher.sendRequest({
      type: "FormAssist:ValidationMessage",
      validationMessage: aElement.validationMessage,
      rect: ElementTouchHelper.getBoundingContentRect(aElement)
    });

    return true;
  },

  _hideFormAssistPopup: function _hideFormAssistPopup() {
    WindowEventDispatcher.sendRequest({ type: "FormAssist:Hide" });
  },

  _isDisabledElement : function(aElement) {
    let currentElement = aElement;
    while (currentElement) {
      if(currentElement.disabled)
	return true;

      currentElement = currentElement.parentElement;
    }
    return false;
  }
};

var XPInstallObserver = {
  init: function() {
    Services.obs.addObserver(this, "addon-install-origin-blocked");
    Services.obs.addObserver(this, "addon-install-disabled");
    Services.obs.addObserver(this, "addon-install-blocked");
    Services.obs.addObserver(this, "addon-install-started");
    Services.obs.addObserver(this, "xpi-signature-changed");
    Services.obs.addObserver(this, "browser-delayed-startup-finished");

    AddonManager.addInstallListener(this);
  },

  observe: function(aSubject, aTopic, aData) {
    let installInfo, tab, host;
    if (aSubject && aSubject.wrappedJSObject) {
      installInfo = aSubject.wrappedJSObject;
      tab = BrowserApp.getTabForBrowser(installInfo.browser);
      if (installInfo.originatingURI) {
        host = installInfo.originatingURI.host;
      }
    }

    let strings = Strings.browser;
    let brandShortName = Strings.brand.GetStringFromName("brandShortName");

    switch (aTopic) {
      case "addon-install-started":
        Snackbars.show(strings.GetStringFromName("alertAddonsDownloading"), Snackbars.LENGTH_LONG);
        break;
      case "addon-install-disabled": {
        if (!tab)
          return;

        let enabled = Services.prefs.getBoolPref("xpinstall.enabled", true);

        let buttons, message, callback;
        if (!enabled) {
          message = strings.GetStringFromName("xpinstallDisabledMessageLocked");
          buttons = [strings.GetStringFromName("unsignedAddonsDisabled.dismiss")];
          callback: (data) => {};
        } else {
          message = strings.formatStringFromName("xpinstallDisabledMessage2", [brandShortName, host], 2);
          buttons = [
              strings.GetStringFromName("xpinstallDisabledButton"),
              strings.GetStringFromName("unsignedAddonsDisabled.dismiss")
          ];
          callback: (data) => {
            if (data.button === 1) {
              Services.prefs.setBoolPref("xpinstall.enabled", true)
            }
          };
        }

        new Prompt({
          title: Strings.browser.GetStringFromName("addonError.titleError"),
          message: message,
          buttons: buttons
        }).show(callback);
        break;
      }
      case "addon-install-blocked": {
        if (!tab)
          return;

        let message;
        if (host) {
          // We have a host which asked for the install.
          message = strings.formatStringFromName("xpinstallPromptWarning2", [brandShortName, host], 2);
        } else {
          // Without a host we address the add-on as the initiator of the install.
          let addon = null;
          if (installInfo.installs.length > 0) {
            addon = installInfo.installs[0].name;
          }
          if (addon) {
            // We have an addon name, show the regular message.
            message = strings.formatStringFromName("xpinstallPromptWarningLocal", [brandShortName, addon], 2);
          } else {
            // We don't have an addon name, show an alternative message.
            message = strings.formatStringFromName("xpinstallPromptWarningDirect", [brandShortName], 1);
          }
        }

        let buttons = [
            strings.GetStringFromName("xpinstallPromptAllowButton"),
            strings.GetStringFromName("unsignedAddonsDisabled.dismiss")
        ];
        new Prompt({
          title: Strings.browser.GetStringFromName("addonError.titleBlocked"),
          message: message,
          buttons: buttons
        }).show((data) => {
          if (data.button === 0) {
            // Kick off the install
            installInfo.install();
          }
        });
        break;
      }
      case "addon-install-origin-blocked": {
        if (!tab)
          return;

        new Prompt({
          title: Strings.browser.GetStringFromName("addonError.titleBlocked"),
          message: strings.formatStringFromName("xpinstallPromptWarningDirect", [brandShortName], 1),
          buttons: [strings.GetStringFromName("unsignedAddonsDisabled.dismiss")]
        }).show((data) => {});
        break;
      }
      case "xpi-signature-changed": {
        if (JSON.parse(aData).disabled.length) {
          this._notifyUnsignedAddonsDisabled();
        }
        break;
      }
      case "browser-delayed-startup-finished": {
        let disabledAddons = AddonManager.getStartupChanges(AddonManager.STARTUP_CHANGE_DISABLED);
        for (let id of disabledAddons) {
          if (AddonManager.getAddonByID(id).signedState <= AddonManager.SIGNEDSTATE_MISSING) {
            this._notifyUnsignedAddonsDisabled();
            break;
          }
        }
        break;
      }
    }
  },

  _notifyUnsignedAddonsDisabled: function() {
    new Prompt({
      window: window,
      title: Strings.browser.GetStringFromName("unsignedAddonsDisabled.title"),
      message: Strings.browser.GetStringFromName("unsignedAddonsDisabled.message"),
      buttons: [
        Strings.browser.GetStringFromName("unsignedAddonsDisabled.viewAddons"),
        Strings.browser.GetStringFromName("unsignedAddonsDisabled.dismiss")
      ]
    }).show((data) => {
      if (data.button === 0) {
        // TODO: Open about:addons to show only unsigned add-ons?
        BrowserApp.selectOrAddTab("about:addons", { parentId: BrowserApp.selectedTab.id });
      }
    });
  },

  onInstallEnded: function(aInstall, aAddon) {
    // Don't create a notification for distribution add-ons.
    if (Distribution.pendingAddonInstalls.has(aInstall)) {
      Distribution.pendingAddonInstalls.delete(aInstall);
      return;
    }

    let needsRestart = false;
    if (aInstall.existingAddon && (aInstall.existingAddon.pendingOperations & AddonManager.PENDING_UPGRADE))
      needsRestart = true;
    else if (aAddon.pendingOperations & AddonManager.PENDING_INSTALL)
      needsRestart = true;

    if (needsRestart) {
      this.showRestartPrompt();
    } else {
      // Display completion message for new installs or updates not done Automatically
      if (!aInstall.existingAddon || !AddonManager.shouldAutoUpdate(aInstall.existingAddon)) {
        let message = Strings.browser.GetStringFromName("alertAddonsInstalledNoRestart.message");
        Snackbars.show(message, Snackbars.LENGTH_LONG, {
          action: {
            label: Strings.browser.GetStringFromName("alertAddonsInstalledNoRestart.action2"),
            callback: () => {
              UITelemetry.addEvent("show.1", "toast", null, "addons");
              BrowserApp.selectOrAddTab("about:addons", { parentId: BrowserApp.selectedTab.id });
            },
          }
        });
      }
    }
  },

  onInstallFailed: function(aInstall) {
    this._showErrorMessage(aInstall);
  },

  onDownloadProgress: function(aInstall) {},

  onDownloadFailed: function(aInstall) {
    this._showErrorMessage(aInstall);
  },

  onDownloadCancelled: function(aInstall) {},

  _showErrorMessage: function(aInstall) {
    // Don't create a notification for distribution add-ons.
    if (Distribution.pendingAddonInstalls.has(aInstall)) {
      Cu.reportError("Error installing distribution add-on: " + aInstall.addon.id);
      Distribution.pendingAddonInstalls.delete(aInstall);
      return;
    }

    let host = (aInstall.originatingURI instanceof Ci.nsIStandardURL) && aInstall.originatingURI.host;
    if (!host) {
      host = (aInstall.sourceURI instanceof Ci.nsIStandardURL) && aInstall.sourceURI.host;
    }

    let error = (host || aInstall.error == 0) ? "addonError" : "addonLocalError";
    if (aInstall.error < 0) {
      error += aInstall.error;
    } else if (aInstall.addon && aInstall.addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED) {
      error += "Blocklisted";
    } else {
      error += "Incompatible";
    }

    let msg = Strings.browser.GetStringFromName(error);
    // TODO: formatStringFromName
    msg = msg.replace("#1", aInstall.name);
    if (host) {
      msg = msg.replace("#2", host);
    }
    msg = msg.replace("#3", Strings.brand.GetStringFromName("brandShortName"));
    msg = msg.replace("#4", Services.appinfo.version);

    if (aInstall.error == AddonManager.ERROR_SIGNEDSTATE_REQUIRED) {
      new Prompt({
        window: window,
        title: Strings.browser.GetStringFromName("addonError.titleBlocked"),
        message: msg,
        buttons: [Strings.browser.GetStringFromName("addonError.learnMore")]
      }).show((data) => {
        if (data.button === 0) {
          let url = Services.urlFormatter.formatURLPref("app.support.baseURL") + "unsigned-addons";
          BrowserApp.addTab(url, { parentId: BrowserApp.selectedTab.id });
        }
      });
    } else {
      Services.prompt.alert(null, Strings.browser.GetStringFromName("addonError.titleError"), msg);
    }
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
          Services.obs.notifyObservers(null, "quit-application-proceeding", null);
          SharedPreferences.forApp().setBoolPref("browser.sessionstore.resume_session_once", true);
          let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
          appStartup.quit(Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit);
        }
      },
      positive: true
    }];

    let message = Strings.browser.GetStringFromName("notificationRestart.normal");
    NativeWindow.doorhanger.show(message, "addon-app-restart", buttons, BrowserApp.selectedTab.id, { persistence: -1 });
  },

  hideRestartPrompt: function() {
    NativeWindow.doorhanger.hide("addon-app-restart", BrowserApp.selectedTab.id);
  }
};

var ViewportHandler = {
  init: function init() {
    GlobalEventDispatcher.registerListener(this, "Window:Resize");
  },

  onEvent: function (event, data, callback) {
    if (event == "Window:Resize" && data) {
      let windowUtils = window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      windowUtils.setNextPaintSyncId(data.id);
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
        let popupCount = browser.pageReport.length;

        let strings = Strings.browser;
        let message = PluralForm.get(popupCount, strings.GetStringFromName("popup.message"))
                                .replace("#1", brandShortName)
                                .replace("#2", popupCount);

        let buttons = [
          {
            label: strings.GetStringFromName("popup.dontShow"),
            callback: function(aChecked) {
              if (aChecked)
                PopupBlockerObserver.allowPopupsForSite(false);
            }
          },
          {
            label: strings.GetStringFromName("popup.show"),
            callback: function(aChecked) {
              // Set permission before opening popup windows
              if (aChecked)
                PopupBlockerObserver.allowPopupsForSite(true);

              PopupBlockerObserver.showPopupsForSite();
            },
            positive: true
          }
        ];

        let options = { checkbox: Strings.browser.GetStringFromName("popup.dontAskAgain") };
        NativeWindow.doorhanger.show(message, "popup-blocked", buttons, null, options);
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
        let popupURIspec = pageReport[i].popupWindowURIspec;

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
        let isPrivate = PrivateBrowsingUtils.isBrowserPrivate(parent.browser);
        BrowserApp.addTab(popupURIspec, { parentId: parent.id, isPrivate: isPrivate });
      }
    }
  }
};


var IndexedDB = {
  _permissionsPrompt: "indexedDB-permissions-prompt",
  _permissionsResponse: "indexedDB-permissions-response",

  init: function IndexedDB_init() {
    Services.obs.addObserver(this, this._permissionsPrompt);
  },

  observe: function IndexedDB_observe(subject, topic, data) {
    if (topic != this._permissionsPrompt) {
      throw new Error("Unexpected topic!");
    }

    let requestor = subject.QueryInterface(Ci.nsIInterfaceRequestor);

    let browser = requestor.getInterface(Ci.nsIDOMNode);
    let tab = BrowserApp.getTabForBrowser(browser);
    if (!tab)
      return;

    let host = browser.currentURI.asciiHost;

    let strings = Strings.browser;

    let message, responseTopic;
    if (topic == this._permissionsPrompt) {
      message = strings.formatStringFromName("offlineApps.ask", [host], 1);
      responseTopic = this._permissionsResponse;
    }

    const firstTimeoutDuration = 300000; // 5 minutes

    let timeoutId;

    let notificationID = responseTopic + host;
    let observer = requestor.getInterface(Ci.nsIObserver);

    // This will be set to the result of PopupNotifications.show() below, or to
    // the result of PopupNotifications.getNotification() if this is a
    // quotaCancel notification.
    let notification;

    function timeoutNotification() {
      // Remove the notification.
      NativeWindow.doorhanger.hide(notificationID, tab.id);

      // Clear all of our timeout stuff. We may be called directly, not just
      // when the timeout actually elapses.
      clearTimeout(timeoutId);

      // And tell the page that the popup timed out.
      observer.observe(null, responseTopic, Ci.nsIPermissionManager.UNKNOWN_ACTION);
    }

    let buttons = [
    {
      label: strings.GetStringFromName("offlineApps.dontAllow2"),
      callback: function(aChecked) {
        clearTimeout(timeoutId);
        let action = aChecked ? Ci.nsIPermissionManager.DENY_ACTION : Ci.nsIPermissionManager.UNKNOWN_ACTION;
        observer.observe(null, responseTopic, action);
      }
    },
    {
      label: strings.GetStringFromName("offlineApps.allow"),
      callback: function() {
        clearTimeout(timeoutId);
        observer.observe(null, responseTopic, Ci.nsIPermissionManager.ALLOW_ACTION);
      },
      positive: true
    }];

    let options = { checkbox: Strings.browser.GetStringFromName("offlineApps.dontAskAgain") };
    NativeWindow.doorhanger.show(message, notificationID, buttons, tab.id, options);

    // Set the timeoutId after the popup has been created, and use the long
    // timeout value. If the user doesn't notice the popup after this amount of
    // time then it is most likely not visible and we want to alert the page.
    timeoutId = setTimeout(timeoutNotification, firstTimeoutDuration);
  }
};

var CharacterEncoding = {
  _charsets: [],

  init: function init() {
    GlobalEventDispatcher.registerListener(this, [
      "CharEncoding:Get",
      "CharEncoding:Set",
    ]);
    InitLater(() => this.sendState());
  },

  onEvent: function onEvent(event, data, callback) {
    switch (event) {
      case "CharEncoding:Get":
        this.getEncoding();
        break;
      case "CharEncoding:Set":
        this.setEncoding(data.encoding);
        break;
    }
  },

  sendState: function sendState() {
    let showCharEncoding = "false";
    try {
      showCharEncoding = Services.prefs.getComplexValue("browser.menu.showCharacterEncoding", Ci.nsIPrefLocalizedString).data;
    } catch (e) { /* Optional */ }

    GlobalEventDispatcher.sendRequest({
      type: "CharEncoding:State",
      visible: showCharEncoding
    });
  },

  getEncoding: function getEncoding() {
    function infoToCharset(info) {
      return { code: info.value, title: info.label };
    }

    if (!this._charsets.length) {
      let data = CharsetMenu.getData();

      // In the desktop UI, the pinned charsets are shown above the rest.
      let pinnedCharsets = data.pinnedCharsets.map(infoToCharset);
      let otherCharsets = data.otherCharsets.map(infoToCharset)

      this._charsets = pinnedCharsets.concat(otherCharsets);
    }

    // Look for the index of the selected charset. Default to -1 if the
    // doc charset isn't found in the list of available charsets.
    let docCharset = BrowserApp.selectedBrowser.contentDocument.characterSet;
    let selected = -1;
    let charsetCount = this._charsets.length;

    for (let i = 0; i < charsetCount; i++) {
      if (this._charsets[i].code === docCharset) {
        selected = i;
        break;
      }
    }

    GlobalEventDispatcher.sendRequest({
      type: "CharEncoding:Data",
      charsets: this._charsets,
      selected: selected
    });
  },

  setEncoding: function setEncoding(aEncoding) {
    let browser = BrowserApp.selectedBrowser;
    browser.docShell.gatherCharsetMenuTelemetry();
    browser.docShell.charset = aEncoding;
    browser.reload(Ci.nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
  }
};

var IdentityHandler = {
  // No trusted identity information. No site identity icon is shown.
  IDENTITY_MODE_UNKNOWN: "unknown",

  // Domain-Validation SSL CA-signed domain verification (DV).
  IDENTITY_MODE_IDENTIFIED: "identified",

  // Extended-Validation SSL CA-signed identity information (EV). A more rigorous validation process.
  IDENTITY_MODE_VERIFIED: "verified",

  // Part of the product's UI (built in about: pages)
  IDENTITY_MODE_CHROMEUI: "chromeUI",

  // The following mixed content modes are only used if "security.mixed_content.block_active_content"
  // is enabled. Our Java frontend coalesces them into one indicator.

  // No mixed content information. No mixed content icon is shown.
  MIXED_MODE_UNKNOWN: "unknown",

  // Blocked active mixed content.
  MIXED_MODE_CONTENT_BLOCKED: "blocked",

  // Loaded active mixed content.
  MIXED_MODE_CONTENT_LOADED: "loaded",

  // The following tracking content modes are only used if tracking protection
  // is enabled. Our Java frontend coalesces them into one indicator.

  // No tracking content information. No tracking content icon is shown.
  TRACKING_MODE_UNKNOWN: "unknown",

  // Blocked active tracking content. Shield icon is shown, with a popup option to load content.
  TRACKING_MODE_CONTENT_BLOCKED: "tracking_content_blocked",

  // Loaded active tracking content. Yellow triangle icon is shown.
  TRACKING_MODE_CONTENT_LOADED: "tracking_content_loaded",

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

  /**
   * Determines the identity mode corresponding to the icon we show in the urlbar.
   */
  getIdentityMode: function getIdentityMode(aState, uri) {
    if (aState & Ci.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL) {
      return this.IDENTITY_MODE_VERIFIED;
    }

    if (aState & Ci.nsIWebProgressListener.STATE_IS_SECURE) {
      return this.IDENTITY_MODE_IDENTIFIED;
    }

    // We also allow "about:" by allowing the selector to be empty (i.e. '(|.....|...|...)'
    let whitelist = /^about:($|about|accounts|addons|buildconfig|cache|config|crashes|devices|downloads|fennec|firefox|feedback|healthreport|home|license|logins|logo|memory|mozilla|networking|plugins|privatebrowsing|rights|serviceworkers|support|telemetry|webrtc)($|\?)/i;
    if (uri.schemeIs("about") && whitelist.test(uri.spec)) {
        return this.IDENTITY_MODE_CHROMEUI;
    }

    return this.IDENTITY_MODE_UNKNOWN;
  },

  getMixedDisplayMode: function getMixedDisplayMode(aState) {
    if (aState & Ci.nsIWebProgressListener.STATE_LOADED_MIXED_DISPLAY_CONTENT) {
        return this.MIXED_MODE_CONTENT_LOADED;
    }

    if (aState & Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_DISPLAY_CONTENT) {
        return this.MIXED_MODE_CONTENT_BLOCKED;
    }

    return this.MIXED_MODE_UNKNOWN;
  },

  getMixedActiveMode: function getActiveDisplayMode(aState) {
    // Only show an indicator for loaded mixed content if the pref to block it is enabled
    if ((aState & Ci.nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT) &&
         !Services.prefs.getBoolPref("security.mixed_content.block_active_content")) {
      return this.MIXED_MODE_CONTENT_LOADED;
    }

    if (aState & Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT) {
      return this.MIXED_MODE_CONTENT_BLOCKED;
    }

    return this.MIXED_MODE_UNKNOWN;
  },

  getTrackingMode: function getTrackingMode(aState, aBrowser) {
    if (aState & Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT) {
      this.shieldHistogramAdd(aBrowser, 2);
      return this.TRACKING_MODE_CONTENT_BLOCKED;
    }

    // Only show an indicator for loaded tracking content if the pref to block it is enabled
    let tpEnabled = Services.prefs.getBoolPref("privacy.trackingprotection.enabled") ||
                    (Services.prefs.getBoolPref("privacy.trackingprotection.pbmode.enabled") &&
                     PrivateBrowsingUtils.isBrowserPrivate(aBrowser));

    if ((aState & Ci.nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT) && tpEnabled) {
      this.shieldHistogramAdd(aBrowser, 1);
      return this.TRACKING_MODE_CONTENT_LOADED;
    }

    this.shieldHistogramAdd(aBrowser, 0);
    return this.TRACKING_MODE_UNKNOWN;
  },

  shieldHistogramAdd: function(browser, value) {
    if (PrivateBrowsingUtils.isBrowserPrivate(browser)) {
      return;
    }
    Telemetry.addData("TRACKING_PROTECTION_SHIELD", value);
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
      locationObj.origin = location.origin;
    } catch (ex) {
      // Can sometimes throw if the URL being visited has no host/hostname,
      // e.g. about:blank. The _state for these pages means we won't need these
      // properties anyways, though.
    }
    this._lastLocation = locationObj;

    let uri = aBrowser.currentURI;
    try {
      uri = Services.uriFixup.createExposableURI(uri);
    } catch (e) {}

    let identityMode = this.getIdentityMode(aState, uri);
    let mixedDisplay = this.getMixedDisplayMode(aState);
    let mixedActive = this.getMixedActiveMode(aState);
    let trackingMode = this.getTrackingMode(aState, aBrowser);
    let result = {
      origin: locationObj.origin,
      mode: {
        identity: identityMode,
        mixed_display: mixedDisplay,
        mixed_active: mixedActive,
        tracking: trackingMode
      }
    };

    // Don't show identity data for pages with an unknown identity or if any
    // mixed content is loaded (mixed display content is loaded by default).
    // We also return for CHROMEUI pages since they don't have any certificate
    // information to load either. result.secure specifically refers to connection
    // security, which is irrelevant for about: pages, as they're loaded locally.
    if (identityMode == this.IDENTITY_MODE_UNKNOWN ||
        identityMode == this.IDENTITY_MODE_CHROMEUI ||
        aState & Ci.nsIWebProgressListener.STATE_IS_BROKEN) {
      result.secure = false;
      return result;
    }

    result.secure = true;

    result.host = this.getEffectiveHost();

    let iData = this.getIdentityData();
    result.verifier = Strings.browser.formatStringFromName("identity.identified.verifier", [iData.caOrg], 1);

    // If the cert is identified, then we can populate the results with credentials
    if (aState & Ci.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL) {
      result.owner = iData.subjectOrg;

      // Build an appropriate supplemental block out of whatever location data we have
      let supplemental = "";
      if (iData.city) {
        supplemental += iData.city + "\n";
      }
      if (iData.state && iData.country) {
        supplemental += Strings.browser.formatStringFromName("identity.identified.state_and_country", [iData.state, iData.country], 2);
        result.country = iData.country;
      } else if (iData.state) { // State only
        supplemental += iData.state;
      } else if (iData.country) { // Country only
        supplemental += iData.country;
        result.country = iData.country;
      }
      result.supplemental = supplemental;

      return result;
    }

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
                                                  iData.cert, {}, {})) {
      result.verifier = Strings.browser.GetStringFromName("identity.identified.verified_by_you");
      result.securityException = true;
    }
    return result;
  },

  /**
   * Attempt to provide proper IDN treatment for host names
   */
  getEffectiveHost: function getEffectiveHost() {
    if (!this._IDNService)
      this._IDNService = Cc["@mozilla.org/network/idn-service;1"]
                         .getService(Ci.nsIIDNService);
    try {
      return this._IDNService.convertToDisplayIDN(this._uri.host, {});
    } catch (e) {
      // If something goes wrong (e.g. hostname is an IP address) just fail back
      // to the full domain.
      return this._lastLocation.hostname;
    }
  }
};

var SearchEngines = {
  _contextMenuId: null,
  PREF_SUGGEST_ENABLED: "browser.search.suggest.enabled",
  PREF_SUGGEST_PROMPTED: "browser.search.suggest.prompted",

  // Shared preference key used for search activity default engine.
  PREF_SEARCH_ACTIVITY_ENGINE_KEY: "search.engines.defaultname",

  init: function init() {
    GlobalEventDispatcher.registerListener(this, [
      "SearchEngines:Add",
      "SearchEngines:GetVisible",
      "SearchEngines:Remove",
      "SearchEngines:RestoreDefaults",
      "SearchEngines:SetDefault",
    ]);
    Services.obs.addObserver(this, "browser-search-engine-modified");
  },

  // Fetch list of search engines. all ? All engines : Visible engines only.
  _handleSearchEnginesGetVisible: function _handleSearchEnginesGetVisible(rv, all) {
    if (!Components.isSuccessCode(rv)) {
      Cu.reportError("Could not initialize search service, bailing out.");
      return;
    }

    let engineData = Services.search.getVisibleEngines({});

    // Our Java UI assumes that the default engine is the first item in the array,
    // so we need to make sure that's the case.
    if (engineData[0] !== Services.search.defaultEngine) {
      engineData = engineData.filter(engine => engine !== Services.search.defaultEngine);
      engineData.unshift(Services.search.defaultEngine);
    }

    let searchEngines = engineData.map(function (engine) {
      return {
        name: engine.name,
        identifier: engine.identifier,
        iconURI: (engine.iconURI ? engine.iconURI.spec : null),
        hidden: engine.hidden
      };
    });

    let suggestTemplate = null;
    let suggestEngine = null;

    // Check to see if the default engine supports search suggestions. We only need to check
    // the default engine because we only show suggestions for the default engine in the UI.
    let engine = Services.search.defaultEngine;
    if (engine.supportsResponseType("application/x-suggestions+json")) {
      suggestEngine = engine.name;
      suggestTemplate = engine.getSubmission("__searchTerms__", "application/x-suggestions+json").uri.spec;
    }

    // By convention, the currently configured default engine is at position zero in searchEngines.
    GlobalEventDispatcher.sendRequest({
      type: "SearchEngines:Data",
      searchEngines: searchEngines,
      suggest: {
        engine: suggestEngine,
        template: suggestTemplate,
        enabled: Services.prefs.getBoolPref(this.PREF_SUGGEST_ENABLED),
        prompted: Services.prefs.getBoolPref(this.PREF_SUGGEST_PROMPTED)
      }
    });

    // Send a speculative connection to the default engine.
    Services.search.defaultEngine.speculativeConnect({ window: window,
                                                       originAttributes: {} });
  },

  // Helper method to extract the engine name from a JSON. Simplifies the observe function.
  _extractEngineFromJSON: function _extractEngineFromJSON(data) {
    return Services.search.getEngineByName(data.engine);
  },

  onEvent: function (event, data, callback) {
    let engine;
    switch (event) {
      case "SearchEngines:Add":
        this.displaySearchEnginesList(data);
        break;
      case "SearchEngines:GetVisible":
        Services.search.init(this._handleSearchEnginesGetVisible.bind(this));
        break;
      case "SearchEngines:Remove":
        // Make sure the engine isn't hidden before removing it, to make sure it's
        // visible if the user later re-adds it (works around bug 341833)
        engine = this._extractEngineFromJSON(data);
        engine.hidden = false;
        Services.search.removeEngine(engine);
        break;
      case "SearchEngines:RestoreDefaults":
        // Un-hides all default engines.
        Services.search.restoreDefaultEngines();
        break;
      case "SearchEngines:SetDefault":
        engine = this._extractEngineFromJSON(data);
        // Move the new default search engine to the top of the search engine list.
        Services.search.moveEngine(engine, 0);
        Services.search.defaultEngine = engine;
        break;
      default:
        dump("Unexpected message type observed: " + event);
        break;
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "browser-search-engine-modified":
        if (aData == "engine-default") {
          this._setSearchActivityDefaultPref(aSubject.QueryInterface(Ci.nsISearchEngine));
        }
        break;
      default:
        dump("Unexpected message type observed: " + aTopic);
        break;
    }
  },

  migrateSearchActivityDefaultPref: function migrateSearchActivityDefaultPref() {
    Services.search.init(() => this._setSearchActivityDefaultPref(Services.search.defaultEngine));
  },

  // Updates the search activity pref when the default engine changes.
  _setSearchActivityDefaultPref: function _setSearchActivityDefaultPref(engine) {
    SharedPreferences.forApp().setCharPref(this.PREF_SEARCH_ACTIVITY_ENGINE_KEY, engine.name);
  },

  // Display context menu listing names of the search engines available to be added.
  displaySearchEnginesList: function displaySearchEnginesList(data) {
    let tab = BrowserApp.getTabForId(data.tabId);

    if (!tab)
      return;

    let browser = tab.browser;
    let engines = browser.engines;

    let p = new Prompt({
      window: browser.contentWindow
    }).setSingleChoiceItems(engines.map(function(e) {
      return { label: e.title };
    })).show((function(data) {
      if (data.button == -1)
        return;

      this.addOpenSearchEngine(engines[data.button]);
      engines.splice(data.button, 1);

      if (engines.length < 1) {
        // Broadcast message that there are no more add-able search engines.
        let newEngineMessage = {
          type: "Link:OpenSearch",
          tabID: tab.id,
          visible: false
        };

        GlobalEventDispatcher.sendRequest(newEngineMessage);
      }
    }).bind(this));
  },

  addOpenSearchEngine: function addOpenSearchEngine(engine) {
    Services.search.addEngine(engine.url, Ci.nsISearchEngine.DATA_XML, engine.iconURL, false, {
      onSuccess: function() {
        // Display a toast confirming addition of new search engine.
        Snackbars.show(Strings.browser.formatStringFromName("alertSearchEngineAddedToast", [engine.title], 1), Snackbars.LENGTH_LONG);
      },

      onError: function(aCode) {
        let errorMessage;
        if (aCode == 2) {
          // Engine is a duplicate.
          errorMessage = "alertSearchEngineDuplicateToast";

        } else {
          // Unknown failure. Display general error message.
          errorMessage = "alertSearchEngineErrorToast";
        }

        Snackbars.show(Strings.browser.formatStringFromName(errorMessage, [engine.title], 1), Snackbars.LENGTH_LONG);
      }
    });
  },

  /**
   * Build and return an array of sorted form data / Query Parameters
   * for an element in a submission form.
   *
   * @param element
   *        A valid submission element of a form.
   */
  _getSortedFormData: function(element) {
    let formData = [];

    for (let formElement of element.form.elements) {
      if (!formElement.type) {
        continue;
      }

      // Make this text field a generic search parameter.
      if (element == formElement) {
        formData.push({ name: formElement.name, value: "{searchTerms}" });
        continue;
      }

      // Add other form elements as parameters.
      switch (formElement.type.toLowerCase()) {
        case "checkbox":
        case "radio":
          if (!formElement.checked) {
            break;
          }
        case "text":
        case "hidden":
        case "textarea":
          formData.push({ name: escape(formElement.name), value: escape(formElement.value) });
          break;

        case "select-one": {
          for (let option of formElement.options) {
            if (option.selected) {
              formData.push({ name: escape(formElement.name), value: escape(formElement.value) });
              break;
            }
          }
        }
      }
    };

    // Return valid, pre-sorted queryParams.
    return formData.filter(a => a.name && a.value).sort((a, b) => {
      // nsIBrowserSearchService.hasEngineWithURL() ensures sort, but this helps.
      if (a.name > b.name) {
        return 1;
      }
      if (b.name > a.name) {
        return -1;
      }

      if (a.value > b.value) {
        return 1;
      }
      if (b.value > a.value) {
        return -1;
      }

      return 0;
    });
  },

  /**
   * Check if any search engines already handle an EngineURL of type
   * URLTYPE_SEARCH_HTML, matching this request-method, formURL, and queryParams.
   */
  visibleEngineExists: function(element) {
    let formData = this._getSortedFormData(element);

    let form = element.form;
    let method = form.method.toUpperCase();

    let charset = element.ownerDocument.characterSet;
    let docURI = Services.io.newURI(element.ownerDocument.URL, charset);
    let formURL = Services.io.newURI(form.getAttribute("action"), charset, docURI).spec;

    return Services.search.hasEngineWithURL(method, formURL, formData);
  },

  /**
   * Adds a new search engine to the BrowserSearchService, based on its provided element. Prompts for an engine
   * name, and appends a simple version-number in case of collision with an existing name.
   *
   * @return callback to handle success value. Currently used for ActionBarHandler.js and UI updates.
   */
  addEngine: function addEngine(aElement, resultCallback) {
    let form = aElement.form;
    let charset = aElement.ownerDocument.characterSet;
    let docURI = Services.io.newURI(aElement.ownerDocument.URL, charset);
    let formURL = Services.io.newURI(form.getAttribute("action"), charset, docURI).spec;
    let method = form.method.toUpperCase();
    let formData = this._getSortedFormData(aElement);

    // prompt user for name of search engine
    let promptTitle = Strings.browser.GetStringFromName("contextmenu.addSearchEngine3");
    let title = { value: (aElement.ownerDocument.title || docURI.host) };
    if (!Services.prompt.prompt(null, promptTitle, null, title, null, {})) {
      if (resultCallback) {
        resultCallback(false);
      };
      return;
    }

    Services.search.init(function addEngine_cb(rv) {
      if (!Components.isSuccessCode(rv)) {
        Cu.reportError("Could not initialize search service, bailing out.");
        if (resultCallback) {
          resultCallback(false);
        };
        return;
      }

      GlobalEventDispatcher.sendRequestForResult({
        type: 'Favicon:Request',
        url: docURI.spec,
        skipNetwork: false
      }).then(data => {
        // if there's already an engine with this name, add a number to
        // make the name unique (e.g., "Google" becomes "Google 2")
        let name = title.value;
        for (let i = 2; Services.search.getEngineByName(name); i++) {
            name = title.value + " " + i;
        }

        Services.search.addEngineWithDetails(name, data, null, null, method, formURL);
        Snackbars.show(Strings.browser.formatStringFromName("alertSearchEngineAddedToast", [name], 1), Snackbars.LENGTH_LONG);

        let engine = Services.search.getEngineByName(name);
        engine.wrappedJSObject._queryCharset = charset;
        formData.forEach(param => { engine.addParam(param.name, param.value, null); });

        if (resultCallback) {
            return resultCallback(true);
        };
      }).catch(e => {
        dump("Error while fetching icon for search engine");

        resultCallback(false);
      });
    });
  }
};

var ActivityObserver = {
  init: function ao_init() {
    Services.obs.addObserver(this, "application-background");
    Services.obs.addObserver(this, "application-foreground");
  },

  observe: function ao_observe(aSubject, aTopic, aData) {
    let isForeground = false;
    let tab = BrowserApp.selectedTab;

    UITelemetry.addEvent("show.1", "system", null, aTopic);

    switch (aTopic) {
      case "application-background" :
        let doc = (tab ? tab.browser.contentDocument : null);
        if (doc && doc.fullscreenElement) {
          doc.exitFullscreen();
        }
        isForeground = false;
        break;
      case "application-foreground" :
        isForeground = true;
        break;
    }

    if (tab && tab.getActive() != isForeground) {
      tab.setActive(isForeground);
    }
  }
};

var Telemetry = {
  addData: function addData(aHistogramId, aValue) {
    let histogram = Services.telemetry.getHistogramById(aHistogramId);
    histogram.add(aValue);
  },
};

var Experiments = {
  // Enable malware download protection (bug 936041)
  MALWARE_DOWNLOAD_PROTECTION: "malware-download-protection",

  // Try to load pages from disk cache when network is offline (bug 935190)
  OFFLINE_CACHE: "offline-cache",

  init() {
    GlobalEventDispatcher.sendRequestForResult({
      type: "Experiments:GetActive"
    }).then(names => {
      for (let name of names) {
        switch (name) {
          case this.MALWARE_DOWNLOAD_PROTECTION: {
            // Apply experiment preferences on the default branch. This allows
            // us to avoid migrating user prefs when experiments are enabled/disabled,
            // and it also allows users to override these prefs in about:config.
            let defaults = Services.prefs.getDefaultBranch(null);
            defaults.setBoolPref("browser.safebrowsing.downloads.enabled", true);
            defaults.setBoolPref("browser.safebrowsing.downloads.remote.enabled", true);
            continue;
          }

          case this.OFFLINE_CACHE: {
            let defaults = Services.prefs.getDefaultBranch(null);
            defaults.setBoolPref("browser.tabs.useCache", true);
            continue;
          }
        }
      }
    });
  },

  setOverride(name, isEnabled) {
    GlobalEventDispatcher.sendRequest({
      type: "Experiments:SetOverride",
      name: name,
      isEnabled: isEnabled
    });
  },

  clearOverride(name) {
    GlobalEventDispatcher.sendRequest({
      type: "Experiments:ClearOverride",
      name: name
    });
  }
};

var ExternalApps = {
  _contextMenuId: null,

  // extend _getLink to pickup html5 media links.
  _getMediaLink: function(aElement) {
    let uri = NativeWindow.contextmenus._getLink(aElement);
    if (uri == null && aElement.nodeType == Ci.nsIDOMNode.ELEMENT_NODE && (aElement instanceof Ci.nsIDOMHTMLMediaElement)) {
      try {
        let mediaSrc = aElement.currentSrc || aElement.src;
        uri = ContentAreaUtils.makeURI(mediaSrc, null, null);
      } catch (e) {}
    }
    return uri;
  },

  init: function helper_init() {
    this._contextMenuId = NativeWindow.contextmenus.add(function(aElement) {
      let uri = null;
      var node = aElement;
      while (node && !uri) {
        uri = ExternalApps._getMediaLink(node);
        node = node.parentNode;
      }
      let apps = [];
      if (uri)
        apps = HelperApps.getAppsForUri(uri);

      return apps.length == 1 ? Strings.browser.formatStringFromName("helperapps.openWithApp2", [apps[0].name], 1) :
                                Strings.browser.GetStringFromName("helperapps.openWithList2");
    }, this.filter, this.openExternal);
  },

  filter: {
    matches: function(aElement) {
      let uri = ExternalApps._getMediaLink(aElement);
      let apps = [];
      if (uri) {
        apps = HelperApps.getAppsForUri(uri);
      }
      return apps.length > 0;
    }
  },

  openExternal: function(aElement) {
    if (aElement.pause) {
      aElement.pause();
    }
    let uri = ExternalApps._getMediaLink(aElement);
    HelperApps.launchUri(uri);
  },

  shouldCheckUri: function(uri) {
    if (!(uri.schemeIs("http") || uri.schemeIs("https") || uri.schemeIs("file"))) {
      return false;
    }

    return true;
  },

  updatePageAction: function updatePageAction(uri, contentDocument) {
    HelperApps.getAppsForUri(uri, { filterHttp: true }, (apps) => {
      this.clearPageAction();
      if (apps.length > 0)
        this._setUriForPageAction(uri, apps, contentDocument);
    });
  },

  updatePageActionUri: function updatePageActionUri(uri) {
    this._pageActionUri = uri;
  },

  _getMediaContentElement(contentDocument) {
    if (!contentDocument.contentType.startsWith("video/") &&
        !contentDocument.contentType.startsWith("audio/")) {
      return null;
    }

    let element = contentDocument.activeElement;

    if (element instanceof HTMLBodyElement) {
      element = element.firstChild;
    }

    if (element instanceof HTMLMediaElement) {
      return element;
    }

    return null;
  },

  _setUriForPageAction: function setUriForPageAction(uri, apps, contentDocument) {
    this.updatePageActionUri(uri);

    // If the pageaction is already added, simply update the URI to be launched when 'onclick' is triggered.
    if (this._pageActionId != undefined)
      return;

    let mediaElement = this._getMediaContentElement(contentDocument);

    this._pageActionId = PageActions.add({
      title: Strings.browser.GetStringFromName("openInApp.pageAction"),
      icon: "drawable://icon_openinapp",

      clickCallback: () => {
        UITelemetry.addEvent("launch.1", "pageaction", null, "helper");

        let wasPlaying = mediaElement && !mediaElement.paused && !mediaElement.ended;
        if (wasPlaying) {
          mediaElement.pause();
        }

        if (apps.length > 1) {
          // Use the HelperApps prompt here to filter out any Http handlers
          HelperApps.prompt(apps, {
            title: Strings.browser.GetStringFromName("openInApp.pageAction"),
            buttons: [
              Strings.browser.GetStringFromName("openInApp.ok"),
              Strings.browser.GetStringFromName("openInApp.cancel")
            ]
          }, (result) => {
            if (result.button != 0) {
              if (wasPlaying) {
                mediaElement.play();
              }

              return;
            }
            apps[result.icongrid0].launch(this._pageActionUri);
          });
        } else {
          apps[0].launch(this._pageActionUri);
        }
      }
    });
  },

  clearPageAction: function clearPageAction() {
    if(!this._pageActionId)
      return;

    PageActions.remove(this._pageActionId);
    delete this._pageActionId;
  },
};

var Distribution = {
  // File used to store campaign data
  _file: null,

  _preferencesJSON: null,

  init: function dc_init() {
    GlobalEventDispatcher.registerListener(this, [
      "Campaign:Set",
      "Distribution:Changed",
      "Distribution:Set",
    ]);
    Services.obs.addObserver(this, "prefservice:after-app-defaults");

    // Look for file outside the APK:
    // /data/data/org.mozilla.xxx/distribution.json
    this._file = Services.dirsvc.get("XCurProcD", Ci.nsIFile);
    this._file.append("distribution.json");
    this.readJSON(this._file, this.update);
  },

  onEvent: function dc_onEvent(event, data, callback) {
    switch (event) {
      case "Campaign:Set": {
        // Update the prefs for this session
        try {
          this.update(data);
        } catch (ex) {
          Cu.reportError("Distribution: Could not parse JSON: " + ex);
          return;
        }

        // Asynchronously copy the data to the file.
        let array = new TextEncoder().encode(JSON.stringify(data));
        OS.File.writeAtomic(this._file.path, array, { tmpPath: this._file.path + ".tmp" });
        break;
      }

      case "Distribution:Changed":
        // Re-init the search service.
        try {
          Services.search._asyncReInit();
        } catch (e) {
          console.log("Unable to reinit search service.");
        }
        // Fall through.

      case "Distribution:Set":
        if (data) {
          try {
            this._preferencesJSON = JSON.parse(data.preferences);
          } catch (e) {
            console.log("Invalid distribution JSON.");
          }
        }
        // Reload the default prefs so we can observe "prefservice:after-app-defaults"
        Services.prefs.QueryInterface(Ci.nsIObserver).observe(null, "reload-default-prefs", null);
        this.installDistroAddons();
        break;
    }
  },

  observe: function dc_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "prefservice:after-app-defaults":
        this.getPrefs();
        break;
    }
  },

  update: function dc_update(aData) {
    // Force the distribution preferences on the default branch
    let defaults = Services.prefs.getDefaultBranch(null);
    defaults.setCharPref("distribution.id", aData.id);
    defaults.setCharPref("distribution.version", aData.version);
  },

  getPrefs: function dc_getPrefs() {
    if (this._preferencesJSON) {
        this.applyPrefs(this._preferencesJSON);
        this._preferencesJSON = null;
        return;
    }

    // Get the distribution directory, and bail if it doesn't exist.
    let file = FileUtils.getDir("XREAppDist", [], false);
    if (!file.exists())
      return;

    file.append("preferences.json");
    this.readJSON(file, this.applyPrefs);
  },

  applyPrefs: function dc_applyPrefs(aData) {
    // Check for required Global preferences
    let global = aData["Global"];
    if (!(global && global["id"] && global["version"] && global["about"])) {
      Cu.reportError("Distribution: missing or incomplete Global preferences");
      return;
    }

    // Force the distribution preferences on the default branch
    let defaults = Services.prefs.getDefaultBranch(null);
    defaults.setCharPref("distribution.id", global["id"]);
    defaults.setCharPref("distribution.version", global["version"]);

    let locale = BrowserApp.getUALocalePref();
    defaults.setStringPref("distribution.about",
                           global["about." + locale] || global["about"]);

    let prefs = aData["Preferences"];
    for (let key in prefs) {
      try {
        let value = prefs[key];
        switch (typeof value) {
          case "boolean":
            defaults.setBoolPref(key, value);
            break;
          case "number":
            defaults.setIntPref(key, value);
            break;
          case "string":
          case "undefined":
            defaults.setCharPref(key, value);
            break;
        }
      } catch (e) { /* ignore bad prefs and move on */ }
    }

    // Apply a lightweight theme if necessary
    if (prefs && prefs["lightweightThemes.selectedThemeID"]) {
      Services.obs.notifyObservers(null, "lightweight-theme-apply", "");
    }

    let localizedString = Cc["@mozilla.org/pref-localizedstring;1"].createInstance(Ci.nsIPrefLocalizedString);
    let localizeablePrefs = aData["LocalizablePreferences"];
    for (let key in localizeablePrefs) {
      try {
        let value = localizeablePrefs[key];
        value = value.replace(/%LOCALE%/g, locale);
        localizedString.data = "data:text/plain," + key + "=" + value;
        defaults.setComplexValue(key, Ci.nsIPrefLocalizedString, localizedString);
      } catch (e) { /* ignore bad prefs and move on */ }
    }

    let localizeablePrefsOverrides = aData["LocalizablePreferences." + locale];
    for (let key in localizeablePrefsOverrides) {
      try {
        let value = localizeablePrefsOverrides[key];
        localizedString.data = "data:text/plain," + key + "=" + value;
        defaults.setComplexValue(key, Ci.nsIPrefLocalizedString, localizedString);
      } catch (e) { /* ignore bad prefs and move on */ }
    }

    GlobalEventDispatcher.sendRequest({ type: "Distribution:Set:OK" });
  },

  // aFile is an nsIFile
  // aCallback takes the parsed JSON object as a parameter
  readJSON: function dc_readJSON(aFile, aCallback) {
    Task.spawn(function*() {
      let bytes = yield OS.File.read(aFile.path);
      let raw = new TextDecoder().decode(bytes) || "";

      try {
        aCallback(JSON.parse(raw));
      } catch (e) {
        Cu.reportError("Distribution: Could not parse JSON: " + e);
      }
    }).then(null, function onError(reason) {
      if (!(reason instanceof OS.File.Error && reason.becauseNoSuchFile)) {
        Cu.reportError("Distribution: Could not read from " + aFile.leafName + " file");
      }
    });
  },

  // Track pending installs so we can avoid showing notifications for them.
  pendingAddonInstalls: new Set(),

  installDistroAddons: Task.async(function* () {
    const PREF_ADDONS_INSTALLED = "distribution.addonsInstalled";
    try {
      let installed = Services.prefs.getBoolPref(PREF_ADDONS_INSTALLED);
      if (installed) {
        return;
      }
    } catch (e) {
      Services.prefs.setBoolPref(PREF_ADDONS_INSTALLED, true);
    }

    let distroPath;
    try {
      distroPath = FileUtils.getDir("XREAppDist", ["extensions"]).path;

      let info = yield OS.File.stat(distroPath);
      if (!info.isDir) {
        return;
      }
    } catch (e) {
      return;
    }

    let it = new OS.File.DirectoryIterator(distroPath);
    try {
      yield it.forEach(entry => {
        // Only support extensions that are zipped in .xpi files.
        if (entry.isDir || !entry.name.endsWith(".xpi")) {
          dump("Ignoring distribution add-on that isn't an XPI: " + entry.path);
          return;
        }

        AddonManager.getInstallForFile(new FileUtils.File(entry.path)).then(install => {
          let id = entry.name.substring(0, entry.name.length - 4);
          if (install.addon.id !== id) {
            Cu.reportError("File entry " + entry.path + " contains an add-on with an incorrect ID");
            return;
          }
          this.pendingAddonInstalls.add(install);
          install.install();
        }).catch(e => {
          Cu.reportError("Error installing distribution add-on: " + entry.path + ": " + e);
        });
      });
    } finally {
      it.close();
    }
  })
};

var Tabs = {
  _enableTabExpiration: false,
  _useCache: false,
  _domains: new Set(),

  init: function() {
    // On low-memory platforms, always allow tab expiration. On high-mem
    // platforms, allow it to be turned on once we hit a low-mem situation.
    if (BrowserApp.isOnLowMemoryPlatform) {
      this._enableTabExpiration = true;
    } else {
      Services.obs.addObserver(this, "memory-pressure");
    }

    GlobalEventDispatcher.registerListener(this, [
      // Watch for opportunities to pre-connect to high probability targets.
      "Session:Prefetch",
    ]);

    // Track the network connection so we can efficiently use the cache
    // for possible offline rendering.
    Services.obs.addObserver(this, "network:link-status-changed");
    let network = Cc["@mozilla.org/network/network-link-service;1"].getService(Ci.nsINetworkLinkService);
    this.useCache = !network.isLinkUp;

    BrowserApp.deck.addEventListener("pageshow", this);
    BrowserApp.deck.addEventListener("TabOpen", this);
  },

  onEvent: function(event, data, callback) {
    switch (event) {
      case "Session:Prefetch":
        if (!data.url) {
          break;
        }
        try {
          let uri = Services.io.newURI(data.url);
          if (uri && !this._domains.has(uri.host)) {
            Services.io.QueryInterface(Ci.nsISpeculativeConnect).speculativeConnect2(
                uri, BrowserApp.selectedBrowser.contentDocument.nodePrincipal, null);
            this._domains.add(uri.host);
          }
        } catch (e) {}
        break;
    }
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "memory-pressure":
        if (aData != "heap-minimize") {
          // We received a low-memory related notification. This will enable
          // expirations.
          this._enableTabExpiration = true;
          Services.obs.removeObserver(this, "memory-pressure");
        } else {
          // Use "heap-minimize" as a trigger to expire the most stale tab.
          this.expireLruTab();
        }
        break;
      case "network:link-status-changed":
        if (["down", "unknown", "up"].indexOf(aData) == -1) {
          return;
        }
        this.useCache = (aData === "down");
        break;
    }
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "pageshow":
        // Clear the domain cache whenever a page is loaded into any browser.
        this._domains.clear();

        break;
      case "TabOpen":
        // Use opening a new tab as a trigger to expire the most stale tab.
        this.expireLruTab();
        break;
    }
  },

  // Manage the most-recently-used list of tabs. Each tab has a timestamp
  // associated with it that indicates when it was last touched.
  expireLruTab: function() {
    if (!this._enableTabExpiration) {
      return false;
    }
    let expireTimeMs = Services.prefs.getIntPref("browser.tabs.expireTime") * 1000;
    if (expireTimeMs < 0) {
      // This behaviour is disabled.
      return false;
    }
    let tabs = BrowserApp.tabs;
    let selected = BrowserApp.selectedTab;
    let lruTab = null;
    // Find the least recently used non-zombie tab.
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i] == selected ||
          tabs[i].browser.__SS_restore ||
          tabs[i].playingAudio) {
        // This tab is selected, is already a zombie, or is currently playing
        // audio, skip it.
        continue;
      }
      if (lruTab == null || tabs[i].lastTouchedAt < lruTab.lastTouchedAt) {
        lruTab = tabs[i];
      }
    }
    // If the tab was last touched more than browser.tabs.expireTime seconds ago,
    // zombify it.
    if (lruTab) {
      if (Date.now() - lruTab.lastTouchedAt > expireTimeMs) {
        lruTab.zombify();
        return true;
      }
    }
    return false;
  },

  get useCache() {
    if (!Services.prefs.getBoolPref("browser.tabs.useCache")) {
      return false;
    }
    return this._useCache;
  },

  set useCache(aUseCache) {
    if (!Services.prefs.getBoolPref("browser.tabs.useCache")) {
      return;
    }

    if (this._useCache == aUseCache) {
      return;
    }

    BrowserApp.tabs.forEach(function(tab) {
      if (tab.browser && tab.browser.docShell) {
        if (aUseCache) {
          tab.browser.docShell.defaultLoadFlags |= Ci.nsIRequest.LOAD_FROM_CACHE;
        } else {
          tab.browser.docShell.defaultLoadFlags &= ~Ci.nsIRequest.LOAD_FROM_CACHE;
        }
      }
    });
    this._useCache = aUseCache;
  },

  // For debugging
  dump: function(aPrefix) {
    let tabs = BrowserApp.tabs;
    for (let i = 0; i < tabs.length; i++) {
      dump(aPrefix + " | " + "Tab [" + tabs[i].browser.contentWindow.location.href + "]: lastTouchedAt:" + tabs[i].lastTouchedAt + ", zombie:" + tabs[i].browser.__SS_restore);
    }
  },
};

function ContextMenuItem(args) {
  this.id = ContextMenuItem._nextId;
  this.args = args;

  // Limit to Java int range.
  ContextMenuItem._nextId = (ContextMenuItem._nextId + 1) & 0x7fffffff;
}

ContextMenuItem._nextId = 1;

ContextMenuItem.prototype = {
  get order() {
    return this.args.order || 0;
  },

  matches: function(elt, x, y) {
    return this.args.selector.matches(elt, x, y);
  },

  callback: function(elt) {
    this.args.callback(elt);
  },

  addVal: function(name, elt, defaultValue) {
    if (!(name in this.args))
      return defaultValue;

    if (typeof this.args[name] == "function")
      return this.args[name](elt);

    return this.args[name];
  },

  getValue: function(elt) {
    return {
      id: this.id,
      label: this.addVal("label", elt),
      showAsActions: this.addVal("showAsActions", elt),
      icon: this.addVal("icon", elt),
      isGroup: this.addVal("isGroup", elt, false),
      inGroup: this.addVal("inGroup", elt, false),
      disabled: this.addVal("disabled", elt, false),
      selected: this.addVal("selected", elt, false),
      isParent: this.addVal("isParent", elt, false),
    };
  }
}

function HTMLContextMenuItem(elt, target) {
  ContextMenuItem.call(this, { });

  this.menuElementRef = Cu.getWeakReference(elt);
  this.targetElementRef = Cu.getWeakReference(target);
}

HTMLContextMenuItem.prototype = Object.create(ContextMenuItem.prototype, {
  order: {
    value: NativeWindow.contextmenus.DEFAULT_HTML5_ORDER
  },

  matches: {
    value: function(target) {
      let t = this.targetElementRef.get();
      return t === target;
    },
  },

  callback: {
    value: function(target) {
      let elt = this.menuElementRef.get();
      if (!elt) {
        return;
      }

      // If this is a menu item, show a new context menu with the submenu in it
      if (elt instanceof Ci.nsIDOMHTMLMenuElement) {
        try {
          NativeWindow.contextmenus.menus = {};

          let elt = this.menuElementRef.get();
          let target = this.targetElementRef.get();
          if (!elt) {
            return;
          }

          var items = NativeWindow.contextmenus._getHTMLContextMenuItemsForMenu(elt, target);
          // This menu will always only have one context, but we still make sure its the "right" one.
          var context = NativeWindow.contextmenus._getContextType(target);
          if (items.length > 0) {
            NativeWindow.contextmenus._addMenuItems(items, context);
          }

        } catch(ex) {
          Cu.reportError(ex);
        }
      } else {
        // otherwise just click the menu item
        elt.click();
      }
    },
  },

  getValue: {
    value: function(target) {
      let elt = this.menuElementRef.get();
      if (!elt) {
        return null;
      }

      if (elt.hasAttribute("hidden")) {
        return null;
      }

      return {
        id: this.id,
        icon: elt.icon,
        label: elt.label,
        disabled: elt.disabled,
        menu: elt instanceof Ci.nsIDOMHTMLMenuElement
      };
    }
  },
});

