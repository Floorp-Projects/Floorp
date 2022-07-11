/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetters(lazy, {
  ThirdPartyUtil: ["@mozilla.org/thirdpartyutil;1", "mozIThirdPartyUtil"],
});

const XPINSTALL_MIMETYPE = "application/x-xpinstall";

const MSG_INSTALL_ENABLED = "WebInstallerIsInstallEnabled";
const MSG_INSTALL_ADDON = "WebInstallerInstallAddonFromWebpage";
const MSG_INSTALL_CALLBACK = "WebInstallerInstallCallback";

const SUPPORTED_XPI_SCHEMES = ["http", "https"];

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
  this._windowID = window.windowGlobalChild.innerWindowId;

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
    if (Services.prefs.getBoolPref("xpinstall.userActivation.required", true)) {
      if (!this._window.windowUtils.isHandlingUserInput) {
        throw new this._window.Error(
          "InstallTrigger.install can only be called from a user input handler"
        );
      }
    }

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

    if (!SUPPORTED_XPI_SCHEMES.includes(url.scheme)) {
      Cu.reportError(
        `InstallTrigger call disallowed on install url with unsupported scheme: ${JSON.stringify(
          {
            installPrincipal: this._principal.spec,
            installURL: url.spec,
          }
        )}`
      );
      throw new this._window.Error(`Unsupported scheme`);
    }

    let iconUrl = null;
    if (item.IconURL) {
      iconUrl = this._resolveURL(item.IconURL);
      if (!this._checkLoadURIFromScript(iconUrl)) {
        iconUrl = null; // If page can't load the icon, just ignore it
      }
    }

    const getTriggeringSource = () => {
      let url;
      let host;
      try {
        if (this._url?.schemeIs("http") || this._url?.schemeIs("https")) {
          url = this._url.spec;
          host = this._url.host;
        } else if (this._url?.schemeIs("blob")) {
          // For a blob url, we keep the blob url as the sourceURL and
          // we pick the related sourceHost from either the principal
          // or the precursorPrincipal (if the principal is a null principal
          // and the precursor one is defined).
          url = this._url.spec;
          host =
            this._principal.isNullPrincipal &&
            this._principal.precursorPrincipal
              ? this._principal.precursorPrincipal.host
              : this._principal.host;
        } else if (!this._principal.isNullPrincipal) {
          url = this._principal.exposableSpec;
          host = this._principal.host;
        } else if (this._principal.precursorPrincipal) {
          url = this._principal.precursorPrincipal.exposableSpec;
          host = this._principal.precursorPrincipal.host;
        } else {
          // Fallback to this._url as last resort.
          url = this._url.spec;
          host = this._url.host;
        }
      } catch (err) {
        Cu.reportError(err);
      }
      // Fallback to an empty string if url and host are still undefined.
      return {
        sourceURL: url || "",
        sourceHost: host || "",
      };
    };

    const { sourceHost, sourceURL } = getTriggeringSource();

    let installData = {
      uri: url.spec,
      hash: item.Hash || null,
      name: item.name,
      icon: iconUrl ? iconUrl.spec : null,
      method: "installTrigger",
      sourceHost,
      sourceURL,
      hasCrossOriginAncestor: lazy.ThirdPartyUtil.isThirdPartyWindow(
        this._window
      ),
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
