/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, manager: Cm, results: Cr, utils: Cu, Constructor: CC} = Components;

Cm.QueryInterface(Ci.nsIComponentRegistrar);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Console",
                                  "resource://gre/modules/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "gFlyWebBundle", function() {
  return Services.strings.createBundle("chrome://flyweb/locale/flyweb.properties");
});

const FLYWEB_ENABLED_PREF = "dom.flyweb.enabled";

let factory, menuID;

function AboutFlyWeb() {}

AboutFlyWeb.prototype = Object.freeze({
  classDescription: "About page for displaying nearby FlyWeb services",
  contractID: "@mozilla.org/network/protocol/about;1?what=flyweb",
  classID: Components.ID("{baa04ff0-08b5-11e6-a837-0800200c9a66}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutModule]),

  getURIFlags: function(aURI) {
    return Ci.nsIAboutModule.ALLOW_SCRIPT;
  },

  newChannel: function(aURI, aLoadInfo) {
    let uri = Services.io.newURI("chrome://flyweb/content/aboutFlyWeb.xhtml", null, null);
    let channel = Services.io.newChannelFromURIWithLoadInfo(uri, aLoadInfo);
    channel.originalURI = aURI;
    return channel;
  }
});

function Factory(component) {
  this.createInstance = function(outer, iid) {
    if (outer) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return new component();
  };
  this.register = function() {
    Cm.registerFactory(component.prototype.classID, component.prototype.classDescription, component.prototype.contractID, this);
  };
  this.unregister = function() {
    Cm.unregisterFactory(component.prototype.classID, this);
  }
  Object.freeze(this);
  this.register();
}

let windowListener = {
  onOpenWindow: function(aWindow) {
    // Wait for the window to finish loading
    let domWindow = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowInternal || Ci.nsIDOMWindow);
    domWindow.addEventListener("UIReady", function onLoad() {
      domWindow.removeEventListener("UIReady", onLoad, false);
      loadIntoWindow(domWindow);
    }, false);
  },

  onCloseWindow: function(aWindow) {},
  onWindowTitleChange: function(aWindow, aTitle) {}
};

let FlyWebUI = {
  init() {
    factory = new Factory(AboutFlyWeb);

    // Load into any existing windows
    let windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      let domWindow = windows.getNext().QueryInterface(Ci.nsIDOMWindow);
      loadIntoWindow(domWindow);
    }

    // Load into any new windows
    Services.wm.addListener(windowListener);
  },

  uninit() {
    factory.unregister();

    // Stop listening for new windows
    Services.wm.removeListener(windowListener);

    // Unload from any existing windows
    let windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      let domWindow = windows.getNext().QueryInterface(Ci.nsIDOMWindow);
      unloadFromWindow(domWindow);
    }
  }
};

function loadIntoWindow(aWindow) {
  menuID = aWindow.NativeWindow.menu.add({
    name: gFlyWebBundle.GetStringFromName("flyweb-menu.name"),
    callback() {
      aWindow.BrowserApp.addTab("about:flyweb");
    }
  });
}

function unloadFromWindow(aWindow) {
  if (!aWindow) {
    return;
  }

  aWindow.NativeWindow.menu.remove(menuID);
}

function prefObserver(aSubject, aTopic, aData) {
  let enabled = Services.prefs.getBoolPref(FLYWEB_ENABLED_PREF);
  if (enabled) {
    FlyWebUI.init();
  } else {
    FlyWebUI.uninit();
  }
}

function install(aData, aReason) {}

function uninstall(aData, aReason) {}

function startup(aData, aReason) {
  // Observe pref changes and enable/disable as necessary.
  Services.prefs.addObserver(FLYWEB_ENABLED_PREF, prefObserver, false);

  // Only initialize if pref is enabled.
  let enabled = Services.prefs.getBoolPref(FLYWEB_ENABLED_PREF);
  if (enabled) {
    FlyWebUI.init();
  }
}

function shutdown(aData, aReason) {
  Services.prefs.removeObserver(FLYWEB_ENABLED_PREF, prefObserver);

  FlyWebUI.uninit();
}
