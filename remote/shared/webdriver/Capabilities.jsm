/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [
  "Capabilities",
  "PageLoadStrategy",
  "Proxy",
  "Timeouts",
  "UnhandledPromptBehavior",
];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Preferences: "resource://gre/modules/Preferences.jsm",

  AppInfo: "chrome://remote/content/shared/AppInfo.jsm",
  assert: "chrome://remote/content/shared/webdriver/Assert.jsm",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  pprint: "chrome://remote/content/shared/Format.jsm",
  RemoteAgent: "chrome://remote/content/components/RemoteAgent.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "remoteAgent", () => {
  return Cc["@mozilla.org/remote/agent;1"].createInstance(Ci.nsIRemoteAgent);
});

/** Representation of WebDriver session timeouts. */
class Timeouts {
  constructor() {
    // disabled
    this.implicit = 0;
    // five minutes
    this.pageLoad = 300000;
    // 30 seconds
    this.script = 30000;
  }

  toString() {
    return "[object Timeouts]";
  }

  /** Marshals timeout durations to a JSON Object. */
  toJSON() {
    return {
      implicit: this.implicit,
      pageLoad: this.pageLoad,
      script: this.script,
    };
  }

  static fromJSON(json) {
    lazy.assert.object(
      json,
      lazy.pprint`Expected "timeouts" to be an object, got ${json}`
    );
    let t = new Timeouts();

    for (let [type, ms] of Object.entries(json)) {
      switch (type) {
        case "implicit":
          t.implicit = lazy.assert.positiveInteger(
            ms,
            lazy.pprint`Expected ${type} to be a positive integer, got ${ms}`
          );
          break;

        case "script":
          if (ms !== null) {
            lazy.assert.positiveInteger(
              ms,
              lazy.pprint`Expected ${type} to be a positive integer, got ${ms}`
            );
          }
          t.script = ms;
          break;

        case "pageLoad":
          t.pageLoad = lazy.assert.positiveInteger(
            ms,
            lazy.pprint`Expected ${type} to be a positive integer, got ${ms}`
          );
          break;

        default:
          throw new lazy.error.InvalidArgumentError(
            "Unrecognised timeout: " + type
          );
      }
    }

    return t;
  }
}

/**
 * Enum of page loading strategies.
 *
 * @enum
 */
const PageLoadStrategy = {
  /** No page load strategy.  Navigation will return immediately. */
  None: "none",
  /**
   * Eager, causing navigation to complete when the document reaches
   * the <code>interactive</code> ready state.
   */
  Eager: "eager",
  /**
   * Normal, causing navigation to return when the document reaches the
   * <code>complete</code> ready state.
   */
  Normal: "normal",
};

/** Proxy configuration object representation. */
class Proxy {
  /** @class */
  constructor() {
    this.proxyType = null;
    this.httpProxy = null;
    this.httpProxyPort = null;
    this.noProxy = null;
    this.sslProxy = null;
    this.sslProxyPort = null;
    this.socksProxy = null;
    this.socksProxyPort = null;
    this.socksVersion = null;
    this.proxyAutoconfigUrl = null;
  }

  /**
   * Sets Firefox proxy settings.
   *
   * @return {boolean}
   *     True if proxy settings were updated as a result of calling this
   *     function, or false indicating that this function acted as
   *     a no-op.
   */
  init() {
    switch (this.proxyType) {
      case "autodetect":
        lazy.Preferences.set("network.proxy.type", 4);
        return true;

      case "direct":
        lazy.Preferences.set("network.proxy.type", 0);
        return true;

      case "manual":
        lazy.Preferences.set("network.proxy.type", 1);

        if (this.httpProxy) {
          lazy.Preferences.set("network.proxy.http", this.httpProxy);
          if (Number.isInteger(this.httpProxyPort)) {
            lazy.Preferences.set("network.proxy.http_port", this.httpProxyPort);
          }
        }

        if (this.sslProxy) {
          lazy.Preferences.set("network.proxy.ssl", this.sslProxy);
          if (Number.isInteger(this.sslProxyPort)) {
            lazy.Preferences.set("network.proxy.ssl_port", this.sslProxyPort);
          }
        }

        if (this.socksProxy) {
          lazy.Preferences.set("network.proxy.socks", this.socksProxy);
          if (Number.isInteger(this.socksProxyPort)) {
            lazy.Preferences.set(
              "network.proxy.socks_port",
              this.socksProxyPort
            );
          }
          if (this.socksVersion) {
            lazy.Preferences.set(
              "network.proxy.socks_version",
              this.socksVersion
            );
          }
        }

        if (this.noProxy) {
          lazy.Preferences.set(
            "network.proxy.no_proxies_on",
            this.noProxy.join(", ")
          );
        }
        return true;

      case "pac":
        lazy.Preferences.set("network.proxy.type", 2);
        lazy.Preferences.set(
          "network.proxy.autoconfig_url",
          this.proxyAutoconfigUrl
        );
        return true;

      case "system":
        lazy.Preferences.set("network.proxy.type", 5);
        return true;

      default:
        return false;
    }
  }

  /**
   * @param {Object.<string, ?>} json
   *     JSON Object to unmarshal.
   *
   * @throws {InvalidArgumentError}
   *     When proxy configuration is invalid.
   */
  static fromJSON(json) {
    function stripBracketsFromIpv6Hostname(hostname) {
      return hostname.includes(":")
        ? hostname.replace(/[\[\]]/g, "")
        : hostname;
    }

    // Parse hostname and optional port from host
    function fromHost(scheme, host) {
      lazy.assert.string(
        host,
        lazy.pprint`Expected proxy "host" to be a string, got ${host}`
      );

      if (host.includes("://")) {
        throw new lazy.error.InvalidArgumentError(`${host} contains a scheme`);
      }

      let url;
      try {
        // To parse the host a scheme has to be added temporarily.
        // If the returned value for the port is an empty string it
        // could mean no port or the default port for this scheme was
        // specified. In such a case parse again with a different
        // scheme to ensure we filter out the default port.
        url = new URL("http://" + host);
        if (url.port == "") {
          url = new URL("https://" + host);
        }
      } catch (e) {
        throw new lazy.error.InvalidArgumentError(e.message);
      }

      let hostname = stripBracketsFromIpv6Hostname(url.hostname);

      // If the port hasn't been set, use the default port of
      // the selected scheme (except for socks which doesn't have one).
      let port = parseInt(url.port);
      if (!Number.isInteger(port)) {
        if (scheme === "socks") {
          port = null;
        } else {
          port = Services.io.getProtocolHandler(scheme).defaultPort;
        }
      }

      if (
        url.username != "" ||
        url.password != "" ||
        url.pathname != "/" ||
        url.search != "" ||
        url.hash != ""
      ) {
        throw new lazy.error.InvalidArgumentError(
          `${host} was not of the form host[:port]`
        );
      }

      return [hostname, port];
    }

    let p = new Proxy();
    if (typeof json == "undefined" || json === null) {
      return p;
    }

    lazy.assert.object(
      json,
      lazy.pprint`Expected "proxy" to be an object, got ${json}`
    );

    lazy.assert.in(
      "proxyType",
      json,
      lazy.pprint`Expected "proxyType" in "proxy" object, got ${json}`
    );
    p.proxyType = lazy.assert.string(
      json.proxyType,
      lazy.pprint`Expected "proxyType" to be a string, got ${json.proxyType}`
    );

    switch (p.proxyType) {
      case "autodetect":
      case "direct":
      case "system":
        break;

      case "pac":
        p.proxyAutoconfigUrl = lazy.assert.string(
          json.proxyAutoconfigUrl,
          `Expected "proxyAutoconfigUrl" to be a string, ` +
            lazy.pprint`got ${json.proxyAutoconfigUrl}`
        );
        break;

      case "manual":
        if (typeof json.ftpProxy != "undefined") {
          throw new lazy.error.InvalidArgumentError(
            "Since Firefox 90 'ftpProxy' is no longer supported"
          );
        }
        if (typeof json.httpProxy != "undefined") {
          [p.httpProxy, p.httpProxyPort] = fromHost("http", json.httpProxy);
        }
        if (typeof json.sslProxy != "undefined") {
          [p.sslProxy, p.sslProxyPort] = fromHost("https", json.sslProxy);
        }
        if (typeof json.socksProxy != "undefined") {
          [p.socksProxy, p.socksProxyPort] = fromHost("socks", json.socksProxy);
          p.socksVersion = lazy.assert.positiveInteger(
            json.socksVersion,
            lazy.pprint`Expected "socksVersion" to be a positive integer, got ${json.socksVersion}`
          );
        }
        if (typeof json.noProxy != "undefined") {
          let entries = lazy.assert.array(
            json.noProxy,
            lazy.pprint`Expected "noProxy" to be an array, got ${json.noProxy}`
          );
          p.noProxy = entries.map(entry => {
            lazy.assert.string(
              entry,
              lazy.pprint`Expected "noProxy" entry to be a string, got ${entry}`
            );
            return stripBracketsFromIpv6Hostname(entry);
          });
        }
        break;

      default:
        throw new lazy.error.InvalidArgumentError(
          `Invalid type of proxy: ${p.proxyType}`
        );
    }

    return p;
  }

  /**
   * @return {Object.<string, (number|string)>}
   *     JSON serialisation of proxy object.
   */
  toJSON() {
    function addBracketsToIpv6Hostname(hostname) {
      return hostname.includes(":") ? `[${hostname}]` : hostname;
    }

    function toHost(hostname, port) {
      if (!hostname) {
        return null;
      }

      // Add brackets around IPv6 addresses
      hostname = addBracketsToIpv6Hostname(hostname);

      if (port != null) {
        return `${hostname}:${port}`;
      }

      return hostname;
    }

    let excludes = this.noProxy;
    if (excludes) {
      excludes = excludes.map(addBracketsToIpv6Hostname);
    }

    return marshal({
      proxyType: this.proxyType,
      httpProxy: toHost(this.httpProxy, this.httpProxyPort),
      noProxy: excludes,
      sslProxy: toHost(this.sslProxy, this.sslProxyPort),
      socksProxy: toHost(this.socksProxy, this.socksProxyPort),
      socksVersion: this.socksVersion,
      proxyAutoconfigUrl: this.proxyAutoconfigUrl,
    });
  }

  toString() {
    return "[object Proxy]";
  }
}

/**
 * Enum of unhandled prompt behavior.
 *
 * @enum
 */
const UnhandledPromptBehavior = {
  /** All simple dialogs encountered should be accepted. */
  Accept: "accept",
  /**
   * All simple dialogs encountered should be accepted, and an error
   * returned that the dialog was handled.
   */
  AcceptAndNotify: "accept and notify",
  /** All simple dialogs encountered should be dismissed. */
  Dismiss: "dismiss",
  /**
   * All simple dialogs encountered should be dismissed, and an error
   * returned that the dialog was handled.
   */
  DismissAndNotify: "dismiss and notify",
  /** All simple dialogs encountered should be left to the user to handle. */
  Ignore: "ignore",
};

/** WebDriver session capabilities representation. */
class Capabilities extends Map {
  /** @class */
  constructor() {
    super([
      // webdriver
      ["browserName", getWebDriverBrowserName()],
      ["browserVersion", lazy.AppInfo.version],
      ["platformName", getWebDriverPlatformName()],
      ["acceptInsecureCerts", false],
      ["pageLoadStrategy", PageLoadStrategy.Normal],
      ["proxy", new Proxy()],
      ["setWindowRect", !lazy.AppInfo.isAndroid],
      ["timeouts", new Timeouts()],
      ["strictFileInteractability", false],
      ["unhandledPromptBehavior", UnhandledPromptBehavior.DismissAndNotify],
      ["webSocketUrl", null],

      // proprietary
      ["moz:accessibilityChecks", false],
      ["moz:buildID", lazy.AppInfo.appBuildID],
      [
        "moz:debuggerAddress",
        // With bug 1715481 fixed always use the Remote Agent instance
        lazy.RemoteAgent.running && lazy.RemoteAgent.cdp
          ? lazy.remoteAgent.debuggerAddress
          : null,
      ],
      [
        "moz:headless",
        Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo).isHeadless,
      ],
      ["moz:platformVersion", Services.sysinfo.getProperty("version")],
      ["moz:processID", lazy.AppInfo.processID],
      ["moz:profile", maybeProfile()],
      [
        "moz:shutdownTimeout",
        Services.prefs.getIntPref("toolkit.asyncshutdown.crash_timeout"),
      ],
      ["moz:useNonSpecCompliantPointerOrigin", false],
      ["moz:webdriverClick", true],
      ["moz:windowless", false],
    ]);
  }

  /**
   * @param {string} key
   *     Capability key.
   * @param {(string|number|boolean)} value
   *     JSON-safe capability value.
   */
  set(key, value) {
    if (key === "timeouts" && !(value instanceof Timeouts)) {
      throw new TypeError();
    } else if (key === "proxy" && !(value instanceof Proxy)) {
      throw new TypeError();
    }

    return super.set(key, value);
  }

  toString() {
    return "[object Capabilities]";
  }

  /**
   * JSON serialisation of capabilities object.
   *
   * @return {Object.<string, ?>}
   */
  toJSON() {
    let marshalled = marshal(this);

    // Always return the proxy capability even if it's empty
    if (!("proxy" in marshalled)) {
      marshalled.proxy = {};
    }

    marshalled.timeouts = super.get("timeouts");

    return marshalled;
  }

  /**
   * Unmarshal a JSON object representation of WebDriver capabilities.
   *
   * @param {Object.<string, *>=} json
   *     WebDriver capabilities.
   *
   * @return {Capabilities}
   *     Internal representation of WebDriver capabilities.
   */
  static fromJSON(json) {
    if (typeof json == "undefined" || json === null) {
      json = {};
    }
    lazy.assert.object(
      json,
      lazy.pprint`Expected "capabilities" to be an object, got ${json}"`
    );

    return Capabilities.match_(json);
  }

  // Matches capabilities as described by WebDriver.
  static match_(json = {}) {
    let matched = new Capabilities();

    for (let [k, v] of Object.entries(json)) {
      switch (k) {
        case "acceptInsecureCerts":
          lazy.assert.boolean(
            v,
            lazy.pprint`Expected ${k} to be a boolean, got ${v}`
          );
          break;

        case "pageLoadStrategy":
          lazy.assert.string(
            v,
            lazy.pprint`Expected ${k} to be a string, got ${v}`
          );
          if (!Object.values(PageLoadStrategy).includes(v)) {
            throw new lazy.error.InvalidArgumentError(
              "Unknown page load strategy: " + v
            );
          }
          break;

        case "proxy":
          v = Proxy.fromJSON(v);
          break;

        case "setWindowRect":
          lazy.assert.boolean(
            v,
            lazy.pprint`Expected ${k} to be boolean, got ${v}`
          );
          if (!lazy.AppInfo.isAndroid && !v) {
            throw new lazy.error.InvalidArgumentError(
              "setWindowRect cannot be disabled"
            );
          } else if (lazy.AppInfo.isAndroid && v) {
            throw new lazy.error.InvalidArgumentError(
              "setWindowRect is only supported on desktop"
            );
          }
          break;

        case "timeouts":
          v = Timeouts.fromJSON(v);
          break;

        case "strictFileInteractability":
          v = lazy.assert.boolean(v);
          break;

        case "unhandledPromptBehavior":
          lazy.assert.string(
            v,
            lazy.pprint`Expected ${k} to be a string, got ${v}`
          );
          if (!Object.values(UnhandledPromptBehavior).includes(v)) {
            throw new lazy.error.InvalidArgumentError(
              `Unknown unhandled prompt behavior: ${v}`
            );
          }
          break;

        case "webSocketUrl":
          lazy.assert.boolean(
            v,
            lazy.pprint`Expected ${k} to be boolean, got ${v}`
          );

          if (!v) {
            throw new lazy.error.InvalidArgumentError(
              lazy.pprint`Expected ${k} to be true, got ${v}`
            );
          }
          break;

        case "moz:accessibilityChecks":
          lazy.assert.boolean(
            v,
            lazy.pprint`Expected ${k} to be boolean, got ${v}`
          );
          break;

        // Don't set the value because it's only used to return the address
        // of the Remote Agent's debugger (HTTP server).
        case "moz:debuggerAddress":
          continue;

        case "moz:useNonSpecCompliantPointerOrigin":
          lazy.assert.boolean(
            v,
            lazy.pprint`Expected ${k} to be boolean, got ${v}`
          );
          break;

        case "moz:webdriverClick":
          lazy.assert.boolean(
            v,
            lazy.pprint`Expected ${k} to be boolean, got ${v}`
          );
          break;

        case "moz:windowless":
          lazy.assert.boolean(
            v,
            lazy.pprint`Expected ${k} to be boolean, got ${v}`
          );

          // Only supported on MacOS
          if (v && !lazy.AppInfo.isMac) {
            throw new lazy.error.InvalidArgumentError(
              "moz:windowless only supported on MacOS"
            );
          }
          break;
      }

      matched.set(k, v);
    }

    return matched;
  }
}

function getWebDriverBrowserName() {
  // Similar to chromedriver which reports "chrome" as browser name for all
  // WebView apps, we will report "firefox" for all GeckoView apps.
  if (lazy.AppInfo.isAndroid) {
    return "firefox";
  }

  return lazy.AppInfo.name?.toLowerCase();
}

function getWebDriverPlatformName() {
  let name = Services.sysinfo.getProperty("name");

  if (lazy.AppInfo.isAndroid) {
    return "android";
  }

  switch (name) {
    case "Windows_NT":
      return "windows";

    case "Darwin":
      return "mac";

    default:
      return name.toLowerCase();
  }
}

// Specialisation of |JSON.stringify| that produces JSON-safe object
// literals, dropping empty objects and entries which values are undefined
// or null. Objects are allowed to produce their own JSON representations
// by implementing a |toJSON| function.
function marshal(obj) {
  let rv = Object.create(null);

  function* iter(mapOrObject) {
    if (mapOrObject instanceof Map) {
      for (const [k, v] of mapOrObject) {
        yield [k, v];
      }
    } else {
      for (const k of Object.keys(mapOrObject)) {
        yield [k, mapOrObject[k]];
      }
    }
  }

  for (let [k, v] of iter(obj)) {
    // Skip empty values when serialising to JSON.
    if (typeof v == "undefined" || v === null) {
      continue;
    }

    // Recursively marshal objects that are able to produce their own
    // JSON representation.
    if (typeof v.toJSON == "function") {
      v = marshal(v.toJSON());

      // Or do the same for object literals.
    } else if (isObject(v)) {
      v = marshal(v);
    }

    // And finally drop (possibly marshaled) objects which have no
    // entries.
    if (!isObjectEmpty(v)) {
      rv[k] = v;
    }
  }

  return rv;
}

function isObject(obj) {
  return Object.prototype.toString.call(obj) == "[object Object]";
}

function isObjectEmpty(obj) {
  return isObject(obj) && Object.keys(obj).length === 0;
}

// Services.dirsvc is not accessible from JSWindowActor child,
// but we should not panic about that.
function maybeProfile() {
  try {
    return Services.dirsvc.get("ProfD", Ci.nsIFile).path;
  } catch (e) {
    return "<protected>";
  }
}
