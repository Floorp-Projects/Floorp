/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WebRequest"];

/* exported WebRequest */

/* globals ChannelWrapper */

const { nsIHttpActivityObserver, nsISocketTransport } = Ci;

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ExtensionUtils: "resource://gre/modules/ExtensionUtils.jsm",
  WebRequestUpload: "resource://gre/modules/WebRequestUpload.jsm",
  SecurityInfo: "resource://gre/modules/SecurityInfo.jsm",
});

function runLater(job) {
  Services.tm.dispatchToMainThread(job);
}

function parseFilter(filter) {
  if (!filter) {
    filter = {};
  }

  // FIXME: Support windowId filtering.
  return {
    urls: filter.urls || null,
    types: filter.types || null,
    incognito: filter.incognito !== undefined ? filter.incognito : null,
  };
}

function parseExtra(extra, allowed = [], optionsObj = {}) {
  if (extra) {
    for (let ex of extra) {
      if (!allowed.includes(ex)) {
        throw new ExtensionUtils.ExtensionError(`Invalid option ${ex}`);
      }
    }
  }

  let result = Object.assign({}, optionsObj);
  for (let al of allowed) {
    if (extra && extra.includes(al)) {
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

    this.array = this.readHeaders();
  }

  getMap() {
    if (!this.map) {
      this.map = new Map();
      for (let header of this.array) {
        this.map.set(header.name.toLowerCase(), header);
      }
    }
    return this.map;
  }

  toArray() {
    return this.array;
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

      return (
        typeof header.value === "string" || Array.isArray(header.binaryValue)
      );
    });
  }

  applyChanges(headers, opts = {}) {
    if (!this.validateHeaders(headers)) {
      /* globals uneval */
      Cu.reportError(`Invalid header array: ${uneval(headers)}`);
      return;
    }

    let newHeaders = new Set(headers.map(({ name }) => name.toLowerCase()));

    // Remove missing headers.
    let origHeaders = this.getMap();
    for (let name of origHeaders.keys()) {
      if (!newHeaders.has(name)) {
        this.setHeader(name, "");
      }
    }

    // Set new or changed headers.  If there are multiple headers with the same
    // name (e.g. Set-Cookie), merge them, instead of having new values
    // overwrite previous ones.
    //
    // When the new value of a header is equal the existing value of the header
    // (e.g. the initial response set "Set-Cookie: examplename=examplevalue",
    // and an extension also added the header
    // "Set-Cookie: examplename=examplevalue") then the header value is not
    // re-set, but subsequent headers of the same type will be merged in.
    let headersAlreadySet = new Set();
    for (let { name, value, binaryValue } of headers) {
      if (binaryValue) {
        value = String.fromCharCode(...binaryValue);
      }

      let lowerCaseName = name.toLowerCase();
      let original = origHeaders.get(lowerCaseName);

      if (!original || value !== original.value) {
        let shouldMerge = headersAlreadySet.has(lowerCaseName);
        this.setHeader(name, value, shouldMerge, opts, lowerCaseName);
      }

      headersAlreadySet.add(lowerCaseName);
    }
  }
}

const checkRestrictedHeaderValue = (value, opts = {}) => {
  let uri = Services.io.newURI(`https://${value}/`);
  let { extension } = opts;

  if (extension && !extension.allowedOrigins.matches(uri)) {
    throw new Error(`Unable to set host header, url missing from permissions.`);
  }

  if (WebExtensionPolicy.isRestrictedURI(uri)) {
    throw new Error(`Unable to set host header to restricted url.`);
  }
};

class RequestHeaderChanger extends HeaderChanger {
  setHeader(name, value, merge, opts, lowerCaseName) {
    try {
      if (value && lowerCaseName === "host") {
        checkRestrictedHeaderValue(value, opts);
      }
      this.channel.setRequestHeader(name, value, merge);
    } catch (e) {
      Cu.reportError(new Error(`Error setting request header ${name}: ${e}`));
    }
  }

  readHeaders() {
    return this.channel.getRequestHeaders();
  }
}

class ResponseHeaderChanger extends HeaderChanger {
  setHeader(name, value, merge, opts, lowerCaseName) {
    try {
      this.channel.setResponseHeader(name, value, merge);
    } catch (e) {
      Cu.reportError(new Error(`Error setting response header ${name}: ${e}`));
    }
  }

  readHeaders() {
    return this.channel.getResponseHeaders();
  }
}

const MAYBE_CACHED_EVENTS = new Set([
  "onResponseStarted",
  "onHeadersReceived",
  "onBeforeRedirect",
  "onCompleted",
  "onErrorOccurred",
]);

const OPTIONAL_PROPERTIES = [
  "requestHeaders",
  "responseHeaders",
  "statusCode",
  "statusLine",
  "error",
  "redirectUrl",
  "requestBody",
  "scheme",
  "realm",
  "isProxy",
  "challenger",
  "proxyInfo",
  "ip",
  "frameAncestors",
  "urlClassification",
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

  if (this.urlClassification) {
    data.urlClassification = {
      firstParty: this.urlClassification.firstParty.filter(
        c => !c.startsWith("socialtracking_")
      ),
      thirdParty: this.urlClassification.thirdParty.filter(
        c => !c.startsWith("socialtracking_")
      ),
    };
  }

  return data;
}

var HttpObserverManager;

var nextFakeRequestId = 1;

var ContentPolicyManager = {
  policyData: new Map(),
  idMap: new Map(),
  nextId: 0,

  init() {
    Services.ppmm.initialProcessData.webRequestContentPolicies = this.policyData;

    Services.ppmm.addMessageListener("WebRequest:ShouldLoad", this);
    Services.mm.addMessageListener("WebRequest:ShouldLoad", this);
  },

  receiveMessage(msg) {
    let browser =
      ChromeUtils.getClassName(msg.target) == "XULFrameElement"
        ? msg.target
        : null;

    let requestId = `fakeRequest-${++nextFakeRequestId}`;
    let data = Object.assign(
      { requestId, browser, serialize: serializeRequestData },
      msg.data
    );

    this.runChannelListener("opening", data);
    runLater(() => this.runChannelListener("onStop", data));
  },

  shouldRunListener(policyType, url, opts) {
    let { filter } = opts;

    if (filter.types && !filter.types.includes(policyType)) {
      return false;
    }

    if (filter.urls && !filter.urls.matches(url)) {
      return false;
    }

    let { extension } = opts;
    if (extension && !extension.allowedOrigins.matches(url)) {
      return false;
    }

    return true;
  },

  runChannelListener(kind, data) {
    let listeners = HttpObserverManager.listeners[kind];
    for (let [callback, opts] of listeners.entries()) {
      if (this.shouldRunListener(data.type, data.url, opts)) {
        try {
          callback(data);
        } catch (e) {
          Cu.reportError(e);
        }
      }
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

    this.idMap.set(callback, id);
  },

  removeListener(callback) {
    let id = this.idMap.get(callback);
    Services.ppmm.broadcastAsyncMessage("WebRequest:RemoveContentPolicy", {
      id,
    });

    this.policyData.delete(id);
    this.idMap.delete(callback);
  },
};
ContentPolicyManager.init();

var ChannelEventSink = {
  _classDescription: "WebRequest channel event sink",
  _classID: Components.ID("115062f8-92f1-11e5-8b7f-080027b0f7ec"),
  _contractID: "@mozilla.org/webrequest/channel-event-sink;1",

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIChannelEventSink,
    Ci.nsIFactory,
  ]),

  init() {
    Components.manager
      .QueryInterface(Ci.nsIComponentRegistrar)
      .registerFactory(
        this._classID,
        this._classDescription,
        this._contractID,
        this
      );
  },

  register() {
    Services.catMan.addCategoryEntry(
      "net-channel-event-sinks",
      this._contractID,
      this._contractID,
      false,
      true
    );
  },

  unregister() {
    Services.catMan.deleteCategoryEntry(
      "net-channel-event-sinks",
      this._contractID,
      false
    );
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
    this.loadGroupCallbacks =
      channel.loadGroup && channel.loadGroup.notificationCallbacks;
    this.httpObserver = httpObserver;
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
    return this._getForwardedInterface(Ci.nsIAuthPromptProvider).getAuthPrompt(
      reason,
      iid
    );
  }

  // nsIAuthPrompt2 promptAuth
  promptAuth(channel, level, authInfo) {
    this._getForwardedInterface(Ci.nsIAuthPrompt2).promptAuth(
      channel,
      level,
      authInfo
    );
  }

  _getForwardPrompt(data) {
    let reason = data.isProxy
      ? Ci.nsIAuthPromptProvider.PROMPT_PROXY
      : Ci.nsIAuthPromptProvider.PROMPT_NORMAL;
    for (let callbacks of [
      this.notificationCallbacks,
      this.loadGroupCallbacks,
    ]) {
      try {
        return callbacks
          .getInterface(Ci.nsIAuthPromptProvider)
          .getAuthPrompt(reason, Ci.nsIAuthPrompt2);
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
    wrapper.authPromptCallback = authCredentials => {
      // The API allows for canceling the request, providing credentials or
      // doing nothing, so we do not provide a way to call onAuthCanceled.
      // Canceling the request will result in canceling the authentication.
      if (
        authCredentials &&
        typeof authCredentials.username === "string" &&
        typeof authCredentials.password === "string"
      ) {
        authInfo.username = authCredentials.username;
        authInfo.password = authCredentials.password;
        try {
          callback.onAuthAvailable(context, authInfo);
        } catch (e) {
          Cu.reportError(`webRequest onAuthAvailable failure ${e}`);
        }
        // At least one addon has responded, so we won't forward to the regular
        // prompt handlers.
        wrapper.authPromptForward = null;
        wrapper.authPromptCallback = null;
      }
    };

    this.httpObserver.runChannelListener(wrapper, "authRequired", data);

    return {
      QueryInterface: ChromeUtils.generateQI([Ci.nsICancelable]),
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

AuthRequestor.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIInterfaceRequestor",
  "nsIAuthPromptProvider",
  "nsIAuthPrompt2",
]);

HttpObserverManager = {
  openingInitialized: false,
  modifyInitialized: false,
  examineInitialized: false,
  redirectInitialized: false,
  activityInitialized: false,
  needTracing: false,
  hasRedirects: false,

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

  getWrapper(nativeChannel) {
    let wrapper = ChannelWrapper.get(nativeChannel);
    if (!wrapper._addedListeners) {
      /* eslint-disable mozilla/balanced-listeners */
      if (this.listeners.onError.size) {
        wrapper.addEventListener("error", this);
      }
      if (this.listeners.onStart.size) {
        wrapper.addEventListener("start", this);
      }
      if (this.listeners.onStop.size) {
        wrapper.addEventListener("stop", this);
      }
      /* eslint-enable mozilla/balanced-listeners */

      wrapper._addedListeners = true;
    }
    return wrapper;
  },

  get activityDistributor() {
    return Cc["@mozilla.org/network/http-activity-distributor;1"].getService(
      Ci.nsIHttpActivityDistributor
    );
  },

  addOrRemove() {
    let needOpening = this.listeners.opening.size;
    let needModify =
      this.listeners.modify.size || this.listeners.afterModify.size;
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

    let haveBlocking = Object.values(this.listeners).some(listeners =>
      Array.from(listeners.values()).some(listener => listener.blockingAllowed)
    );

    this.needTracing =
      this.listeners.onStart.size ||
      this.listeners.onError.size ||
      this.listeners.onStop.size ||
      haveBlocking;

    let needExamine =
      this.needTracing ||
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

    // If we have any listeners, we need the channelsink so the channelwrapper is
    // updated properly. Otherwise events for channels that are redirected will not
    // happen correctly.  If we have no listeners, shut it down.
    this.hasRedirects = this.listeners.onRedirect.size > 0;
    let needRedirect =
      this.hasRedirects || needExamine || needOpening || needModify;
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
    let channel = this.getWrapper(subject);
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
  observeActivity(
    nativeChannel,
    activityType,
    activitySubtype /* , aTimestamp, aExtraSizeData, aExtraStringData */
  ) {
    // Sometimes we get a NullHttpChannel, which implements
    // nsIHttpChannel but not nsIChannel.
    if (!(nativeChannel instanceof Ci.nsIChannel)) {
      return;
    }
    let channel = this.getWrapper(nativeChannel);

    let lastActivity = channel.lastActivity || 0;
    if (
      activitySubtype ===
        nsIHttpActivityObserver.ACTIVITY_SUBTYPE_RESPONSE_COMPLETE &&
      lastActivity &&
      lastActivity !== this.GOOD_LAST_ACTIVITY
    ) {
      // Make a trip through the event loop to make sure errors have a
      // chance to be processed before we fall back to a generic error
      // string.
      Services.tm.dispatchToMainThread(() => {
        channel.errorCheck();
        if (!channel.errorString) {
          this.runChannelListener(channel, "onError", {
            error:
              this.activityErrorsMap.get(lastActivity) ||
              `NS_ERROR_NET_UNKNOWN_${lastActivity}`,
          });
        }
      });
    } else if (
      lastActivity !== this.GOOD_LAST_ACTIVITY &&
      lastActivity !==
        nsIHttpActivityObserver.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE
    ) {
      channel.lastActivity = activitySubtype;
    }
  },

  getRequestData(channel, extraData, policy) {
    let originAttributes =
      channel.loadInfo && channel.loadInfo.originAttributes;
    let data = {
      requestId: String(channel.id),
      url: channel.finalURL,
      method: channel.method,
      browser: channel.browserElement,
      type: channel.type,
      fromCache: channel.fromCache,
      originAttributes,

      originUrl: channel.originURL || undefined,
      documentUrl: channel.documentURL || undefined,

      windowId: channel.windowId,
      parentWindowId: channel.parentWindowId,

      frameAncestors: channel.frameAncestors || undefined,

      ip: channel.remoteAddress,

      proxyInfo: channel.proxyInfo,

      serialize: serializeRequestData,
    };

    // We're limiting access to
    // urlClassification while the feature is further fleshed out.
    if (policy && policy.extension.isPrivileged) {
      data.urlClassification = channel.urlClassification;
    }

    return Object.assign(data, extraData);
  },

  handleEvent(event) {
    let channel = event.currentTarget;
    switch (event.type) {
      case "error":
        this.runChannelListener(channel, "onError", {
          error: channel.errorString,
        });
        break;
      case "start":
        this.runChannelListener(channel, "onStart");
        break;
      case "stop":
        this.runChannelListener(channel, "onStop");
        break;
    }
  },

  STATUS_TYPES: new Set([
    "headersReceived",
    "authRequired",
    "onRedirect",
    "onStart",
    "onStop",
  ]),
  FILTER_TYPES: new Set([
    "opening",
    "modify",
    "afterModify",
    "headersReceived",
    "authRequired",
    "onRedirect",
  ]),

  runChannelListener(channel, kind, extraData = null) {
    let handlerResults = [];
    let requestHeaders;
    let responseHeaders;

    try {
      if (kind !== "onError" && channel.errorString) {
        return;
      }

      let registerFilter = this.FILTER_TYPES.has(kind);
      let commonData = null;
      let requestBody;
      this.listeners[kind].forEach((opts, callback) => {
        if (!channel.matches(opts.filter, opts.extension, extraData)) {
          return;
        }

        if (!commonData) {
          commonData = this.getRequestData(channel, extraData, opts.extension);
          if (this.STATUS_TYPES.has(kind)) {
            commonData.statusCode = channel.statusCode;
            commonData.statusLine = channel.statusLine;
          }
        }
        let data = Object.create(commonData);

        if (registerFilter && opts.blocking && opts.extension) {
          data.registerTraceableChannel = (extension, remoteTab) => {
            // `channel` is a ChannelWrapper, which contains the actual
            // underlying nsIChannel in `channel.channel`.  For startup events
            // that are held until the extension background page is started,
            // it is possible that the underlying channel can be closed and
            // cleaned up between the time the event occurred and the time
            // we reach this code.
            if (channel.channel) {
              channel.registerTraceableChannel(extension, remoteTab);
            }
          };
        }

        if (opts.requestHeaders) {
          requestHeaders = requestHeaders || new RequestHeaderChanger(channel);
          data.requestHeaders = requestHeaders.toArray();
        }

        if (opts.responseHeaders) {
          try {
            responseHeaders =
              responseHeaders || new ResponseHeaderChanger(channel);
            data.responseHeaders = responseHeaders.toArray();
          } catch (e) {
            /* headers may not be available on some redirects */
          }
        }

        if (opts.requestBody && channel.canModify) {
          requestBody =
            requestBody || WebRequestUpload.createRequestBody(channel.channel);
          data.requestBody = requestBody;
        }

        try {
          let result = callback(data);

          // isProxy is set during onAuth if the auth request is for a proxy.
          // We allow handling proxy auth regardless of canModify.
          if (
            (channel.canModify || data.isProxy) &&
            typeof result === "object" &&
            opts.blocking
          ) {
            handlerResults.push({ opts, result });
          }
        } catch (e) {
          Cu.reportError(e);
        }
      });
    } catch (e) {
      Cu.reportError(e);
    }

    return this.applyChanges(
      kind,
      channel,
      handlerResults,
      requestHeaders,
      responseHeaders
    );
  },

  async applyChanges(
    kind,
    channel,
    handlerResults,
    requestHeaders,
    responseHeaders
  ) {
    let shouldResume = !channel.suspended;

    try {
      for (let { opts, result } of handlerResults) {
        if (isThenable(result)) {
          channel.suspended = true;
          try {
            result = await result;
          } catch (e) {
            let error;

            if (e instanceof Error) {
              error = e;
            } else if (typeof e === "object" && e.message) {
              error = new Error(e.message, e.fileName, e.lineNumber);
            }

            Cu.reportError(error);
            continue;
          }
          if (!result || typeof result !== "object") {
            continue;
          }
        }

        if (
          kind === "authRequired" &&
          result.authCredentials &&
          channel.authPromptCallback
        ) {
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
          return;
        }

        if (result.redirectUrl) {
          try {
            channel.suspended = false;
            channel.redirectTo(Services.io.newURI(result.redirectUrl));
            // Web Extensions using the WebRequest API are allowed
            // to redirect a channel to a data: URI, hence we mark
            // the channel to let the redirect blocker know.
            channel.loadInfo.allowInsecureRedirectToDataURI = true;

            // Web Extentions using the WebRequest API are also allowed
            // to redirect a channel before any data has been send.
            // This means we dont have  "Access-Control-Allow-Origin"
            // information at that point so CORS checks would fail.
            // Since we trust the WebExtention, we mark the Channel
            // to skip the CORS check.
            channel.loadInfo.bypassCORSChecks = true;

            // Please note that this marking needs to happen after the
            // channel.redirectTo is called because the channel's
            // RedirectTo() implementation explicitly drops the flags
            // to avoid redirects not caused by the
            // Web Extension.
            return;
          } catch (e) {
            Cu.reportError(e);
          }
        }

        if (result.upgradeToSecure && kind === "opening") {
          try {
            channel.upgradeToSecure();
          } catch (e) {
            Cu.reportError(e);
          }
        }

        if (opts.requestHeaders && result.requestHeaders && requestHeaders) {
          requestHeaders.applyChanges(result.requestHeaders, opts);
        }

        if (opts.responseHeaders && result.responseHeaders && responseHeaders) {
          responseHeaders.applyChanges(result.responseHeaders, opts);
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
        channel.errorCheck();
      }
    } catch (e) {
      Cu.reportError(e);
    }

    // Only resume the channel if it was suspended by this call.
    if (shouldResume) {
      channel.suspended = false;
    }
  },

  shouldHookListener(listener, channel, extraData) {
    if (listener.size == 0) {
      return false;
    }

    for (let opts of listener.values()) {
      if (channel.matches(opts.filter, opts.extension, extraData)) {
        return true;
      }
    }
    return false;
  },

  examine(channel, topic, data) {
    if (this.listeners.headersReceived.size) {
      this.runChannelListener(channel, "headersReceived");
    }

    if (
      !channel.hasAuthRequestor &&
      this.shouldHookListener(this.listeners.authRequired, channel, {
        isProxy: true,
      })
    ) {
      channel.channel.notificationCallbacks = new AuthRequestor(
        channel.channel,
        this
      );
      channel.hasAuthRequestor = true;
    }
  },

  onChannelReplaced(oldChannel, newChannel) {
    let channel = this.getWrapper(oldChannel);

    // We want originalURI, this will provide a moz-ext rather than jar or file
    // uri on redirects.
    if (this.hasRedirects) {
      this.runChannelListener(channel, "onRedirect", {
        redirectUrl: newChannel.originalURI.spec,
      });
    }
    channel.channel = newChannel;
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

var onBeforeSendHeaders = new HttpEvent("modify", [
  "requestHeaders",
  "blocking",
]);
var onSendHeaders = new HttpEvent("afterModify", ["requestHeaders"]);
var onHeadersReceived = new HttpEvent("headersReceived", [
  "blocking",
  "responseHeaders",
]);
var onAuthRequired = new HttpEvent("authRequired", [
  "blocking",
  "responseHeaders",
]);
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

  getSecurityInfo: details => {
    let channel = ChannelWrapper.getRegisteredChannel(
      details.id,
      details.extension,
      details.remoteTab
    );
    if (channel) {
      return SecurityInfo.getSecurityInfo(channel.channel, details.options);
    }
  },
};

Services.ppmm.loadProcessScript(
  "resource://gre/modules/WebRequestContent.js",
  true
);
