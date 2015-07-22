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

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function RemoteTagServiceService()
{
}

RemoteTagServiceService.prototype = {
  classID: Components.ID("{dfd07380-6083-11e4-9803-0800200c9a66}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRemoteTagService, Ci.nsISupportsWeakReference]),

  /**
   * CPOWs can have user data attached to them. This data originates
   * in the local process from this function, getRemoteObjectTag. It's
   * sent along with the CPOW to the remote process, where it can be
   * fetched with Components.utils.getCrossProcessWrapperTag.
   */
  getRemoteObjectTag: function(target) {
    if (target instanceof Ci.nsIDocShellTreeItem) {
      return "ContentDocShellTreeItem";
    }

    if (target instanceof Ci.nsIDOMDocument) {
      return "ContentDocument";
    }

    return "generic";
  }
};

function AddonPolicyService()
{
  this.wrappedJSObject = this;
  this.mayLoadURICallbacks = new Map();
}

AddonPolicyService.prototype = {
  classID: Components.ID("{89560ed3-72e3-498d-a0e8-ffe50334d7c5}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAddonPolicyService]),

  /*
   * Invokes a callback (if any) associated with the addon to determine whether
   * unprivileged code running within the addon is allowed to perform loads from
   * the given URI.
   *
   * @see nsIAddonPolicyService.addonMayLoadURI
   */
  addonMayLoadURI(aAddonId, aURI) {
    let cb = this.mayLoadURICallbacks[aAddonId];
    return cb ? cb(aURI) : false;
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
    this.mayLoadURICallbacks[aAddonId] = aCallback;
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

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RemoteTagServiceService, AddonPolicyService]);
