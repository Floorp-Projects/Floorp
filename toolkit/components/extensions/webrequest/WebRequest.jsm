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
    //
    // Multiple addons will be able to provide modifications to any headers
    // listed in the default set.
    let headersAlreadySet = new Set(["content-security-policy"]);
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
  let { policy } = opts;

  if (policy && !policy.allowedOrigins.matches(uri)) {
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
  "requestSize",
  "responseSize",
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
    thirdParty: this.thirdParty,
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
      throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
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
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
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
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
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

    this.httpObserver.runChannelListener(wrapper, "onAuthRequired", data);

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

// Most WebRequest events are implemented via the observer services, but
// a few use custom xpcom interfaces.  This class (HttpObserverManager)
// serves two main purposes:
//  1. It abstracts away the names and details of the underlying
//     implementation (e.g., onBeforeBeforeRequest is dispatched from
//     the http-on-modify-request observable).
//  2. It aggregates multiple listeners so that a single observer or
//     handler can serve multiple webRequest listeners.
HttpObserverManager = {
  listeners: {
    // onBeforeRequest uses http-on-modify observer for HTTP(S).
    onBeforeRequest: new Map(),

    // onBeforeSendHeaders and onSendHeaders correspond to the
    // http-on-before-connect observer.
    onBeforeSendHeaders: new Map(),
    onSendHeaders: new Map(),

    // onHeadersReceived corresponds to the http-on-examine-* obserservers.
    onHeadersReceived: new Map(),

    // onAuthRequired is handled via the nsIAuthPrompt2 xpcom interface
    // which is managed here by AuthRequestor.
    onAuthRequired: new Map(),

    // onBeforeRedirect is handled by the nsIChannelEVentSink xpcom interface
    // which is managed here by ChannelEventSink.
    onBeforeRedirect: new Map(),

    // onResponseStarted, onErrorOccurred, and OnCompleted correspond
    // to events dispatched by the ChannelWrapper EventTarget.
    onResponseStarted: new Map(),
    onErrorOccurred: new Map(),
    onCompleted: new Map(),
  },

  openingInitialized: false,
  beforeConnectInitialized: false,
  examineInitialized: false,
  redirectInitialized: false,
  activityInitialized: false,
  needTracing: false,
  hasRedirects: false,

  getWrapper(nativeChannel) {
    let wrapper = ChannelWrapper.get(nativeChannel);
    if (!wrapper._addedListeners) {
      /* eslint-disable mozilla/balanced-listeners */
      if (this.listeners.onErrorOccurred.size) {
        wrapper.addEventListener("error", this);
      }
      if (this.listeners.onResponseStarted.size) {
        wrapper.addEventListener("start", this);
      }
      if (this.listeners.onCompleted.size) {
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

  // This method is called whenever webRequest listeners are added or removed.
  // It reconciles the set of listeners with underlying observers, event
  // handlers, etc. by adding new low-level handlers for any newly added
  // webRequest listeners and removing those that are no longer needed if
  // there are no more listeners for corresponding webRequest events.
  addOrRemove() {
    let needOpening = this.listeners.onBeforeRequest.size;
    let needBeforeConnect =
      this.listeners.onBeforeSendHeaders.size ||
      this.listeners.onSendHeaders.size;
    if (needOpening && !this.openingInitialized) {
      this.openingInitialized = true;
      Services.obs.addObserver(this, "http-on-modify-request");
    } else if (!needOpening && this.openingInitialized) {
      this.openingInitialized = false;
      Services.obs.removeObserver(this, "http-on-modify-request");
    }
    if (needBeforeConnect && !this.beforeConnectInitialized) {
      this.beforeConnectInitialized = true;
      Services.obs.addObserver(this, "http-on-before-connect");
    } else if (!needBeforeConnect && this.beforeConnectInitialized) {
      this.beforeConnectInitialized = false;
      Services.obs.removeObserver(this, "http-on-before-connect");
    }

    let haveBlocking = Object.values(this.listeners).some(listeners =>
      Array.from(listeners.values()).some(listener => listener.blockingAllowed)
    );

    this.needTracing =
      this.listeners.onResponseStarted.size ||
      this.listeners.onErrorOccurred.size ||
      this.listeners.onCompleted.size ||
      haveBlocking;

    let needExamine =
      this.needTracing ||
      this.listeners.onHeadersReceived.size ||
      this.listeners.onAuthRequired.size;

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
    this.hasRedirects = this.listeners.onBeforeRedirect.size > 0;
    let needRedirect =
      this.hasRedirects || needExamine || needOpening || needBeforeConnect;
    if (needRedirect && !this.redirectInitialized) {
      this.redirectInitialized = true;
      ChannelEventSink.register();
    } else if (!needRedirect && this.redirectInitialized) {
      this.redirectInitialized = false;
      ChannelEventSink.unregister();
    }

    let needActivity = this.listeners.onErrorOccurred.size;
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
        this.runChannelListener(channel, "onBeforeRequest");
        break;
      case "http-on-before-connect":
        this.runChannelListener(channel, "onBeforeSendHeaders");
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
          this.runChannelListener(channel, "onErrorOccurred", {
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

  getRequestData(channel, extraData) {
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
      thirdParty: channel.thirdParty,

      originUrl: channel.originURL || undefined,
      documentUrl: channel.documentURL || undefined,

      windowId: channel.windowId,
      parentWindowId: channel.parentWindowId,

      frameAncestors: channel.frameAncestors || undefined,

      ip: channel.remoteAddress,

      proxyInfo: channel.proxyInfo,

      serialize: serializeRequestData,
      requestSize: channel.requestSize,
      responseSize: channel.responseSize,
      urlClassification: channel.urlClassification,
    };

    return Object.assign(data, extraData);
  },

  handleEvent(event) {
    let channel = event.currentTarget;
    switch (event.type) {
      case "error":
        this.runChannelListener(channel, "onErrorOccurred", {
          error: channel.errorString,
        });
        break;
      case "start":
        this.runChannelListener(channel, "onResponseStarted");
        break;
      case "stop":
        this.runChannelListener(channel, "onCompleted");
        break;
    }
  },

  STATUS_TYPES: new Set([
    "onHeadersReceived",
    "onAuthRequired",
    "onBeforeRedirect",
    "onResponseStarted",
    "onCompleted",
  ]),
  FILTER_TYPES: new Set([
    "onBeforeRequest",
    "onBeforeSendHeaders",
    "onSendHeaders",
    "onHeadersReceived",
    "onAuthRequired",
    "onBeforeRedirect",
  ]),

  runChannelListener(channel, kind, extraData = null) {
    let handlerResults = [];
    let requestHeaders;
    let responseHeaders;

    try {
      if (kind !== "onErrorOccurred" && channel.errorString) {
        return;
      }

      let registerFilter = this.FILTER_TYPES.has(kind);
      let commonData = null;
      let requestBody;
      this.listeners[kind].forEach((opts, callback) => {
        if (!channel.matches(opts.filter, opts.policy, extraData)) {
          return;
        }

        if (!commonData) {
          commonData = this.getRequestData(channel, extraData);
          if (this.STATUS_TYPES.has(kind)) {
            commonData.statusCode = channel.statusCode;
            commonData.statusLine = channel.statusLine;
          }
        }
        let data = Object.create(commonData);

        if (registerFilter && opts.blocking && opts.policy) {
          data.registerTraceableChannel = (policy, remoteTab) => {
            // `channel` is a ChannelWrapper, which contains the actual
            // underlying nsIChannel in `channel.channel`.  For startup events
            // that are held until the extension background page is started,
            // it is possible that the underlying channel can be closed and
            // cleaned up between the time the event occurred and the time
            // we reach this code.
            if (channel.channel) {
              channel.registerTraceableChannel(policy, remoteTab);
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
    let suspenders = [];

    try {
      for (let { opts, result } of handlerResults) {
        if (isThenable(result)) {
          suspenders.push(opts.addonId);
          channel.suspend();
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
          kind === "onAuthRequired" &&
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
          let text = "";
          if (Services.profiler?.IsActive()) {
            text =
              `${kind} ${channel.finalURL}` +
              ` by ${suspenders.join(", ")} canceled`;
          }
          channel.resume(text);
          channel.cancel(
            Cr.NS_ERROR_ABORT,
            Ci.nsILoadInfo.BLOCKING_REASON_EXTENSION_WEBREQUEST
          );
          let { policy } = opts;
          if (policy) {
            let properties = channel.channel.QueryInterface(
              Ci.nsIWritablePropertyBag
            );
            properties.setProperty("cancelledByExtension", policy.id);
          }
          return;
        }

        if (result.redirectUrl) {
          try {
            let text = "";
            if (Services.profiler?.IsActive()) {
              text =
                `${kind} ${channel.finalURL}` +
                ` by ${suspenders.join(", ")}` +
                ` redirected to ${result.redirectUrl}`;
            }
            channel.resume(text);
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

        if (result.upgradeToSecure && kind === "onBeforeRequest") {
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
      if (kind === "onAuthRequired" && channel.authPromptForward) {
        channel.authPromptForward();
      }

      if (kind === "onBeforeSendHeaders" && this.listeners.onSendHeaders.size) {
        await this.runChannelListener(channel, "onSendHeaders");
      } else if (kind !== "onErrorOccurred") {
        channel.errorCheck();
      }
    } catch (e) {
      Cu.reportError(e);
    }

    // Only resume the channel if it was suspended by this call.
    if (shouldResume) {
      let text = "";
      if (Services.profiler?.IsActive()) {
        text = `${kind} ${channel.finalURL} by ${suspenders.join(", ")}`;
      }
      channel.resume(text);
    }
  },

  shouldHookListener(listener, channel, extraData) {
    if (listener.size == 0) {
      return false;
    }

    for (let opts of listener.values()) {
      if (channel.matches(opts.filter, opts.policy, extraData)) {
        return true;
      }
    }
    return false;
  },

  examine(channel, topic, data) {
    if (this.listeners.onHeadersReceived.size) {
      this.runChannelListener(channel, "onHeadersReceived");
    }

    if (
      !channel.hasAuthRequestor &&
      this.shouldHookListener(this.listeners.onAuthRequired, channel, {
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
      this.runChannelListener(channel, "onBeforeRedirect", {
        redirectUrl: newChannel.originalURI.spec,
      });
    }
    channel.channel = newChannel;
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

var onBeforeRequest = new HttpEvent("onBeforeRequest", [
  "blocking",
  "requestBody",
]);
var onBeforeSendHeaders = new HttpEvent("onBeforeSendHeaders", [
  "requestHeaders",
  "blocking",
]);
var onSendHeaders = new HttpEvent("onSendHeaders", ["requestHeaders"]);
var onHeadersReceived = new HttpEvent("onHeadersReceived", [
  "blocking",
  "responseHeaders",
]);
var onAuthRequired = new HttpEvent("onAuthRequired", [
  "blocking",
  "responseHeaders",
]);
var onBeforeRedirect = new HttpEvent("onBeforeRedirect", ["responseHeaders"]);
var onResponseStarted = new HttpEvent("onResponseStarted", ["responseHeaders"]);
var onCompleted = new HttpEvent("onCompleted", ["responseHeaders"]);
var onErrorOccurred = new HttpEvent("onErrorOccurred");

var WebRequest = {
  onBeforeRequest,
  onBeforeSendHeaders,
  onSendHeaders,
  onHeadersReceived,
  onAuthRequired,
  onBeforeRedirect,
  onResponseStarted,
  onCompleted,
  onErrorOccurred,

  getSecurityInfo: details => {
    let channel = ChannelWrapper.getRegisteredChannel(
      details.id,
      details.policy,
      details.remoteTab
    );
    if (channel) {
      return SecurityInfo.getSecurityInfo(channel.channel, details.options);
    }
  },
};
