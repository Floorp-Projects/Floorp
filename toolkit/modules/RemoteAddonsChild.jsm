// -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["RemoteAddonsChild"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import('resource://gre/modules/Services.jsm');

/**
 * This code registers an nsIContentPolicy in the child process. When
 * it runs, it notifies the parent that it needs to run its own
 * nsIContentPolicy list. If any policy in the parent rejects the load,
 * that answer is returned to the child.
 */
let ContentPolicyChild = {
  _classDescription: "Addon shim content policy",
  _classID: Components.ID("6e869130-635c-11e2-bcfd-0800200c9a66"),
  _contractID: "@mozilla.org/addon-child/policy;1",

  /**
   * We only notify the parent of the load if it has any
   * non-builtin policies registered.
   */
  _childNeedsHook: false,

  init: function(aContentGlobal) {
    let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.registerFactory(this._classID, this._classDescription, this._contractID, this);

    var catMan = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
    catMan.addCategoryEntry("content-policy", this._contractID, this._contractID, false, true);

    let policiesToIgnore = [];
    let services = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager)
                                                       .enumerateCategory("content-policy");
    while (services.hasMoreElements()) {
      let item = services.getNext();
      let name = item.QueryInterface(Components.interfaces.nsISupportsCString).toString();
      policiesToIgnore.push(name);
    }

    let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
               .getService(Ci.nsISyncMessageSender);
    cpmm.addMessageListener("Addons:ContentPolicy:NeedHook", this);
    cpmm.sendAsyncMessage("Addons:ContentPolicy:IgnorePolicies", { "policies": policiesToIgnore });
  },

  receiveMessage: function(aMessage) {
    switch (aMessage.name) {
      case "Addons:ContentPolicy:NeedHook":
        this._childNeedsHook = aMessage.data.needed;
        break;
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPolicy, Ci.nsIObserver,
                                         Ci.nsIChannelEventSink, Ci.nsIFactory,
                                         Ci.nsISupportsWeakReference]),

  shouldLoad: function(contentType, contentLocation, requestOrigin, node, mimeTypeGuess, extra) {
    if (!this._childNeedsHook)
      return Ci.nsIContentPolicy.ACCEPT;

    let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
               .getService(Ci.nsISyncMessageSender);
    var rval = cpmm.sendRpcMessage("Addons:ContentPolicy:Run", {
      contentType: contentType,
      mimeTypeGuess: mimeTypeGuess
    }, {
      contentLocation: contentLocation,
      requestOrigin: requestOrigin,
      node: node
    });
    if (rval.length != 1)
      return Ci.nsIContentPolicy.ACCEPT;

    return rval[0];
  },

  shouldProcess: function(contentType, contentLocation, requestOrigin, insecNode, mimeType, extra) {
    return Ci.nsIContentPolicy.ACCEPT;
  },

  createInstance: function(outer, iid) {
    if (outer)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },
};

let RemoteAddonsChild = {
  initialized: false,

  init: function(aContentGlobal) {
    if (this.initialized)
      return;

    this.initialized = true;

    ContentPolicyChild.init(aContentGlobal);
  },
};
