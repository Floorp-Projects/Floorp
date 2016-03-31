/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "MatchPattern",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebRequestCommon",
                                  "resource://gre/modules/WebRequestCommon.jsm");

const IS_HTTP = /^https?:/;

var ContentPolicy = {
  _classDescription: "WebRequest content policy",
  _classID: Components.ID("938e5d24-9ccc-4b55-883e-c252a41f7ce9"),
  _contractID: "@mozilla.org/webrequest/policy;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPolicy,
                                         Ci.nsIFactory,
                                         Ci.nsISupportsWeakReference]),

  init() {
    let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.registerFactory(this._classID, this._classDescription, this._contractID, this);

    this.contentPolicies = new Map();
    Services.cpmm.addMessageListener("WebRequest:AddContentPolicy", this);
    Services.cpmm.addMessageListener("WebRequest:RemoveContentPolicy", this);

    if (initialProcessData && initialProcessData.webRequestContentPolicies) {
      for (let data of initialProcessData.webRequestContentPolicies.values()) {
        this.addContentPolicy(data);
      }
    }
  },

  addContentPolicy({id, blocking, filter}) {
    if (this.contentPolicies.size == 0) {
      this.register();
    }
    if (filter.urls) {
      filter.urls = new MatchPattern(filter.urls);
    }
    this.contentPolicies.set(id, {blocking, filter});
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "WebRequest:AddContentPolicy":
        this.addContentPolicy(msg.data);
        break;

      case "WebRequest:RemoveContentPolicy":
        this.contentPolicies.delete(msg.data.id);
        if (this.contentPolicies.size == 0) {
          this.unregister();
        }
        break;
    }
  },

  register() {
    let catMan = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
    catMan.addCategoryEntry("content-policy", this._contractID, this._contractID, false, true);
  },

  unregister() {
    let catMan = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
    catMan.deleteCategoryEntry("content-policy", this._contractID, false);
  },

  shouldLoad(policyType, contentLocation, requestOrigin,
             node, mimeTypeGuess, extra, requestPrincipal) {
    let url = contentLocation.spec;
    if (IS_HTTP.test(url)) {
      // We'll handle this in our parent process HTTP observer.
      return Ci.nsIContentPolicy.ACCEPT;
    }

    let block = false;
    let ids = [];
    for (let [id, {blocking, filter}] of this.contentPolicies.entries()) {
      if (WebRequestCommon.typeMatches(policyType, filter.types) &&
          WebRequestCommon.urlMatches(contentLocation, filter.urls)) {
        if (blocking) {
          block = true;
        }
        ids.push(id);
      }
    }

    if (!ids.length) {
      return Ci.nsIContentPolicy.ACCEPT;
    }

    let windowId = 0;
    let parentWindowId = -1;
    let mm = Services.cpmm;

    function getWindowId(window) {
      return window.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils)
        .outerWindowID;
    }

    if (policyType == Ci.nsIContentPolicy.TYPE_SUBDOCUMENT ||
        (node instanceof Ci.nsIDOMXULElement && node.localName == "browser")) {
      // Chrome sets frameId to the ID of the sub-window. But when
      // Firefox loads an iframe, it sets |node| to the <iframe>
      // element, whose window is the parent window. We adopt the
      // Chrome behavior here.
      node = node.contentWindow;
    }

    if (node) {
      let window;
      if (node instanceof Ci.nsIDOMWindow) {
        window = node;
      } else {
        let doc;
        if (node.ownerDocument) {
          doc = node.ownerDocument;
        } else {
          doc = node;
        }
        window = doc.defaultView;
      }

      windowId = getWindowId(window);
      if (window.parent !== window) {
        parentWindowId = getWindowId(window.parent);
      }

      let ir = window.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDocShell)
                     .QueryInterface(Ci.nsIInterfaceRequestor);
      try {
        // If e10s is disabled, this throws NS_NOINTERFACE for closed tabs.
        mm = ir.getInterface(Ci.nsIContentFrameMessageManager);
      } catch (e) {
        if (e.result != Cr.NS_NOINTERFACE) {
          throw e;
        }
      }
    }

    let data = {ids,
                url,
                type: WebRequestCommon.typeForPolicyType(policyType),
                windowId,
                parentWindowId};
    if (requestOrigin) {
      data.originUrl = requestOrigin.spec;
    }
    if (block) {
      let rval = mm.sendSyncMessage("WebRequest:ShouldLoad", data);
      if (rval.length == 1 && rval[0].cancel) {
        return Ci.nsIContentPolicy.REJECT;
      }
    } else {
      mm.sendAsyncMessage("WebRequest:ShouldLoad", data);
    }

    return Ci.nsIContentPolicy.ACCEPT;
  },

  shouldProcess: function(contentType, contentLocation, requestOrigin, insecNode, mimeType, extra) {
    return Ci.nsIContentPolicy.ACCEPT;
  },

  createInstance: function(outer, iid) {
    if (outer) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(iid);
  },
};

ContentPolicy.init();
