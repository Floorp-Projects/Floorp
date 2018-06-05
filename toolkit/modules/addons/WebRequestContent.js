/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Managed via the message managers.
/* globals MatchPatternSet, initialProcessData */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "WebRequestCommon",
                               "resource://gre/modules/WebRequestCommon.jsm");

// Websockets will get handled via httpchannel notifications same as http
// requests, treat them the same as http in ContentPolicy.
const IS_HTTP = /^(?:http|ws)s?:/;

var ContentPolicy = {
  _classDescription: "WebRequest content policy",
  _classID: Components.ID("938e5d24-9ccc-4b55-883e-c252a41f7ce9"),
  _contractID: "@mozilla.org/webrequest/policy;1",

  QueryInterface: ChromeUtils.generateQI([Ci.nsIContentPolicy,
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
      filter.urls = new MatchPatternSet(filter.urls);
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

  shouldLoad(contentLocation, loadInfo, mimeTypeGuess) {
    let policyType = loadInfo.externalContentPolicyType;
    let loadingPrincipal = loadInfo.loadingPrincipal;
    let requestPrincipal = loadInfo.triggeringPrincipal;
    let requestOrigin = null;
    if (loadingPrincipal) {
      requestOrigin = loadingPrincipal.URI;
    }

    // Loads of TYPE_DOCUMENT and TYPE_SUBDOCUMENT perform a ConPol check
    // within docshell as well as within the ContentSecurityManager. To avoid
    // duplicate evaluations we ignore ConPol checks performed within docShell.
    if (loadInfo.skipContentPolicyCheckForWebRequest) {
      return Ci.nsIContentPolicy.ACCEPT;
    }

    if (requestPrincipal &&
        Services.scriptSecurityManager.isSystemPrincipal(requestPrincipal)) {
      return Ci.nsIContentPolicy.ACCEPT;
    }
    let url = contentLocation.spec;
    if (IS_HTTP.test(url)) {
      // We'll handle this in our parent process HTTP observer.
      return Ci.nsIContentPolicy.ACCEPT;
    }

    let ids = [];
    for (let [id, {filter}] of this.contentPolicies.entries()) {
      if (WebRequestCommon.typeMatches(policyType, filter.types) &&
          WebRequestCommon.urlMatches(contentLocation, filter.urls)) {
        ids.push(id);
      }
    }

    if (!ids.length) {
      return Ci.nsIContentPolicy.ACCEPT;
    }

    let windowId = 0;
    let parentWindowId = -1;
    let frameAncestors = [];
    let mm = Services.cpmm;

    function getWindowId(window) {
      return window.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils)
        .outerWindowID;
    }

    let node = loadInfo.loadingContext;
    if (node &&
        (policyType == Ci.nsIContentPolicy.TYPE_SUBDOCUMENT ||
         (ChromeUtils.getClassName(node) == "XULElement" &&
          node.localName == "browser"))) {
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

        for (let frame = window.parent; ; frame = frame.parent) {
          frameAncestors.push({
            url: frame.document.documentURIObject.spec,
            frameId: getWindowId(frame),
          });
          if (frame === frame.parent) {
            // Set the last frameId to zero for top level frame.
            frameAncestors[frameAncestors.length - 1].frameId = 0;
            break;
          }
        }
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
    if (frameAncestors.length > 0) {
      data.frameAncestors = frameAncestors;
    }
    if (requestOrigin) {
      data.documentUrl = requestOrigin.spec;
    }
    if (requestPrincipal && requestPrincipal.URI) {
      data.originUrl = requestPrincipal.URI.spec;
    }
    mm.sendAsyncMessage("WebRequest:ShouldLoad", data);

    return Ci.nsIContentPolicy.ACCEPT;
  },

  shouldProcess: function(contentLocation, loadInfo, mimeType) {
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
