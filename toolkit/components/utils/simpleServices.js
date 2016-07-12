/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Dumping ground for simple services for which the isolation of a full global
 * is overkill. Be careful about namespace pollution, and be mindful about
 * importing lots of JSMs in global scope, since this file will almost certainly
 * be loaded from enough callsites that any such imports will always end up getting
 * eagerly loaded at startup.
 */

"use strict";

const Cc = Components.classes;
const Cu = Components.utils;
const Ci = Components.interfaces;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

function AddonPolicyService()
{
  this.wrappedJSObject = this;
  this.cspStrings = new Map();
  this.backgroundPageUrlCallbacks = new Map();
  this.mayLoadURICallbacks = new Map();
  this.localizeCallbacks = new Map();

  XPCOMUtils.defineLazyPreferenceGetter(
    this, "baseCSP", "extensions.webextensions.base-content-security-policy",
    "script-src 'self' https://* moz-extension: blob: filesystem: 'unsafe-eval' 'unsafe-inline'; " +
    "object-src 'self' https://* moz-extension: blob: filesystem:;");

  XPCOMUtils.defineLazyPreferenceGetter(
    this, "defaultCSP", "extensions.webextensions.default-content-security-policy",
    "script-src 'self'; object-src 'self';");
}

AddonPolicyService.prototype = {
  classID: Components.ID("{89560ed3-72e3-498d-a0e8-ffe50334d7c5}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAddonPolicyService]),

  /**
   * Returns the content security policy which applies to documents belonging
   * to the extension with the given ID. This may be either a custom policy,
   * if one was supplied, or the default policy if one was not.
   */
  getAddonCSP(aAddonId) {
    let csp = this.cspStrings.get(aAddonId);
    return csp || this.defaultCSP;
  },

  /**
   * Returns the generated background page as a data-URI, if any. If the addon
   * does not have an auto-generated background page, an empty string is
   * returned.
   */
  getGeneratedBackgroundPageUrl(aAddonId) {
    let cb = this.backgroundPageUrlCallbacks.get(aAddonId);
    return cb && cb(aAddonId) || '';
  },

  /*
   * Invokes a callback (if any) associated with the addon to determine whether
   * unprivileged code running within the addon is allowed to perform loads from
   * the given URI.
   *
   * @see nsIAddonPolicyService.addonMayLoadURI
   */
  addonMayLoadURI(aAddonId, aURI) {
    let cb = this.mayLoadURICallbacks.get(aAddonId);
    return cb ? cb(aURI) : false;
  },

  /*
   * Invokes a callback (if any) associated with the addon to loclaize a
   * resource belonging to that add-on.
   */
  localizeAddonString(aAddonId, aString) {
    let cb = this.localizeCallbacks.get(aAddonId);
    return cb ? cb(aString) : aString;
  },

  /*
   * Invokes a callback (if any) to determine if an extension URI should be
   * web-accessible.
   *
   * @see nsIAddonPolicyService.extensionURILoadableByAnyone
   */
  extensionURILoadableByAnyone(aURI) {
    if (aURI.scheme != "moz-extension") {
      throw new TypeError("non-extension URI passed");
    }

    let cb = this.extensionURILoadCallback;
    return cb ? cb(aURI) : false;
  },

  /*
   * Maps an extension URI to an addon ID.
   *
   * @see nsIAddonPolicyService.extensionURIToAddonId
   */
  extensionURIToAddonId(aURI) {
    if (aURI.scheme != "moz-extension") {
      throw new TypeError("non-extension URI passed");
    }

    let cb = this.extensionURIToAddonIdCallback;
    if (!cb) {
      throw new Error("no callback set to map extension URIs to addon Ids");
    }
    return cb(aURI);
  },

  /*
   * Sets the callbacks used in addonMayLoadURI above. Not accessible over
   * XPCOM - callers should use .wrappedJSObject on the service to call it
   * directly.
   */
  setAddonLoadURICallback(aAddonId, aCallback) {
    if (aCallback) {
      this.mayLoadURICallbacks.set(aAddonId, aCallback);
    } else {
      this.mayLoadURICallbacks.delete(aAddonId);
    }
  },

  /*
   * Sets the custom CSP string to be used for the add-on. Not accessible over
   * XPCOM - callers should use .wrappedJSObject on the service to call it
   * directly.
   */
  setAddonCSP(aAddonId, aCSPString) {
    if (aCSPString) {
      this.cspStrings.set(aAddonId, aCSPString);
    } else {
      this.cspStrings.delete(aAddonId);
    }
  },

  /**
   * Set the callback that generates a data-URL for the background page.
   */
  setBackgroundPageUrlCallback(aAddonId, aCallback) {
    if (aCallback) {
      this.backgroundPageUrlCallbacks.set(aAddonId, aCallback);
    } else {
      this.backgroundPageUrlCallbacks.delete(aAddonId);
    }
  },

  /*
   * Sets the callbacks used by the stream converter service to localize
   * add-on resources.
   */
  setAddonLocalizeCallback(aAddonId, aCallback) {
    if (aCallback) {
      this.localizeCallbacks.set(aAddonId, aCallback);
    } else {
      this.localizeCallbacks.delete(aAddonId);
    }
  },

  /*
   * Sets the callback used in extensionURILoadableByAnyone above. Not
   * accessible over XPCOM - callers should use .wrappedJSObject on the
   * service to call it directly.
   */
  setExtensionURILoadCallback(aCallback) {
    var old = this.extensionURILoadCallback;
    this.extensionURILoadCallback = aCallback;
    return old;
  },

  /*
   * Sets the callback used in extensionURIToAddonId above. Not accessible over
   * XPCOM - callers should use .wrappedJSObject on the service to call it
   * directly.
   */
  setExtensionURIToAddonIdCallback(aCallback) {
    var old = this.extensionURIToAddonIdCallback;
    this.extensionURIToAddonIdCallback = aCallback;
    return old;
  }
};

/*
 * This class provides a stream filter for locale messages in CSS files served
 * by the moz-extension: protocol handler.
 *
 * See SubstituteChannel in netwerk/protocol/res/ExtensionProtocolHandler.cpp
 * for usage.
 */
function AddonLocalizationConverter()
{
  this.aps = Cc["@mozilla.org/addons/policy-service;1"].getService(Ci.nsIAddonPolicyService)
    .wrappedJSObject;
}

AddonLocalizationConverter.prototype = {
  classID: Components.ID("{ded150e3-c92e-4077-a396-0dba9953e39f}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIStreamConverter]),

  FROM_TYPE: "application/vnd.mozilla.webext.unlocalized",
  TO_TYPE: "text/css",

  checkTypes(aFromType, aToType) {
    if (aFromType != this.FROM_TYPE) {
      throw Components.Exception("Invalid aFromType value", Cr.NS_ERROR_INVALID_ARG,
                                 Components.stack.caller.caller);
    }
    if (aToType != this.TO_TYPE) {
      throw Components.Exception("Invalid aToType value", Cr.NS_ERROR_INVALID_ARG,
                                 Components.stack.caller.caller);
    }
  },

  // aContext must be a nsIURI object for a valid moz-extension: URL.
  getAddonId(aContext) {
    // In this case, we want the add-on ID even if the URL is web accessible,
    // so check the root rather than the exact path.
    let uri = Services.io.newURI("/", null, aContext);

    let id = this.aps.extensionURIToAddonId(uri);
    if (id == undefined) {
      throw new Components.Exception("Invalid context", Cr.NS_ERROR_INVALID_ARG);
    }
    return id;
  },

  convertToStream(aAddonId, aString) {
    let stream = Cc["@mozilla.org/io/string-input-stream;1"]
      .createInstance(Ci.nsIStringInputStream);

    stream.data = this.aps.localizeAddonString(aAddonId, aString);
    return stream;
  },

  convert(aStream, aFromType, aToType, aContext) {
    this.checkTypes(aFromType, aToType);
    let addonId = this.getAddonId(aContext);

    let string = (
      aStream.available() ?
      NetUtil.readInputStreamToString(aStream, aStream.available()): ""
    );
    return this.convertToStream(addonId, string);
  },

  asyncConvertData(aFromType, aToType, aListener, aContext) {
    this.checkTypes(aFromType, aToType);
    this.addonId = this.getAddonId(aContext);
    this.listener = aListener;
  },

  onStartRequest(aRequest, aContext) {
    this.parts = [];
  },

  onDataAvailable(aRequest, aContext, aInputStream, aOffset, aCount) {
    this.parts.push(NetUtil.readInputStreamToString(aInputStream, aCount));
  },

  onStopRequest(aRequest, aContext, aStatusCode) {
    try {
      this.listener.onStartRequest(aRequest, null);
      if (Components.isSuccessCode(aStatusCode)) {
        let string = this.parts.join("");
        let stream = this.convertToStream(this.addonId, string);

        this.listener.onDataAvailable(aRequest, null, stream, 0, stream.data.length);
      }
    } catch (e) {
      aStatusCode = e.result || Cr.NS_ERROR_FAILURE;
    }
    this.listener.onStopRequest(aRequest, null, aStatusCode);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AddonPolicyService,
                                                     AddonLocalizationConverter]);
