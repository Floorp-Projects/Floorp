/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WebRequest"];

/* exported WebRequest */

/* globals ChannelWrapper */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

const {nsIHttpActivityObserver, nsISocketTransport} = Ci;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "NSSErrorsService",
                                   "@mozilla.org/nss_errors_service;1",
                                   "nsINSSErrorsService");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionUtils",
                                  "resource://gre/modules/ExtensionUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebRequestCommon",
                                  "resource://gre/modules/WebRequestCommon.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebRequestUpload",
                                  "resource://gre/modules/WebRequestUpload.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "webReqService",
                                   "@mozilla.org/addons/webrequest-service;1",
                                   "mozIWebRequestService");

XPCOMUtils.defineLazyGetter(this, "ExtensionError", () => ExtensionUtils.ExtensionError);

let WebRequestListener = Components.Constructor("@mozilla.org/webextensions/webRequestListener;1",
                                                "nsIWebRequestListener", "init");

function runLater(job) {
  Services.tm.dispatchToMainThread(job);
}

function parseFilter(filter) {
  if (!filter) {
    filter = {};
  }

  // FIXME: Support windowId filtering.
  return {urls: filter.urls || null, types: filter.types || null};
}

function parseExtra(extra, allowed = [], optionsObj = {}) {
  if (extra) {
    for (let ex of extra) {
      if (allowed.indexOf(ex) == -1) {
        throw new ExtensionError(`Invalid option ${ex}`);
      }
    }
  }

  let result = Object.assign({}, optionsObj);
  for (let al of allowed) {
    if (extra && extra.indexOf(al) != -1) {
      result[al] = true;
    }
  }
  return result;
}

function isThenable(value) {
  return value && typeof value === "object" && typeof value.then === "function";
}

class HeaderChanger {
  constructor(channel) {
    this.channel = channel;

    this.originalHeaders = new Map();
    for (let [name, value] of this.iterHeaders()) {
      this.originalHeaders.set(name.toLowerCase(), {name, value});
    }
  }

  toArray() {
    return Array.from(this.originalHeaders.values());
  }

  validateHeaders(headers) {
    // We should probably use schema validation for this.

    if (!Array.isArray(headers)) {
      return false;
    }

    return headers.every(header => {
      if (typeof header !== "object" || header === null) {
        return false;
      }

      if (typeof header.name !== "string") {
        return false;
      }

      return (typeof header.value === "string" ||
              Array.isArray(header.binaryValue));
    });
  }

  applyChanges(headers) {
    if (!this.validateHeaders(headers)) {
      /* globals uneval */
      Cu.reportError(`Invalid header array: ${uneval(headers)}`);
      return;
    }

    let newHeaders = new Set(headers.map(
      ({name}) => name.toLowerCase()));

    // Remove missing headers.
    for (let name of this.originalHeaders.keys()) {
      if (!newHeaders.has(name)) {
        this.setHeader(name, "");
      }
    }

    // Set new or changed headers.
    for (let {name, value, binaryValue} of headers) {
      if (binaryValue) {
        value = String.fromCharCode(...binaryValue);
      }
      let original = this.originalHeaders.get(name.toLowerCase());
      if (!original || value !== original.value) {
        this.setHeader(name, value);
      }
    }
  }
}

class RequestHeaderChanger extends HeaderChanger {
  setHeader(name, value) {
    try {
      this.channel.setRequestHeader(name, value);
    } catch (e) {
      Cu.reportError(new Error(`Error setting request header ${name}: ${e}`));
    }
  }

  iterHeaders() {
    return this.channel.getRequestHeaders().entries();
  }
}

class ResponseHeaderChanger extends HeaderChanger {
  setHeader(name, value) {
    try {
      if (name.toLowerCase() === "content-type" && value) {
        // The Content-Type header value can't be modified, so we
        // set the channel's content type directly, instead, and
        // record that we made the change for the sake of
        // subsequent observers.
        this.channel.contentType = value;
        this.channel._contentType = value;
      } else {
        this.channel.setResponseHeader(name, value);
      }
    } catch (e) {
      Cu.reportError(new Error(`Error setting response header ${name}: ${e}`));
    }
  }

  * iterHeaders() {
    for (let [name, value] of this.channel.getResponseHeaders()) {
      if (name.toLowerCase() === "content-type") {
        value = this.channel._contentType || value;
      }
      yield [name, value];
    }
  }
}

const MAYBE_CACHED_EVENTS = new Set([
  "onResponseStarted", "onBeforeRedirect", "onCompleted", "onErrorOccurred",
]);

const OPTIONAL_PROPERTIES = [
  "requestHeaders", "responseHeaders", "statusCode", "statusLine", "error", "redirectUrl",
  "requestBody", "scheme", "realm", "isProxy", "challenger", "proxyInfo", "ip", "frameAncestors",
];

function serializeRequestData(eventName) {
  let data = {
    requestId: this.requestId,
    url: this.url,
    originUrl: this.originUrl,
    documentUrl: this.documentUrl,
    method: this.method,
    type: this.type,
    timeStamp: Date.now(),
    frameId: this.windowId,
    parentFrameId: this.parentWindowId,
  };

  if (MAYBE_CACHED_EVENTS.has(eventName)) {
    data.fromCache = !!this.fromCache;
  }

  for (let opt of OPTIONAL_PROPERTIES) {
    if (typeof this[opt] !== "undefined") {
      data[opt] = this[opt];
    }
  }
  return data;
}

var HttpObserverManager;

var nextFakeRequestId = 1;

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

    let requestId = `fakeRequest-${++nextFakeRequestId}`;
    for (let id of msg.data.ids) {
      let callback = this.policies.get(id);
      if (!callback) {
        // It's possible that this listener has been removed and the
        // child hasn't learned yet.
        continue;
      }
      let response = null;
      let listenerKind = "onStop";
      let data = Object.assign({requestId, browser, serialize: serializeRequestData}, msg.data);
      data.URI = data.url;

      delete data.ids;
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
    let uri = Services.io.newURI(data.url);
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
      opts.filter.urls = opts.filter.urls.patterns.map(url => url.pattern);
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

function StartStopListener(manager, channel) {
  this.manager = manager;
  new WebRequestListener(this, channel.channel);
}

StartStopListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRequestObserver,
                                         Ci.nsIStreamListener]),

  onStartRequest: function(request, context) {
    this.manager.onStartRequest(ChannelWrapper.get(request));
  },

  onStopRequest(request, context, statusCode) {
    this.manager.onStopRequest(ChannelWrapper.get(request));
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

// nsIAuthPrompt2 implementation for onAuthRequired
class AuthRequestor {
  constructor(channel, httpObserver) {
    this.notificationCallbacks = channel.notificationCallbacks;
    this.loadGroupCallbacks = channel.loadGroup && channel.loadGroup.notificationCallbacks;
    this.httpObserver = httpObserver;
  }

  QueryInterface(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIInterfaceRequestor) ||
        iid.equals(Ci.nsIAuthPromptProvider) ||
        iid.equals(Ci.nsIAuthPrompt2)) {
      return this;
    }
    try {
      return this.notificationCallbacks.QueryInterface(iid);
    } catch (e) {}
    throw Cr.NS_ERROR_NO_INTERFACE;
  }

  getInterface(iid) {
    if (iid.equals(Ci.nsIAuthPromptProvider) || iid.equals(Ci.nsIAuthPrompt2)) {
      return this;
    }
    try {
      return this.notificationCallbacks.getInterface(iid);
    } catch (e) {}
    throw Cr.NS_ERROR_NO_INTERFACE;
  }

  _getForwardedInterface(iid) {
    try {
      return this.notificationCallbacks.getInterface(iid);
    } catch (e) {
      return this.loadGroupCallbacks.getInterface(iid);
    }
  }

  // nsIAuthPromptProvider getAuthPrompt
  getAuthPrompt(reason, iid) {
    // This should never get called without getInterface having been called first.
    if (iid.equals(Ci.nsIAuthPrompt2)) {
      return this;
    }
    return this._getForwardedInterface(Ci.nsIAuthPromptProvider).getAuthPrompt(reason, iid);
  }

  // nsIAuthPrompt2 promptAuth
  promptAuth(channel, level, authInfo) {
    this._getForwardedInterface(Ci.nsIAuthPrompt2).promptAuth(channel, level, authInfo);
  }

  _getForwardPrompt(data) {
    let reason = data.isProxy ? Ci.nsIAuthPromptProvider.PROMPT_PROXY : Ci.nsIAuthPromptProvider.PROMPT_NORMAL;
    for (let callbacks of [this.notificationCallbacks, this.loadGroupCallbacks]) {
      try {
        return callbacks.getInterface(Ci.nsIAuthPromptProvider).getAuthPrompt(reason, Ci.nsIAuthPrompt2);
      } catch (e) {}
      try {
        return callbacks.getInterface(Ci.nsIAuthPrompt2);
      } catch (e) {}
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }

  // nsIAuthPrompt2 asyncPromptAuth
  asyncPromptAuth(channel, callback, context, level, authInfo) {
    let wrapper = ChannelWrapper.get(channel);

    let uri = channel.URI;
    let proxyInfo;
    let isProxy = !!(authInfo.flags & authInfo.AUTH_PROXY);
    if (isProxy && channel instanceof Ci.nsIProxiedChannel) {
      proxyInfo = channel.proxyInfo;
    }
    let data = {
      scheme: authInfo.authenticationScheme,
      realm: authInfo.realm,
      isProxy,
      challenger: {
        host: proxyInfo ? proxyInfo.host : uri.host,
        port: proxyInfo ? proxyInfo.port : uri.port,
      },
    };

    // In the case that no listener provides credentials, we fallback to the
    // previously set callback class for authentication.
    wrapper.authPromptForward = () => {
      try {
        let prompt = this._getForwardPrompt(data);
        prompt.asyncPromptAuth(channel, callback, context, level, authInfo);
      } catch (e) {
        Cu.reportError(`webRequest asyncPromptAuth failure ${e}`);
        callback.onAuthCancelled(context, false);
      }
      wrapper.authPromptForward = null;
      wrapper.authPromptCallback = null;
    };
    wrapper.authPromptCallback = (authCredentials) => {
      // The API allows for canceling the request, providing credentials or
      // doing nothing, so we do not provide a way to call onAuthCanceled.
      // Canceling the request will result in canceling the authentication.
      if (authCredentials &&
          typeof authCredentials.username === "string" &&
          typeof authCredentials.password === "string") {
        authInfo.username = authCredentials.username;
        authInfo.password = authCredentials.password;
        try {
          callback.onAuthAvailable(context, authInfo);
        } catch (e) {
          Cu.reportError(`webRequest onAuthAvailable failure ${e}`);
        }
        // At least one addon has responded, so we wont forward to the regular
        // prompt handlers.
        wrapper.authPromptForward = null;
        wrapper.authPromptCallback = null;
      }
    };

    this.httpObserver.runChannelListener(wrapper, "authRequired", data);

    return {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
      cancel() {
        try {
          callback.onAuthCancelled(context, false);
        } catch (e) {
          Cu.reportError(`webRequest onAuthCancelled failure ${e}`);
        }
        wrapper.authPromptForward = null;
        wrapper.authPromptCallback = null;
      },
    };
  }
}

HttpObserverManager = {
  openingInitialized: false,
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
    authRequired: new Map(),
    onRedirect: new Map(),
    onStart: new Map(),
    onError: new Map(),
    onStop: new Map(),
  },

  get activityDistributor() {
    return Cc["@mozilla.org/network/http-activity-distributor;1"].getService(Ci.nsIHttpActivityDistributor);
  },

  addOrRemove() {
    let needOpening = this.listeners.opening.size;
    let needModify = this.listeners.modify.size || this.listeners.afterModify.size;
    if (needOpening && !this.openingInitialized) {
      this.openingInitialized = true;
      Services.obs.addObserver(this, "http-on-modify-request");
    } else if (!needOpening && this.openingInitialized) {
      this.openingInitialized = false;
      Services.obs.removeObserver(this, "http-on-modify-request");
    }
    if (needModify && !this.modifyInitialized) {
      this.modifyInitialized = true;
      Services.obs.addObserver(this, "http-on-before-connect");
    } else if (!needModify && this.modifyInitialized) {
      this.modifyInitialized = false;
      Services.obs.removeObserver(this, "http-on-before-connect");
    }

    let haveBlocking = Object.values(this.listeners)
                             .some(listeners => Array.from(listeners.values())
                                                     .some(listener => listener.blockingAllowed));

    this.needTracing = this.listeners.onStart.size ||
                       this.listeners.onError.size ||
                       this.listeners.onStop.size ||
                       haveBlocking;

    let needExamine = this.needTracing ||
                      this.listeners.headersReceived.size ||
                      this.listeners.authRequired.size;

    if (needExamine && !this.examineInitialized) {
      this.examineInitialized = true;
      Services.obs.addObserver(this, "http-on-examine-response");
      Services.obs.addObserver(this, "http-on-examine-cached-response");
      Services.obs.addObserver(this, "http-on-examine-merged-response");
    } else if (!needExamine && this.examineInitialized) {
      this.examineInitialized = false;
      Services.obs.removeObserver(this, "http-on-examine-response");
      Services.obs.removeObserver(this, "http-on-examine-cached-response");
      Services.obs.removeObserver(this, "http-on-examine-merged-response");
    }

    let needRedirect = this.listeners.onRedirect.size || haveBlocking;
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

  observe(subject, topic, data) {
    let channel = ChannelWrapper.get(subject);
    switch (topic) {
      case "http-on-modify-request":
        this.runChannelListener(channel, "opening");
        break;
      case "http-on-before-connect":
        this.runChannelListener(channel, "modify");
        break;
      case "http-on-examine-cached-response":
      case "http-on-examine-merged-response":
        channel.fromCache = true;
        // falls through
      case "http-on-examine-response":
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
  observeActivity(nativeChannel, activityType, activitySubtype /* , aTimestamp, aExtraSizeData, aExtraStringData */) {
    // Sometimes we get a NullHttpChannel, which implements
    // nsIHttpChannel but not nsIChannel.
    if (!(nativeChannel instanceof Ci.nsIChannel)) {
      return;
    }
    let channel = ChannelWrapper.get(nativeChannel);

    // StartStopListener has to be activated early in the request to catch
    // SSL connection issues which do not get reported via nsIHttpActivityObserver.
    if (activityType == nsIHttpActivityObserver.ACTIVITY_TYPE_HTTP_TRANSACTION &&
        activitySubtype == nsIHttpActivityObserver.ACTIVITY_SUBTYPE_REQUEST_HEADER) {
      this.attachStartStopListener(channel);
    }

    let lastActivity = channel.lastActivity || 0;
    if (activitySubtype === nsIHttpActivityObserver.ACTIVITY_SUBTYPE_RESPONSE_COMPLETE &&
        lastActivity && lastActivity !== this.GOOD_LAST_ACTIVITY) {
      if (!this.errorCheck(channel)) {
        this.runChannelListener(channel, "onError",
          {error: this.activityErrorsMap.get(lastActivity) ||
                  `NS_ERROR_NET_UNKNOWN_${lastActivity}`});
      }
    } else if (lastActivity !== this.GOOD_LAST_ACTIVITY &&
               lastActivity !== nsIHttpActivityObserver.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE) {
      channel.lastActivity = activitySubtype;
    }
  },

  shouldRunListener(policyType, uri, filter) {
    // force the protocol to be ws again.
    if (policyType == "websocket" && ["http", "https"].includes(uri.scheme)) {
      uri = Services.io.newURI(`ws${uri.spec.substring(4)}`);
    }

    if (filter.types && !filter.types.includes(policyType)) {
      return false;
    }

    return !filter.urls || filter.urls.matches(uri);
  },

  get resultsMap() {
    delete this.resultsMap;
    this.resultsMap = new Map(Object.keys(Cr).map(name => [Cr[name], name]));
    return this.resultsMap;
  },

  maybeError({channel}) {
    // FIXME: Move to ChannelWrapper.

    let {securityInfo} = channel;
    if (securityInfo instanceof Ci.nsITransportSecurityInfo) {
      if (NSSErrorsService.isNSSErrorCode(securityInfo.errorCode)) {
        let nsresult = NSSErrorsService.getXPCOMFromNSSError(securityInfo.errorCode);
        return {error: NSSErrorsService.getErrorMessage(nsresult)};
      }
    }

    if (!Components.isSuccessCode(channel.status)) {
      return {error: this.resultsMap.get(channel.status) || "NS_ERROR_NET_UNKNOWN"};
    }
    return null;
  },

  errorCheck(channel) {
    let errorData = this.maybeError(channel);
    if (errorData) {
      this.runChannelListener(channel, "onError", errorData);
    }
    return errorData;
  },

  getRequestData(channel, extraData) {
    let data = {
      requestId: String(channel.id),
      url: channel.finalURL,
      URI: channel.finalURI,
      method: channel.method,
      browser: channel.browserElement,
      type: channel.type,
      fromCache: channel.fromCache,

      originUrl: channel.originURL || undefined,
      documentUrl: channel.documentURL || undefined,
      originURI: channel.originURI,
      documentURI: channel.documentURI,
      isSystemPrincipal: channel.isSystemLoad,

      windowId: channel.windowId,
      parentWindowId: channel.parentWindowId,

      ip: channel.remoteAddress,

      proxyInfo: channel.proxyInfo,

      serialize: serializeRequestData,
    };

    try {
      let {frameAncestors} = channel;
      if (frameAncestors !== null) {
        data.frameAncestors = frameAncestors;
      }
    } catch (e) {}

    // force the protocol to be ws again.
    if (data.type == "websocket" && data.url.startsWith("http")) {
      data.url = `ws${data.url.substring(4)}`;
    }

    return Object.assign(data, extraData);
  },

  registerChannel(channel, opts) {
    if (!opts.blockingAllowed || !opts.addonId) {
      return;
    }

    if (!channel.registeredFilters) {
      channel.registeredFilters = new Map();
    } else if (channel.registeredFilters.has(opts.addonId)) {
      return;
    }

    let filter = webReqService.registerTraceableChannel(
      channel.id,
      channel.channel,
      opts.addonId,
      opts.tabParent);

    channel.registeredFilters.set(opts.addonId, filter);
  },

  destroyFilters(channel) {
    let filters = channel.registeredFilters || new Map();
    for (let [key, filter] of filters.entries()) {
      filter.destruct();
      filters.delete(key);
    }
  },

  runChannelListener(channel, kind, extraData = null) {
    let handlerResults = [];
    let requestHeaders;
    let responseHeaders;

    try {
      if (this.activityInitialized) {
        if (kind === "onError") {
          this.destroyFilters(channel);
          if (channel.errorNotified) {
            return;
          }
          channel.errorNotified = true;
        } else if (this.errorCheck(channel)) {
          return;
        }
      }

      let includeStatus = ["headersReceived", "authRequired", "onRedirect", "onStart", "onStop"].includes(kind);
      let registerFilter = ["opening", "modify", "afterModify", "headersReceived", "authRequired", "onRedirect"].includes(kind);

      let commonData = null;
      let uri = channel.finalURI;
      let requestBody;
      for (let [callback, opts] of this.listeners[kind].entries()) {
        if (!this.shouldRunListener(channel.type, uri, opts.filter)) {
          continue;
        }

        if (!commonData) {
          commonData = this.getRequestData(channel, extraData);
          if (includeStatus) {
            commonData.statusCode = channel.statusCode;
            commonData.statusLine = channel.statusLine;
          }
        }
        let data = Object.assign({}, commonData);

        if (registerFilter && opts.blocking) {
          this.registerChannel(channel, opts);
        }

        if (opts.requestHeaders) {
          requestHeaders = requestHeaders || new RequestHeaderChanger(channel);
          data.requestHeaders = requestHeaders.toArray();
        }

        if (opts.responseHeaders) {
          responseHeaders = responseHeaders || new ResponseHeaderChanger(channel);
          data.responseHeaders = responseHeaders.toArray();
        }

        if (opts.requestBody) {
          requestBody = requestBody || WebRequestUpload.createRequestBody(channel.channel);
          data.requestBody = requestBody;
        }

        try {
          let result = callback(data);

          // isProxy is set during onAuth if the auth request is for a proxy.
          // We allow handling proxy auth regardless of canModify.
          if ((channel.canModify || data.isProxy) && typeof result === "object" && opts.blocking) {
            handlerResults.push({opts, result});
          }
        } catch (e) {
          Cu.reportError(e);
        }
      }
    } catch (e) {
      Cu.reportError(e);
    }

    return this.applyChanges(kind, channel, handlerResults, requestHeaders, responseHeaders);
  },

  async applyChanges(kind, channel, handlerResults, requestHeaders, responseHeaders) {
    let shouldResume = !channel.suspended;

    try {
      for (let {opts, result} of handlerResults) {
        if (isThenable(result)) {
          channel.suspended = true;
          try {
            result = await result;
          } catch (e) {
            Cu.reportError(e);
            continue;
          }
          if (!result || typeof result !== "object") {
            continue;
          }
        }

        if (kind === "authRequired" && result.authCredentials && channel.authPromptCallback) {
          channel.authPromptCallback(result.authCredentials);
        }

        // We allow proxy auth to cancel or handle authCredentials regardless of
        // canModify, but ensure we do nothing else.
        if (!channel.canModify) {
          continue;
        }

        if (result.cancel) {
          channel.suspended = false;
          channel.cancel(Cr.NS_ERROR_ABORT);

          this.errorCheck(channel);
          return;
        }

        if (result.redirectUrl) {
          try {
            channel.suspended = false;
            channel.redirectTo(Services.io.newURI(result.redirectUrl));
            return;
          } catch (e) {
            Cu.reportError(e);
          }
        }

        if (opts.requestHeaders && result.requestHeaders && requestHeaders) {
          requestHeaders.applyChanges(result.requestHeaders);
        }

        if (opts.responseHeaders && result.responseHeaders && responseHeaders) {
          responseHeaders.applyChanges(result.responseHeaders);
        }
      }

      // If a listener did not cancel the request or provide credentials, we
      // forward the auth request to the base handler.
      if (kind === "authRequired" && channel.authPromptForward) {
        channel.authPromptForward();
      }

      if (kind === "modify" && this.listeners.afterModify.size) {
        await this.runChannelListener(channel, "afterModify");
      } else if (kind !== "onError") {
        this.errorCheck(channel);
      }
    } catch (e) {
      Cu.reportError(e);
    }

    // Only resume the channel if it was suspended by this call.
    if (shouldResume) {
      channel.suspended = false;
    }
  },

  shouldHookListener(listener, channel) {
    if (listener.size == 0) {
      return false;
    }

    for (let opts of listener.values()) {
      if (this.shouldRunListener(channel.type, channel.finalURI, opts.filter)) {
        return true;
      }
    }
    return false;
  },

  attachStartStopListener(channel) {
    // Check whether we've already added a listener to this channel,
    // so we don't wind up chaining multiple listeners.
    if (!this.needTracing || channel.hasListener ||
        !(channel.channel instanceof Ci.nsITraceableChannel)) {
      return;
    }

    // skip redirections, https://bugzilla.mozilla.org/show_bug.cgi?id=728901#c8
    let {statusCode} = channel;
    if (statusCode < 300 || statusCode >= 400) {
      new StartStopListener(this, channel);
      channel.hasListener = true;
    }
  },

  examine(channel, topic, data) {
    this.attachStartStopListener(channel);

    if (this.listeners.headersReceived.size) {
      this.runChannelListener(channel, "headersReceived");
    }

    if (!channel.hasAuthRequestor && this.shouldHookListener(this.listeners.authRequired, channel)) {
      channel.channel.notificationCallbacks = new AuthRequestor(channel.channel, this);
      channel.hasAuthRequestor = true;
    }
  },

  onChannelReplaced(oldChannel, newChannel) {
    let channel = ChannelWrapper.get(oldChannel);

    // We want originalURI, this will provide a moz-ext rather than jar or file
    // uri on redirects.
    this.destroyFilters(channel);
    this.runChannelListener(channel, "onRedirect", {redirectUrl: newChannel.originalURI.spec});
    channel.channel = newChannel;
  },

  onStartRequest(channel) {
    this.destroyFilters(channel);
    this.runChannelListener(channel, "onStart");
  },

  onStopRequest(channel) {
    this.runChannelListener(channel, "onStop");
  },
};

var onBeforeRequest = {
  allowedOptions: ["blocking", "requestBody"],

  addListener(callback, filter = null, options = null, optionsObject = null) {
    let opts = parseExtra(options, this.allowedOptions);
    opts.filter = parseFilter(filter);
    ContentPolicyManager.addListener(callback, opts);

    opts = Object.assign({}, opts, optionsObject);
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
  addListener(callback, filter = null, options = null, optionsObject = null) {
    let opts = parseExtra(options, this.options, optionsObject);
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
var onAuthRequired = new HttpEvent("authRequired", ["blocking", "responseHeaders"]);
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

  // http-on-examine-*observer.
  onAuthRequired: onAuthRequired,

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
