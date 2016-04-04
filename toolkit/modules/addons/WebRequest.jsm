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

const {nsIHttpActivityObserver, nsISocketTransport} = Ci;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
                                  "resource://gre/modules/BrowserUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebRequestCommon",
                                  "resource://gre/modules/WebRequestCommon.jsm");

function attachToChannel(channel, key, data) {
  if (channel instanceof Ci.nsIWritablePropertyBag2) {
    let wrapper = {wrappedJSObject: data};
    channel.setPropertyAsInterface(key, wrapper);
  }
  return data;
}

function extractFromChannel(channel, key) {
  if (channel instanceof Ci.nsIPropertyBag2 && channel.hasKey(key)) {
    let data = channel.get(key);
    return data && data.wrappedJSObject;
  }
  return null;
}

function getData(channel) {
  const key = "mozilla.webRequest.data";
  return extractFromChannel(channel, key) || attachToChannel(channel, key, {});
}

var RequestId = {
  count: 1,
  create(channel = null) {
    let id = (this.count++).toString();
    if (channel) {
      getData(channel).requestId = id;
    }
    return id;
  },

  get(channel) {
    return channel && getData(channel).requestId || this.create(channel);
  },
};

function runLater(job) {
  Services.tm.currentThread.dispatch(job, Ci.nsIEventTarget.DISPATCH_NORMAL);
}

function parseFilter(filter) {
  if (!filter) {
    filter = {};
  }

  // FIXME: Support windowId filtering.
  return {urls: filter.urls || null, types: filter.types || null};
}

function parseExtra(extra, allowed = []) {
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

function mergeStatus(data, channel, event) {
  try {
    data.statusCode = channel.responseStatus;
    let statusText = channel.responseStatusText;
    let maj = {};
    let min = {};
    channel.QueryInterface(Ci.nsIHttpChannelInternal).getResponseVersion(maj, min);
    data.statusLine = `HTTP/${maj.value}.${min.value} ${data.statusCode} ${statusText}`;
  } catch (e) {
    // NS_ERROR_NOT_AVAILABLE might be thrown if it's an internal redirect, happening before
    // any actual HTTP traffic. Otherwise, let's report.
    if (event !== "onRedirect" || e.result !== Cr.NS_ERROR_NOT_AVAILABLE) {
      Cu.reportError(`webRequest Error: ${e} trying to merge status in ${event}@${channel.name}`);
    }
  }
}

var HttpObserverManager;

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
      let listenerKind = "onStop";
      let data = {
        url: msg.data.url,
        windowId: msg.data.windowId,
        parentWindowId: msg.data.parentWindowId,
        type: msg.data.type,
        browser: browser,
        requestId: RequestId.create(),
      };
      try {
        response = callback(data);
        if (response) {
          if (response.cancel) {
            listenerKind = "onError";
            data.error = "NS_ERROR_ABORT";
            return {cancel: true};
          }
          // FIXME: Need to handle redirection here (for non-HTTP URIs only)
        }
      } catch (e) {
        Cu.reportError(e);
      } finally {
        runLater(() => this.runChannelListener(listenerKind, data));
      }
    }

    return {};
  },

  runChannelListener(kind, data) {
    let listeners = HttpObserverManager.listeners[kind];
    let uri = BrowserUtils.makeURI(data.url);
    let policyType = data.type;
    for (let [callback, opts] of listeners.entries()) {
      if (!HttpObserverManager.shouldRunListener(policyType, uri, opts.filter)) {
        continue;
      }
      callback(data);
    }
  },

  addListener(callback, opts) {
    // Clone opts, since we're going to modify them for IPC.
    opts = Object.assign({}, opts);
    let id = this.nextId++;
    opts.id = id;
    if (opts.filter.urls) {
      opts.filter = Object.assign({}, opts.filter);
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

var ChannelEventSink = {
  _classDescription: "WebRequest channel event sink",
  _classID: Components.ID("115062f8-92f1-11e5-8b7f-080027b0f7ec"),
  _contractID: "@mozilla.org/webrequest/channel-event-sink;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannelEventSink,
                                         Ci.nsIFactory]),

  init() {
    Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
      .registerFactory(this._classID, this._classDescription, this._contractID, this);
  },

  register() {
    let catMan = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
    catMan.addCategoryEntry("net-channel-event-sinks", this._contractID, this._contractID, false, true);
  },

  unregister() {
    let catMan = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
    catMan.deleteCategoryEntry("net-channel-event-sinks", this._contractID, false);
  },

  // nsIChannelEventSink implementation
  asyncOnChannelRedirect(oldChannel, newChannel, flags, redirectCallback) {
    runLater(() => redirectCallback.onRedirectVerifyCallback(Cr.NS_OK));
    try {
      HttpObserverManager.onChannelReplaced(oldChannel, newChannel);
    } catch (e) {
      // we don't wanna throw: it would abort the redirection
    }
  },

  // nsIFactory implementation
  createInstance(outer, iid) {
    if (outer) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(iid);
  },
};

ChannelEventSink.init();

HttpObserverManager = {
  modifyInitialized: false,
  examineInitialized: false,
  redirectInitialized: false,
  activityInitialized: false,
  needTracing: false,

  listeners: {
    opening: new Map(),
    modify: new Map(),
    afterModify: new Map(),
    headersReceived: new Map(),
    onRedirect: new Map(),
    onStart: new Map(),
    onError: new Map(),
    onStop: new Map(),
  },

  get activityDistributor() {
    return Cc["@mozilla.org/network/http-activity-distributor;1"].getService(Ci.nsIHttpActivityDistributor);
  },

  addOrRemove() {
    let needModify = this.listeners.opening.size || this.listeners.modify.size || this.listeners.afterModify.size;
    if (needModify && !this.modifyInitialized) {
      this.modifyInitialized = true;
      Services.obs.addObserver(this, "http-on-modify-request", false);
    } else if (!needModify && this.modifyInitialized) {
      this.modifyInitialized = false;
      Services.obs.removeObserver(this, "http-on-modify-request");
    }
    this.needTracing = this.listeners.onStart.size ||
                       this.listeners.onError.size ||
                       this.listeners.onStop.size;

    let needExamine = this.needTracing ||
                      this.listeners.headersReceived.size;

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

    let needRedirect = this.listeners.onRedirect.size;
    if (needRedirect && !this.redirectInitialized) {
      this.redirectInitialized = true;
      ChannelEventSink.register();
    } else if (!needRedirect && this.redirectInitialized) {
      this.redirectInitialized = false;
      ChannelEventSink.unregister();
    }

    let needActivity = this.listeners.onError.size;
    if (needActivity && !this.activityInitialized) {
      this.activityInitialized = true;
      this.activityDistributor.addObserver(this);
    } else if (!needActivity && this.activityInitialized) {
      this.activityInitialized = false;
      this.activityDistributor.removeObserver(this);
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

  getHeaders(channel, method, event) {
    let headers = [];
    let visitor = {
      visitHeader(name, value) {
        headers.push({name, value});
      },

      QueryInterface: XPCOMUtils.generateQI([Ci.nsIHttpHeaderVisitor,
                                             Ci.nsISupports]),
    };

    try {
      channel[method](visitor);
    } catch (e) {
      Cu.reportError(`webRequest Error: ${e} trying to perform ${method} in ${event}@${channel.name}`);
    }
    return headers;
  },

  replaceHeaders(headers, originalNames, setHeader) {
    let failures = new Set();
    // Start by clearing everything.
    for (let name of originalNames) {
      try {
        setHeader(name, "");
      } catch (e) {
        // Let's collect physiological failures in order
        // to know what is worth reporting.
        failures.add(name);
      }
    }
    try {
      for (let {name, value, binaryValue} of headers) {
        try {
          if (Array.isArray(binaryValue)) {
            value = String.fromCharCode.apply(String, binaryValue);
          }
          setHeader(name, value);
        } catch (e) {
          if (!failures.has(name)) {
            Cu.reportError(e);
          }
        }
      }
    } catch (e) {
      Cu.reportError(e);
    }
  },

  observe(subject, topic, data) {
    let channel = subject.QueryInterface(Ci.nsIHttpChannel);
    switch (topic) {
      case "http-on-modify-request":
        this.modify(channel, topic, data);
        break;
      case "http-on-examine-response":
      case "http-on-examine-cached-response":
      case "http-on-examine-merged-response":
        this.examine(channel, topic, data);
        break;
    }
  },

  // We map activity values with tentative error names, e.g. "STATUS_RESOLVING" => "NS_ERROR_NET_ON_RESOLVING".
  get activityErrorsMap() {
    let prefix = /^(?:ACTIVITY_SUBTYPE_|STATUS_)/;
    let map = new Map();
    for (let iface of [nsIHttpActivityObserver, nsISocketTransport]) {
      for (let c of Object.keys(iface).filter(name => prefix.test(name))) {
        map.set(iface[c], c.replace(prefix, "NS_ERROR_NET_ON_"));
      }
    }
    delete this.activityErrorsMap;
    this.activityErrorsMap = map;
    return this.activityErrorsMap;
  },
  GOOD_LAST_ACTIVITY: nsIHttpActivityObserver.ACTIVITY_SUBTYPE_RESPONSE_HEADER,
  observeActivity(channel, activityType, activitySubtype /* , aTimestamp, aExtraSizeData, aExtraStringData */) {
    let channelData = getData(channel);
    let lastActivity = channelData.lastActivity || 0;
    if (activitySubtype === nsIHttpActivityObserver.ACTIVITY_SUBTYPE_RESPONSE_COMPLETE &&
        lastActivity && lastActivity !== this.GOOD_LAST_ACTIVITY) {
      let loadContext = this.getLoadContext(channel);
      if (!this.errorCheck(channel, loadContext, channelData)) {
        this.runChannelListener(channel, loadContext, "onError",
                                {error: this.activityErrorsMap.get(lastActivity) ||
                                        `NS_ERROR_NET_UNKNOWN_${lastActivity}`});
      }
    } else if (lastActivity !== this.GOOD_LAST_ACTIVITY &&
               lastActivity !== nsIHttpActivityObserver.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE) {
      channelData.lastActivity = activitySubtype;
    }
  },

  shouldRunListener(policyType, uri, filter) {
    return WebRequestCommon.typeMatches(policyType, filter.types) &&
           WebRequestCommon.urlMatches(uri, filter.urls);
  },

  get resultsMap() {
    delete this.resultsMap;
    this.resultsMap = new Map(Object.keys(Cr).map(name => [Cr[name], name]));
    return this.resultsMap;
  },
  maybeError(channel, extraData = null, channelData = null) {
    if (!(extraData && extraData.error)) {
      if (!Components.isSuccessCode(channel.status)) {
        extraData = {error: this.resultsMap.get(channel.status)};
      }
    }
    return extraData;
  },
  errorCheck(channel, loadContext, channelData = null) {
    let errorData = this.maybeError(channel, null, channelData);
    if (errorData) {
      this.runChannelListener(channel, loadContext, "onError", errorData);
    }
    return errorData;
  },

  runChannelListener(channel, loadContext, kind, extraData = null) {
    if (this.activityInitialized) {
      let channelData = getData(channel);
      if (kind === "onError") {
        if (channelData.errorNotified) {
          return false;
        }
        channelData.errorNotified = true;
      } else if (this.errorCheck(channel, loadContext, channelData)) {
        return false;
      }
    }
    let listeners = this.listeners[kind];
    let browser = loadContext ? loadContext.topFrameElement : null;
    let loadInfo = channel.loadInfo;
    let policyType = loadInfo ?
                     loadInfo.externalContentPolicyType :
                     Ci.nsIContentPolicy.TYPE_OTHER;

    let requestHeaderNames;
    let responseHeaderNames;

    let includeStatus = kind === "headersReceived" ||
                        kind === "onRedirect" ||
                        kind === "onStart" ||
                        kind === "onStop";

    for (let [callback, opts] of listeners.entries()) {
      if (!this.shouldRunListener(policyType, channel.URI, opts.filter)) {
        continue;
      }

      let data = {
        requestId: RequestId.get(channel),
        url: channel.URI.spec,
        method: channel.requestMethod,
        browser: browser,
        type: WebRequestCommon.typeForPolicyType(policyType),
        windowId: loadInfo ? loadInfo.outerWindowID : 0,
        parentWindowId: loadInfo ? loadInfo.parentOuterWindowID : 0,
      };

      let httpChannel = channel.QueryInterface(Ci.nsIHttpChannelInternal);
      try {
        data.ip = httpChannel.remoteAddress;
      } catch (e) {
        // The remoteAddress getter throws if the address is unavailable,
        // but ip is an optional property so just ignore the exception.
      }

      if (extraData) {
        Object.assign(data, extraData);
      }
      if (opts.requestHeaders) {
        data.requestHeaders = this.getHeaders(channel, "visitRequestHeaders", kind);
        requestHeaderNames = data.requestHeaders.map(h => h.name);
      }
      if (opts.responseHeaders) {
        data.responseHeaders = this.getHeaders(channel, "visitResponseHeaders", kind);
        responseHeaderNames = data.responseHeaders.map(h => h.name);
      }
      if (includeStatus) {
        mergeStatus(data, channel, kind);
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
        channel.cancel(Cr.NS_ERROR_ABORT);
        this.errorCheck(channel, loadContext);
        return false;
      }
      if (result.redirectUrl) {
        channel.redirectTo(BrowserUtils.makeURI(result.redirectUrl));
        return false;
      }
      if (opts.requestHeaders && result.requestHeaders) {
        this.replaceHeaders(
          result.requestHeaders, requestHeaderNames,
          (name, value) => channel.setRequestHeader(name, value, false)
        );
      }
      if (opts.responseHeaders && result.responseHeaders) {
        this.replaceHeaders(
          result.responseHeaders, responseHeaderNames,
          (name, value) => channel.setResponseHeader(name, value, false)
        );
      }
    }

    return true;
  },

  modify(channel, topic, data) {
    let loadContext = this.getLoadContext(channel);

    if (this.runChannelListener(channel, loadContext, "opening") &&
        this.runChannelListener(channel, loadContext, "modify")) {
      this.runChannelListener(channel, loadContext, "afterModify");
    }
  },

  examine(channel, topic, data) {
    let loadContext = this.getLoadContext(channel);

    if (this.needTracing) {
      if (channel instanceof Ci.nsITraceableChannel) {
        let responseStatus = channel.responseStatus;
        // skip redirections, https://bugzilla.mozilla.org/show_bug.cgi?id=728901#c8
        if (responseStatus < 300 || responseStatus >= 400) {
          let listener = new StartStopListener(this, loadContext);
          let orig = channel.setNewListener(listener);
          listener.orig = orig;
        }
      }
    }

    this.runChannelListener(channel, loadContext, "headersReceived");
  },

  onChannelReplaced(oldChannel, newChannel) {
    this.runChannelListener(oldChannel, this.getLoadContext(oldChannel),
                            "onRedirect", {redirectUrl: newChannel.URI.spec});
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
    HttpObserverManager.addListener("opening", callback, opts);
  },

  removeListener(callback) {
    HttpObserverManager.removeListener("opening", callback);
    ContentPolicyManager.removeListener(callback);
  },
};

function HttpEvent(internalEvent, options) {
  this.internalEvent = internalEvent;
  this.options = options;
}

HttpEvent.prototype = {
  addListener(callback, filter = null, opt_extraInfoSpec = null) {
    let opts = parseExtra(opt_extraInfoSpec, this.options);
    opts.filter = parseFilter(filter);
    HttpObserverManager.addListener(this.internalEvent, callback, opts);
  },

  removeListener(callback) {
    HttpObserverManager.removeListener(this.internalEvent, callback);
  },
};

var onBeforeSendHeaders = new HttpEvent("modify", ["requestHeaders", "blocking"]);
var onSendHeaders = new HttpEvent("afterModify", ["requestHeaders"]);
var onHeadersReceived = new HttpEvent("headersReceived", ["blocking", "responseHeaders"]);
var onBeforeRedirect = new HttpEvent("onRedirect", ["responseHeaders"]);
var onResponseStarted = new HttpEvent("onStart", ["responseHeaders"]);
var onCompleted = new HttpEvent("onStop", ["responseHeaders"]);
var onErrorOccurred = new HttpEvent("onError");

var WebRequest = {
  // http-on-modify observer for HTTP(S), content policy for the other protocols (notably, data:)
  onBeforeRequest: onBeforeRequest,

  // http-on-modify observer.
  onBeforeSendHeaders: onBeforeSendHeaders,

  // http-on-modify observer.
  onSendHeaders: onSendHeaders,

  // http-on-examine-*observer.
  onHeadersReceived: onHeadersReceived,

  // nsIChannelEventSink.
  onBeforeRedirect: onBeforeRedirect,

  // OnStartRequest channel listener.
  onResponseStarted: onResponseStarted,

  // OnStopRequest channel listener.
  onCompleted: onCompleted,

  // nsIHttpActivityObserver.
  onErrorOccurred: onErrorOccurred,
};

Services.ppmm.loadProcessScript("resource://gre/modules/WebRequestContent.js", true);
