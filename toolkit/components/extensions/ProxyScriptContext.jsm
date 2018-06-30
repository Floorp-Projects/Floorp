/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ProxyScriptContext", "ProxyChannelFilter"];

/* exported ProxyScriptContext, ProxyChannelFilter */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/ExtensionCommon.jsm");
ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");

ChromeUtils.defineModuleGetter(this, "ExtensionChild",
                               "resource://gre/modules/ExtensionChild.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionParent",
                               "resource://gre/modules/ExtensionParent.jsm");
ChromeUtils.defineModuleGetter(this, "Schemas",
                               "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "ProxyService",
                                   "@mozilla.org/network/protocol-proxy-service;1",
                                   "nsIProtocolProxyService");

XPCOMUtils.defineLazyGetter(this, "tabTracker", () => {
  return ExtensionParent.apiManager.global.tabTracker;
});

const CATEGORY_EXTENSION_SCRIPTS_CONTENT = "webextension-scripts-content";

// DNS is resolved on the SOCKS proxy server.
const {TRANSPARENT_PROXY_RESOLVES_HOST} = Ci.nsIProxyInfo;

// The length of time (seconds) to wait for a proxy to resolve before ignoring it.
const PROXY_TIMEOUT_SEC = 10;

const {
  ExtensionError,
} = ExtensionUtils;

const {
  BaseContext,
  CanOfAPIs,
  LocalAPIImplementation,
  SchemaAPIManager,
  defineLazyGetter,
} = ExtensionCommon;

const PROXY_TYPES = Object.freeze({
  DIRECT: "direct",
  HTTPS: "https",
  PROXY: "http", // Synonym for PROXY_TYPES.HTTP
  HTTP: "http",
  SOCKS: "socks", // SOCKS5
  SOCKS4: "socks4",
});

const ProxyInfoData = {
  validate(proxyData) {
    if (proxyData.type && proxyData.type.toLowerCase() === "direct") {
      return {type: proxyData.type};
    }
    for (let prop of ["type", "host", "port", "username", "password", "proxyDNS", "failoverTimeout"]) {
      this[prop](proxyData);
    }
    return proxyData;
  },

  type(proxyData) {
    let {type} = proxyData;
    if (typeof type !== "string" || !PROXY_TYPES.hasOwnProperty(type.toUpperCase())) {
      throw new ExtensionError(`ProxyInfoData: Invalid proxy server type: "${type}"`);
    }
    proxyData.type = PROXY_TYPES[type.toUpperCase()];
  },

  host(proxyData) {
    let {host} = proxyData;
    if (typeof host !== "string" || host.includes(" ")) {
      throw new ExtensionError(`ProxyInfoData: Invalid proxy server host: "${host}"`);
    }
    if (!host.length) {
      throw new ExtensionError("ProxyInfoData: Proxy server host cannot be empty");
    }
    proxyData.host = host;
  },

  port(proxyData) {
    let port = Number.parseInt(proxyData.port, 10);
    if (!Number.isInteger(port)) {
      throw new ExtensionError(`ProxyInfoData: Invalid proxy server port: "${port}"`);
    }

    if (port < 1 || port > 0xffff) {
      throw new ExtensionError(`ProxyInfoData: Proxy server port ${port} outside range 1 to 65535`);
    }
    proxyData.port = port;
  },

  username(proxyData) {
    let {username} = proxyData;
    if (username !== undefined && typeof username !== "string") {
      throw new ExtensionError(`ProxyInfoData: Invalid proxy server username: "${username}"`);
    }
  },

  password(proxyData) {
    let {password} = proxyData;
    if (password !== undefined && typeof password !== "string") {
      throw new ExtensionError(`ProxyInfoData: Invalid proxy server password: "${password}"`);
    }
  },

  proxyDNS(proxyData) {
    let {proxyDNS, type} = proxyData;
    if (proxyDNS !== undefined) {
      if (typeof proxyDNS !== "boolean") {
        throw new ExtensionError(`ProxyInfoData: Invalid proxyDNS value: "${proxyDNS}"`);
      }
      if (proxyDNS && type !== PROXY_TYPES.SOCKS && type !== PROXY_TYPES.SOCKS4) {
        throw new ExtensionError(`ProxyInfoData: proxyDNS can only be true for SOCKS proxy servers`);
      }
    }
  },

  failoverTimeout(proxyData) {
    let {failoverTimeout} = proxyData;
    if (failoverTimeout !== undefined && (!Number.isInteger(failoverTimeout) || failoverTimeout < 1)) {
      throw new ExtensionError(`ProxyInfoData: Invalid failover timeout: "${failoverTimeout}"`);
    }
  },

  createProxyInfoFromData(proxyDataList, defaultProxyInfo, proxyDataListIndex = 0) {
    if (proxyDataListIndex >= proxyDataList.length) {
      return defaultProxyInfo;
    }
    let proxyData = proxyDataList[proxyDataListIndex];
    if (proxyData == null) {
      return null;
    }
    let {type, host, port, username, password, proxyDNS, failoverTimeout} =
        ProxyInfoData.validate(proxyData);
    if (type === PROXY_TYPES.DIRECT) {
      return defaultProxyInfo;
    }
    let failoverProxy = this.createProxyInfoFromData(proxyDataList, defaultProxyInfo, proxyDataListIndex + 1);

    if (type === PROXY_TYPES.SOCKS || type === PROXY_TYPES.SOCKS4) {
      return ProxyService.newProxyInfoWithAuth(
        type, host, port, username, password,
        proxyDNS ? TRANSPARENT_PROXY_RESOLVES_HOST : 0,
        failoverTimeout ? failoverTimeout : PROXY_TIMEOUT_SEC,
        failoverProxy);
    }
    return ProxyService.newProxyInfo(
      type, host, port,
      proxyDNS ? TRANSPARENT_PROXY_RESOLVES_HOST : 0,
      failoverTimeout ? failoverTimeout : PROXY_TIMEOUT_SEC,
      failoverProxy);
  },

  /**
   * Creates a new proxy info data object using the return value of FindProxyForURL.
   *
   * @param {Array<string>} rule A single proxy rule returned by FindProxyForURL.
   *    (e.g. "PROXY 1.2.3.4:8080", "SOCKS 1.1.1.1:9090" or "DIRECT")
   * @returns {nsIProxyInfo} The proxy info to apply for the given URI.
   */
  parseProxyInfoDataFromPAC(rule) {
    if (!rule) {
      throw new ExtensionError("ProxyInfoData: Missing Proxy Rule");
    }

    let parts = rule.toLowerCase().split(/\s+/);
    if (!parts[0] || parts.length > 2) {
      throw new ExtensionError(`ProxyInfoData: Invalid arguments passed for proxy rule: "${rule}"`);
    }
    let type = parts[0];
    let [host, port] = parts.length > 1 ? parts[1].split(":") : [];

    switch (PROXY_TYPES[type.toUpperCase()]) {
      case PROXY_TYPES.HTTP:
      case PROXY_TYPES.HTTPS:
      case PROXY_TYPES.SOCKS:
      case PROXY_TYPES.SOCKS4:
        if (!host || !port) {
          throw new ExtensionError(`ProxyInfoData: Invalid host or port from proxy rule: "${rule}"`);
        }
        return {type, host, port};
      case PROXY_TYPES.DIRECT:
        if (host || port) {
          throw new ExtensionError(`ProxyInfoData: Invalid argument for proxy type: "${type}"`);
        }
        return {type};
      default:
        throw new ExtensionError(`ProxyInfoData: Unrecognized proxy type: "${type}"`);
    }
  },

  proxyInfoFromProxyData(context, proxyData, defaultProxyInfo) {
    switch (typeof proxyData) {
      case "string":
        let proxyRules = [];
        try {
          for (let result of proxyData.split(";")) {
            proxyRules.push(ProxyInfoData.parseProxyInfoDataFromPAC(result.trim()));
          }
        } catch (e) {
          // If we have valid proxies already, lets use them and just emit
          // errors for the failovers.
          if (proxyRules.length === 0) {
            throw e;
          }
          let error = context.normalizeError(e);
          context.extension.emit("proxy-error", {
            message: error.message,
            fileName: error.fileName,
            lineNumber: error.lineNumber,
            stack: error.stack,
          });
        }
        proxyData = proxyRules;
        // fall through
      case "object":
        if (Array.isArray(proxyData) && proxyData.length > 0) {
          return ProxyInfoData.createProxyInfoFromData(proxyData, defaultProxyInfo);
        }
        // Not an array, fall through to error.
      default:
        throw new ExtensionError("ProxyInfoData: proxyData must be a string or array of objects");
    }
  },
};

function normalizeFilter(filter) {
  if (!filter) {
    filter = {};
  }

  return {urls: filter.urls || null, types: filter.types || null};
}

class ProxyChannelFilter {
  constructor(context, extension, listener, filter, extraInfoSpec) {
    this.context = context;
    this.extension = extension;
    this.filter = normalizeFilter(filter);
    this.listener = listener;
    this.extraInfoSpec = extraInfoSpec || [];

    ProxyService.registerChannelFilter(
      this /* nsIProtocolProxyChannelFilter aFilter */,
      0 /* unsigned long aPosition */
    );
  }

  // Copy from WebRequest.jsm with small changes.
  getRequestData(channel, extraData) {
    let data = {
      requestId: String(channel.id),
      url: channel.finalURL,
      method: channel.method,
      type: channel.type,
      fromCache: !!channel.fromCache,

      originUrl: channel.originURL || undefined,
      documentUrl: channel.documentURL || undefined,

      frameId: channel.windowId,
      parentFrameId: channel.parentWindowId,

      frameAncestors: channel.frameAncestors || undefined,

      timeStamp: Date.now(),

      ...extraData,
    };
    if (this.extraInfoSpec.includes("requestHeaders")) {
      data.requestHeaders = channel.getRequestHeaders();
    }
    return data;
  }

  /**
   * This method (which is required by the nsIProtocolProxyService interface)
   * is called to apply proxy filter rules for the given URI and proxy object
   * (or list of proxy objects).
   *
   * @param {nsIProtocolProxyService} service A reference to the Protocol Proxy Service.
   * @param {nsIChannel} channel The channel for which these proxy settings apply.
   * @param {nsIProxyInfo} defaultProxyInfo The proxy (or list of proxies) that
   *     would be used by default for the given URI. This may be null.
   * @param {nsIProtocolProxyChannelFilter} proxyFilter
   */
  async applyFilter(service, channel, defaultProxyInfo, proxyFilter) {
    let proxyInfo;
    try {
      let wrapper = ChannelWrapper.get(channel);

      let browserData = {tabId: -1, windowId: -1};
      if (wrapper.browserElement) {
        browserData = tabTracker.getBrowserData(wrapper.browserElement);
      }
      let {filter} = this;
      if (filter.tabId != null && browserData.tabId !== filter.tabId) {
        return;
      }
      if (filter.windowId != null && browserData.windowId !== filter.windowId) {
        return;
      }

      if (wrapper.matches(filter, this.extension.policy, {isProxy: true})) {
        let data = this.getRequestData(wrapper, {tabId: browserData.tabId});

        let ret = await this.listener(data);
        if (ret == null) {
          // If ret undefined or null, fall through to the `finally` block to apply the proxy result.
          proxyInfo = ret;
          return;
        }
        // We only accept proxyInfo objects, not the PAC strings. ProxyInfoData will
        // accept either, so we want to enforce the limit here.
        if (typeof ret !== "object") {
          throw new ExtensionError("ProxyInfoData: proxyData must be an object or array of objects");
        }
        // We allow the call to return either a single proxyInfo or an array of proxyInfo.
        if (!Array.isArray(ret)) {
          ret = [ret];
        }
        proxyInfo = ProxyInfoData.createProxyInfoFromData(ret, defaultProxyInfo);
      }
    } catch (e) {
      // We need to normalize errors to dispatch them to the extension handler.  If
      // we have not started up yet, we'll just log those to the console.
      if (!this.context) {
        this.extension.logError(`proxy-error before extension startup: ${e}`);
        return;
      }
      let error = this.context.normalizeError(e);
      this.extension.emit("proxy-error", {
        message: error.message,
        fileName: error.fileName,
        lineNumber: error.lineNumber,
        stack: error.stack,
      });
    } finally {
      // We must call onProxyFilterResult.  proxyInfo may be null or nsIProxyInfo.
      // defaultProxyInfo will be null unless a prior proxy handler has set something.
      // If proxyInfo is null, that removes any prior proxy config.  This allows a
      // proxy extension to override higher level (e.g. prefs) config under certain
      // circumstances.
      proxyFilter.onProxyFilterResult(proxyInfo !== undefined ? proxyInfo : defaultProxyInfo);
    }
  }

  destroy() {
    ProxyService.unregisterFilter(this);
  }
}

class ProxyScriptContext extends BaseContext {
  constructor(extension, url, contextInfo = {}) {
    super("proxy_script", extension);
    this.contextInfo = contextInfo;
    this.extension = extension;
    this.messageManager = Services.cpmm;
    this.sandbox = Cu.Sandbox(this.extension.principal, {
      sandboxName: `Extension Proxy Script (${extension.policy.debugName}): ${url}`,
      metadata: {addonID: extension.id},
    });
    this.url = url;
    this.FindProxyForURL = null;
  }

  /**
   * Loads and validates a proxy script into the sandbox, and then
   * registers a new proxy filter for the context.
   *
   * @returns {boolean} true if load succeeded; false otherwise.
   */
  load() {
    Schemas.exportLazyGetter(this.sandbox, "browser", () => this.browserObj);

    try {
      Services.scriptloader.loadSubScript(this.url, this.sandbox, "UTF-8");
    } catch (error) {
      this.extension.emit("proxy-error", {
        message: this.normalizeError(error).message,
      });
      return false;
    }

    this.FindProxyForURL = Cu.unwaiveXrays(this.sandbox.FindProxyForURL);
    if (typeof this.FindProxyForURL !== "function") {
      this.extension.emit("proxy-error", {
        message: "The proxy script must define FindProxyForURL as a function",
      });
      return false;
    }

    ProxyService.registerFilter(
      this /* nsIProtocolProxyFilter aFilter */,
      0 /* unsigned long aPosition */
    );

    return true;
  }

  get principal() {
    return this.extension.principal;
  }

  get cloneScope() {
    return this.sandbox;
  }

  /**
   * This method (which is required by the nsIProtocolProxyService interface)
   * is called to apply proxy filter rules for the given URI and proxy object
   * (or list of proxy objects).
   *
   * @param {Object} service A reference to the Protocol Proxy Service.
   * @param {Object} uri The URI for which these proxy settings apply.
   * @param {Object} defaultProxyInfo The proxy (or list of proxies) that
   *     would be used by default for the given URI. This may be null.
   * @param {Object} callback nsIProxyProtocolFilterResult to call onProxyFilterResult
         on with the proxy info to apply for the given URI.
   */
  applyFilter(service, uri, defaultProxyInfo, callback) {
    try {
      // TODO Bug 1337001 - provide path and query components to non-https URLs.
      let ret = this.FindProxyForURL(uri.prePath, uri.host, this.contextInfo);
      ret = ProxyInfoData.proxyInfoFromProxyData(this, ret, defaultProxyInfo);
      callback.onProxyFilterResult(ret);
    } catch (e) {
      let error = this.normalizeError(e);
      this.extension.emit("proxy-error", {
        message: error.message,
        fileName: error.fileName,
        lineNumber: error.lineNumber,
        stack: error.stack,
      });
      callback.onProxyFilterResult(defaultProxyInfo);
    }
  }

  /**
   * Unloads the proxy filter and shuts down the sandbox.
   */
  unload() {
    super.unload();
    ProxyService.unregisterFilter(this);
    Cu.nukeSandbox(this.sandbox);
    this.sandbox = null;
  }
}

class ProxyScriptAPIManager extends SchemaAPIManager {
  constructor() {
    super("proxy", Schemas);
    this.initialized = false;
  }

  lazyInit() {
    if (!this.initialized) {
      this.initGlobal();
      let entries = XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCRIPTS_CONTENT);
      for (let [/* name */, value] of entries) {
        this.loadScript(value);
      }
      this.initialized = true;
    }
  }
}

class ProxyScriptInjectionContext {
  constructor(context, apiCan) {
    this.context = context;
    this.localAPIs = apiCan.root;
    this.apiCan = apiCan;
  }

  shouldInject(namespace, name, allowedContexts) {
    if (this.context.envType !== "proxy_script") {
      throw new Error(`Unexpected context type "${this.context.envType}"`);
    }

    // Do not generate proxy script APIs unless explicitly allowed.
    return allowedContexts.includes("proxy");
  }

  getImplementation(namespace, name) {
    this.apiCan.findAPIPath(`${namespace}.${name}`);
    let obj = this.apiCan.findAPIPath(namespace);

    if (obj && name in obj) {
      return new LocalAPIImplementation(obj, name, this.context);
    }
  }

  get cloneScope() {
    return this.context.cloneScope;
  }

  get principal() {
    return this.context.principal;
  }
}

defineLazyGetter(ProxyScriptContext.prototype, "messenger", function() {
  let sender = {id: this.extension.id, frameId: this.frameId, url: this.url};
  let filter = {extensionId: this.extension.id, toProxyScript: true};
  return new ExtensionChild.Messenger(this, [this.messageManager], sender, filter);
});

let proxyScriptAPIManager = new ProxyScriptAPIManager();

defineLazyGetter(ProxyScriptContext.prototype, "browserObj", function() {
  let localAPIs = {};
  let can = new CanOfAPIs(this, proxyScriptAPIManager, localAPIs);
  proxyScriptAPIManager.lazyInit();

  let browserObj = Cu.createObjectIn(this.sandbox);
  let injectionContext = new ProxyScriptInjectionContext(this, can);
  proxyScriptAPIManager.schema.inject(browserObj, injectionContext);
  return browserObj;
});
