/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");

const XPINSTALL_MIMETYPE = "application/x-xpinstall";

const MSG_INSTALL_ENABLED = "WebInstallerIsInstallEnabled";
const MSG_INSTALL_ADDON = "WebInstallerInstallAddonFromWebpage";
const MSG_INSTALL_CALLBACK = "WebInstallerInstallCallback";

var log = Log.repository.getLogger("AddonManager.InstallTrigger");
log.level =
  Log.Level[
    Preferences.get("extensions.logging.enabled", false) ? "Warn" : "Trace"
  ];

function CallbackObject(id, callback, mediator) {
  this.id = id;
  this.callback = callback;
  this.callCallback = function(url, status) {
    try {
      this.callback(url, status);
    } catch (e) {
      log.warn("InstallTrigger callback threw an exception: " + e);
    }

    mediator._callbacks.delete(id);
  };
}

function RemoteMediator(window) {
  let utils = window.windowUtils;
  this._windowID = utils.currentInnerWindowID;

  this.mm = window.docShell.messageManager;
  this.mm.addWeakMessageListener(MSG_INSTALL_CALLBACK, this);

  this._lastCallbackID = 0;
  this._callbacks = new Map();
}

RemoteMediator.prototype = {
  receiveMessage(message) {
    if (message.name == MSG_INSTALL_CALLBACK) {
      let payload = message.data;
      let callbackHandler = this._callbacks.get(payload.callbackID);
      if (callbackHandler) {
        callbackHandler.callCallback(payload.url, payload.status);
      }
    }
  },

  enabled(url) {
    let params = {
      mimetype: XPINSTALL_MIMETYPE,
    };
    return this.mm.sendSyncMessage(MSG_INSTALL_ENABLED, params)[0];
  },

  install(install, principal, callback, window) {
    let callbackID = this._addCallback(callback);

    install.mimetype = XPINSTALL_MIMETYPE;
    install.triggeringPrincipal = principal;
    install.callbackID = callbackID;
    install.browsingContext = BrowsingContext.getFromWindow(window);

    return Services.cpmm.sendSyncMessage(MSG_INSTALL_ADDON, install)[0];
  },

  _addCallback(callback) {
    if (!callback || typeof callback != "function") {
      return -1;
    }

    let callbackID = this._windowID + "-" + ++this._lastCallbackID;
    let callbackObject = new CallbackObject(callbackID, callback, this);
    this._callbacks.set(callbackID, callbackObject);
    return callbackID;
  },

  QueryInterface: ChromeUtils.generateQI(["nsISupportsWeakReference"]),
};

function InstallTrigger() {}

InstallTrigger.prototype = {
  // We've declared ourselves as providing the nsIDOMGlobalPropertyInitializer
  // interface.  This means that when the InstallTrigger property is gotten from
  // the window that will createInstance this object and then call init(),
  // passing the window were bound to.  It will then automatically create the
  // WebIDL wrapper (InstallTriggerImpl) for this object.  This indirection is
  // necessary because webidl does not (yet) support statics (bug 863952). See
  // bug 926712 and then bug 1442360 for more details about this implementation.
  init(window) {
    this._window = window;
    this._principal = window.document.nodePrincipal;
    this._url = window.document.documentURIObject;

    this._mediator = new RemoteMediator(window);
    // If we can't set up IPC (e.g., because this is a top-level window or
    // something), then don't expose InstallTrigger.  The Window code handles
    // that, if we throw an exception here.
  },

  enabled() {
    return this._mediator.enabled(this._url.spec);
  },

  updateEnabled() {
    return this.enabled();
  },

  install(installs, callback) {
    let keys = Object.keys(installs);
    if (keys.length > 1) {
      throw new this._window.Error("Only one XPI may be installed at a time");
    }

    let item = installs[keys[0]];

    if (typeof item === "string") {
      item = { URL: item };
    }
    if (!item.URL) {
      throw new this._window.Error(
        "Missing URL property for '" + keys[0] + "'"
      );
    }

    let url = this._resolveURL(item.URL);
    if (!this._checkLoadURIFromScript(url)) {
      throw new this._window.Error(
        "Insufficient permissions to install: " + url.spec
      );
    }

    let iconUrl = null;
    if (item.IconURL) {
      iconUrl = this._resolveURL(item.IconURL);
      if (!this._checkLoadURIFromScript(iconUrl)) {
        iconUrl = null; // If page can't load the icon, just ignore it
      }
    }

    let installData = {
      uri: url.spec,
      hash: item.Hash || null,
      name: item.name,
      icon: iconUrl ? iconUrl.spec : null,
      method: "installTrigger",
      sourceHost: this._window.location?.host,
      sourceURL: this._window.location?.href,
    };

    return this._mediator.install(
      installData,
      this._principal,
      callback,
      this._window
    );
  },

  startSoftwareUpdate(url, flags) {
    let filename = Services.io.newURI(url).QueryInterface(Ci.nsIURL).filename;
    let args = {};
    args[filename] = { URL: url };
    return this.install(args);
  },

  installChrome(type, url, skin) {
    return this.startSoftwareUpdate(url);
  },

  _resolveURL(url) {
    return Services.io.newURI(url, null, this._url);
  },

  _checkLoadURIFromScript(uri) {
    let secman = Services.scriptSecurityManager;
    try {
      secman.checkLoadURIWithPrincipal(
        this._principal,
        uri,
        secman.DISALLOW_INHERIT_PRINCIPAL
      );
      return true;
    } catch (e) {
      return false;
    }
  },

  classID: Components.ID("{9df8ef2b-94da-45c9-ab9f-132eb55fddf1}"),
  contractID: "@mozilla.org/addons/installtrigger;1",
  QueryInterface: ChromeUtils.generateQI(["nsIDOMGlobalPropertyInitializer"]),
};

var EXPORTED_SYMBOLS = ["InstallTrigger"];
