/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WebRequest"];

/* exported WebRequest */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
                                  "resource://gre/modules/BrowserUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebRequestCommon",
                                  "resource://gre/modules/WebRequestCommon.jsm");

// TODO
// Figure out how to handle requestId. Gecko seems to have no such thing. (Bug 1163862)
// We also don't know the method for content policy. (Bug 1163862)
// We don't even have a window ID for HTTP observer stuff. (Bug 1163861)

function parseFilter(filter) {
  if (!filter) {
    filter = {};
  }

  // FIXME: Support windowId filtering.
  return {urls: filter.urls || null, types: filter.types || null};
}

function parseExtra(extra, allowed) {
  if (extra) {
    for (let ex of extra) {
      if (allowed.indexOf(ex) == -1) {
        throw new Error(`Invalid option ${ex}`);
      }
    }
  }

  let result = {};
  for (let al of allowed) {
    if (extra && extra.indexOf(al) != -1) {
      result[al] = true;
    }
  }
  return result;
}

var ContentPolicyManager = {
  policyData: new Map(),
  policies: new Map(),
  idMap: new Map(),
  nextId: 0,

  init() {
    Services.ppmm.initialProcessData.webRequestContentPolicies = this.policyData;

    Services.ppmm.addMessageListener("WebRequest:ShouldLoad", this);
    Services.mm.addMessageListener("WebRequest:ShouldLoad", this);
  },

  receiveMessage(msg) {
    let browser = msg.target instanceof Ci.nsIDOMXULElement ? msg.target : null;

    for (let id of msg.data.ids) {
      let callback = this.policies.get(id);
      if (!callback) {
        // It's possible that this listener has been removed and the
        // child hasn't learned yet.
        continue;
      }
      let response = null;
      try {
        response = callback({
          url: msg.data.url,
          windowId: msg.data.windowId,
          parentWindowId: msg.data.parentWindowId,
          type: msg.data.type,
          browser: browser,
        });
      } catch (e) {
        Cu.reportError(e);
      }

      if (response && response.cancel) {
        return {cancel: true};
      }

      // FIXME: Need to handle redirection here. (Bug 1163862)
    }

    return {};
  },

  addListener(callback, opts) {
    let id = this.nextId++;
    opts.id = id;
    if (opts.filter.urls) {
      opts.filter.urls = opts.filter.urls.serialize();
    }
    Services.ppmm.broadcastAsyncMessage("WebRequest:AddContentPolicy", opts);

    this.policyData.set(id, opts);

    this.policies.set(id, callback);
    this.idMap.set(callback, id);
  },

  removeListener(callback) {
    let id = this.idMap.get(callback);
    Services.ppmm.broadcastAsyncMessage("WebRequest:RemoveContentPolicy", {id});

    this.policyData.delete(id);
    this.idMap.delete(callback);
    this.policies.delete(id);
  },
};
ContentPolicyManager.init();

function StartStopListener(manager, loadContext) {
  this.manager = manager;
  this.loadContext = loadContext;
  this.orig = null;
}

StartStopListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRequestObserver,
                                         Ci.nsIStreamListener,
                                         Ci.nsISupports]),

  onStartRequest: function(request, context) {
    this.manager.onStartRequest(request, this.loadContext);
    return this.orig.onStartRequest(request, context);
  },

  onStopRequest(request, context, statusCode) {
    let result = this.orig.onStopRequest(request, context, statusCode);
    this.manager.onStopRequest(request, this.loadContext);
    return result;
  },

  onDataAvailable(...args) {
    return this.orig.onDataAvailable(...args);
  },
};

var HttpObserverManager = {
  modifyInitialized: false,
  examineInitialized: false,

  listeners: {
    modify: new Map(),
    afterModify: new Map(),
    headersReceived: new Map(),
    onStart: new Map(),
    onStop: new Map(),
  },

  addOrRemove() {
    let needModify = this.listeners.modify.size || this.listeners.afterModify.size;
    if (needModify && !this.modifyInitialized) {
      this.modifyInitialized = true;
      Services.obs.addObserver(this, "http-on-modify-request", false);
    } else if (!needModify && this.modifyInitialized) {
      this.modifyInitialized = false;
      Services.obs.removeObserver(this, "http-on-modify-request");
    }

    let needExamine = this.listeners.headersReceived.size ||
                      this.listeners.onStart.size ||
                      this.listeners.onStop.size;
    if (needExamine && !this.examineInitialized) {
      this.examineInitialized = true;
      Services.obs.addObserver(this, "http-on-examine-response", false);
      Services.obs.addObserver(this, "http-on-examine-cached-response", false);
      Services.obs.addObserver(this, "http-on-examine-merged-response", false);
    } else if (!needExamine && this.examineInitialized) {
      this.examineInitialized = false;
      Services.obs.removeObserver(this, "http-on-examine-response");
      Services.obs.removeObserver(this, "http-on-examine-cached-response");
      Services.obs.removeObserver(this, "http-on-examine-merged-response");
    }
  },

  addListener(kind, callback, opts) {
    this.listeners[kind].set(callback, opts);
    this.addOrRemove();
  },

  removeListener(kind, callback) {
    this.listeners[kind].delete(callback);
    this.addOrRemove();
  },

  getLoadContext(channel) {
    try {
      return channel.QueryInterface(Ci.nsIChannel)
                    .notificationCallbacks
                    .getInterface(Components.interfaces.nsILoadContext);
    } catch (e) {
      try {
        return channel.loadGroup
                      .notificationCallbacks
                      .getInterface(Components.interfaces.nsILoadContext);
      } catch (e) {
        return null;
      }
    }
  },

  getHeaders(channel, method) {
    let headers = [];
    let visitor = {
      visitHeader(name, value) {
        headers.push({name, value});
      },

      QueryInterface: XPCOMUtils.generateQI([Ci.nsIHttpHeaderVisitor,
                                             Ci.nsISupports]),
    };

    channel[method](visitor);
    return headers;
  },

  observe(subject, topic, data) {
    let channel = subject.QueryInterface(Ci.nsIHttpChannel);

    if (topic == "http-on-modify-request") {
      this.modify(channel, topic, data);
    } else if (topic == "http-on-examine-response" ||
               topic == "http-on-examine-cached-response" ||
               topic == "http-on-examine-merged-response") {
      this.examine(channel, topic, data);
    }
  },

  shouldRunListener(policyType, uri, filter) {
    return WebRequestCommon.typeMatches(policyType, filter.types) &&
           WebRequestCommon.urlMatches(uri, filter.urls);
  },

  runChannelListener(channel, loadContext, kind) {
    let listeners = this.listeners[kind];
    let browser = loadContext ? loadContext.topFrameElement : null;
    let loadInfo = channel.loadInfo;
    let policyType = loadInfo ?
                     loadInfo.externalContentPolicyType :
                     Ci.nsIContentPolicy.TYPE_OTHER;

    let requestHeaders;
    let responseHeaders;

    let includeStatus = kind == "headersReceived" || kind == "onStart" || kind == "onStop";

    for (let [callback, opts] of listeners.entries()) {
      if (!this.shouldRunListener(policyType, channel.URI, opts.filter)) {
        continue;
      }

      let data = {
        url: channel.URI.spec,
        method: channel.requestMethod,
        browser: browser,
        type: WebRequestCommon.typeForPolicyType(policyType),
        windowId: loadInfo ? loadInfo.outerWindowID : 0,
        parentWindowId: loadInfo ? loadInfo.parentOuterWindowID : 0,
      };
      if (opts.requestHeaders) {
        if (!requestHeaders) {
          requestHeaders = this.getHeaders(channel, "visitRequestHeaders");
        }
        data.requestHeaders = requestHeaders;
      }
      if (opts.responseHeaders) {
        if (!responseHeaders) {
          responseHeaders = this.getHeaders(channel, "visitResponseHeaders");
        }
        data.responseHeaders = responseHeaders;
      }
      if (includeStatus) {
        data.statusCode = channel.responseStatus;
      }

      let result = null;
      try {
        result = callback(data);
      } catch (e) {
        Cu.reportError(e);
      }

      if (!result || !opts.blocking) {
        return true;
      }
      if (result.cancel) {
        channel.cancel();
        return false;
      }
      if (result.redirectUrl) {
        channel.redirectTo(BrowserUtils.makeURI(result.redirectUrl));
        return false;
      }
      if (opts.requestHeaders && result.requestHeaders) {
        // Start by clearing everything.
        for (let {name} of requestHeaders) {
          channel.setRequestHeader(name, "", false);
        }

        for (let {name, value} of result.requestHeaders) {
          channel.setRequestHeader(name, value, false);
        }
      }
      if (opts.responseHeaders && result.responseHeaders) {
        // Start by clearing everything.
        for (let {name} of responseHeaders) {
          channel.setResponseHeader(name, "", false);
        }

        for (let {name, value} of result.responseHeaders) {
          channel.setResponseHeader(name, value, false);
        }
      }
    }

    return true;
  },

  modify(channel, topic, data) {
    let loadContext = this.getLoadContext(channel);

    if (this.runChannelListener(channel, loadContext, "modify")) {
      this.runChannelListener(channel, loadContext, "afterModify");
    }
  },

  examine(channel, topic, data) {
    let loadContext = this.getLoadContext(channel);

    if (this.listeners.onStart.size || this.listeners.onStop.size) {
      if (channel instanceof Components.interfaces.nsITraceableChannel) {
        let listener = new StartStopListener(this, loadContext);
        let orig = channel.setNewListener(listener);
        listener.orig = orig;
      }
    }

    this.runChannelListener(channel, loadContext, "headersReceived");
  },

  onStartRequest(channel, loadContext) {
    this.runChannelListener(channel, loadContext, "onStart");
  },

  onStopRequest(channel, loadContext) {
    this.runChannelListener(channel, loadContext, "onStop");
  },
};

var onBeforeRequest = {
  addListener(callback, filter = null, opt_extraInfoSpec = null) {
    // FIXME: Add requestBody support.
    let opts = parseExtra(opt_extraInfoSpec, ["blocking"]);
    opts.filter = parseFilter(filter);
    ContentPolicyManager.addListener(callback, opts);
  },

  removeListener(callback) {
    ContentPolicyManager.removeListener(callback);
  },
};

var onBeforeSendHeaders = {
  addListener(callback, filter = null, opt_extraInfoSpec = null) {
    let opts = parseExtra(opt_extraInfoSpec, ["requestHeaders", "blocking"]);
    opts.filter = parseFilter(filter);
    HttpObserverManager.addListener("modify", callback, opts);
  },

  removeListener(callback) {
    HttpObserverManager.removeListener("modify", callback);
  },
};

var onSendHeaders = {
  addListener(callback, filter = null, opt_extraInfoSpec = null) {
    let opts = parseExtra(opt_extraInfoSpec, ["requestHeaders"]);
    opts.filter = parseFilter(filter);
    HttpObserverManager.addListener("afterModify", callback, opts);
  },

  removeListener(callback) {
    HttpObserverManager.removeListener("afterModify", callback);
  },
};

var onHeadersReceived = {
  addListener(callback, filter = null, opt_extraInfoSpec = null) {
    let opts = parseExtra(opt_extraInfoSpec, ["blocking", "responseHeaders"]);
    opts.filter = parseFilter(filter);
    HttpObserverManager.addListener("headersReceived", callback, opts);
  },

  removeListener(callback) {
    HttpObserverManager.removeListener("headersReceived", callback);
  },
};

var onResponseStarted = {
  addListener(callback, filter = null, opt_extraInfoSpec = null) {
    let opts = parseExtra(opt_extraInfoSpec, ["responseHeaders"]);
    opts.filter = parseFilter(filter);
    HttpObserverManager.addListener("onStart", callback, opts);
  },

  removeListener(callback) {
    HttpObserverManager.removeListener("onStart", callback);
  },
};

var onCompleted = {
  addListener(callback, filter = null, opt_extraInfoSpec = null) {
    let opts = parseExtra(opt_extraInfoSpec, ["responseHeaders"]);
    opts.filter = parseFilter(filter);
    HttpObserverManager.addListener("onStop", callback, opts);
  },

  removeListener(callback) {
    HttpObserverManager.removeListener("onStop", callback);
  },
};

var WebRequest = {
  // Handled via content policy.
  onBeforeRequest: onBeforeRequest,

  // http-on-modify observer.
  onBeforeSendHeaders: onBeforeSendHeaders,

  // http-on-modify observer.
  onSendHeaders: onSendHeaders,

  // http-on-examine-*observer.
  onHeadersReceived: onHeadersReceived,

  // OnStartRequest channel listener.
  onResponseStarted: onResponseStarted,

  // OnStopRequest channel listener.
  onCompleted: onCompleted,
};

Services.ppmm.loadProcessScript("resource://gre/modules/WebRequestContent.js", true);
