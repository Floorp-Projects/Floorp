/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ProxyScriptContext"];

/* exported ProxyScriptContext */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/ExtensionCommon.jsm");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionChild",
                                  "resource://gre/modules/ExtensionChild.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "ProxyService",
                                   "@mozilla.org/network/protocol-proxy-service;1",
                                   "nsIProtocolProxyService");

const CATEGORY_EXTENSION_SCRIPTS_CONTENT = "webextension-scripts-content";

// The length of time (seconds) to wait for a proxy to resolve before ignoring it.
const PROXY_TIMEOUT_SEC = 10;

const {
  defineLazyGetter,
} = ExtensionUtils;

const {
  BaseContext,
  CanOfAPIs,
  LocalAPIImplementation,
  SchemaAPIManager,
} = ExtensionCommon;

const PROXY_TYPES = Object.freeze({
  DIRECT: "direct",
  HTTPS: "https",
  PROXY: "proxy",
  HTTP: "http", // Synonym for PROXY_TYPES.PROXY
  SOCKS: "socks",  // SOCKS5
  SOCKS4: "socks4",
});

class ProxyScriptContext extends BaseContext {
  constructor(extension, url, contextInfo = {}) {
    super("proxy_script", extension);
    this.contextInfo = contextInfo;
    this.extension = extension;
    this.messageManager = Services.cpmm;
    this.sandbox = Cu.Sandbox(this.extension.principal, {
      sandboxName: `proxyscript:${extension.id}:${url}`,
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
   * @returns {Object} The proxy info to apply for the given URI.
   */
  applyFilter(service, uri, defaultProxyInfo) {
    let ret;
    try {
      // Bug 1337001 - provide path and query components to non-https URLs.
      ret = this.FindProxyForURL(uri.prePath, uri.host, this.contextInfo);
    } catch (e) {
      let error = this.normalizeError(e);
      this.extension.emit("proxy-error", {
        message: error.message,
        fileName: error.fileName,
        lineNumber: error.lineNumber,
        stack: error.stack,
      });
      return defaultProxyInfo;
    }

    if (!ret || typeof ret !== "string") {
      this.extension.emit("proxy-error", {
        message: "FindProxyForURL: Return type must be a string",
      });
      return defaultProxyInfo;
    }

    let rules = ret.split(";");
    let proxyInfo = this.createProxyInfo(rules);

    return proxyInfo || defaultProxyInfo;
  }

  /**
   * Creates a new proxy info object using the return value of FindProxyForURL.
   *
   * @param {Array<string>} rules The list of proxy rules returned by FindProxyForURL.
   *    (e.g. ["PROXY 1.2.3.4:8080", "SOCKS 1.1.1.1:9090", "DIRECT"])
   * @returns {nsIProxyInfo} The proxy info to apply for the given URI.
   */
  createProxyInfo(rules) {
    if (!rules.length) {
      return null;
    }

    let rule = rules[0].trim();

    if (!rule) {
      this.extension.emit("proxy-error", {
        message: "FindProxyForURL: Expected Proxy Rule",
      });
      return null;
    }

    let parts = rule.split(/\s+/);
    if (!parts[0] || parts.length !== 2) {
      this.extension.emit("proxy-error", {
        message: `FindProxyForURL: Invalid Proxy Rule: ${rule}`,
      });
      return null;
    }

    parts[0] = parts[0].toLowerCase();

    switch (parts[0]) {
      case PROXY_TYPES.PROXY:
      case PROXY_TYPES.HTTP:
      case PROXY_TYPES.HTTPS:
      case PROXY_TYPES.SOCKS:
      case PROXY_TYPES.SOCKS4:
        if (!parts[1]) {
          this.extension.emit("proxy-error", {
            message: `FindProxyForURL: Missing argument for "${parts[0]}"`,
          });
          return null;
        }

        let [host, port] = parts[1].split(":");
        if (!host || !port) {
          this.extension.emit("proxy-error", {
            message: `FindProxyForURL: Unable to parse argument for ${rule}`,
          });
          return null;
        }

        let type = parts[0];
        if (parts[0] == PROXY_TYPES.PROXY) {
          // PROXY_TYPES.HTTP and PROXY_TYPES.PROXY are synonyms
          type = PROXY_TYPES.HTTP;
        }

        let failoverProxy = this.createProxyInfo(rules.slice(1));
        return ProxyService.newProxyInfo(type, host, port, 0,
          PROXY_TIMEOUT_SEC, failoverProxy);
      case PROXY_TYPES.DIRECT:
        return null;
      default:
        this.extension.emit("proxy-error", {
          message: `FindProxyForURL: Unrecognized proxy type: "${parts[0]}"`,
        });
        return null;
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
    super("proxy");
    this.initialized = false;
  }

  lazyInit() {
    if (!this.initialized) {
      for (let [/* name */, value] of XPCOMUtils.enumerateCategoryEntries(
          CATEGORY_EXTENSION_SCRIPTS_CONTENT)) {
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
  Schemas.inject(browserObj, injectionContext);
  return browserObj;
});
