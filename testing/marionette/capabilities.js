/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Preferences.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.import("chrome://marionette/content/assert.js");
const {
  InvalidArgumentError,
} = ChromeUtils.import("chrome://marionette/content/error.js", {});
const {
  pprint,
} = ChromeUtils.import("chrome://marionette/content/format.js", {});

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

this.EXPORTED_SYMBOLS = [
  "Capabilities",
  "PageLoadStrategy",
  "Proxy",
  "Timeouts",
];

// Enable testing this module, as Services.appinfo.* is not available
// in xpcshell tests.
const appinfo = {name: "<missing>", version: "<missing>"};
try { appinfo.name = Services.appinfo.name.toLowerCase(); } catch (e) {}
try { appinfo.version = Services.appinfo.version; } catch (e) {}

/** Representation of WebDriver session timeouts. */
class Timeouts {
  constructor() {
    // disabled
    this.implicit = 0;
    // five mintues
    this.pageLoad = 300000;
    // 30 seconds
    this.script = 30000;
  }

  toString() { return "[object Timeouts]"; }

  /** Marshals timeout durations to a JSON Object. */
  toJSON() {
    return {
      implicit: this.implicit,
      pageLoad: this.pageLoad,
      script: this.script,
    };
  }

  static fromJSON(json) {
    assert.object(json,
        pprint`Expected "timeouts" to be an object, got ${json}`);
    let t = new Timeouts();

    for (let [type, ms] of Object.entries(json)) {
      switch (type) {
        case "implicit":
          t.implicit = assert.positiveInteger(ms,
              pprint`Expected ${type} to be a positive integer, got ${ms}`);
          break;

        case "script":
          t.script = assert.positiveInteger(ms,
              pprint`Expected ${type} to be a positive integer, got ${ms}`);
          break;

        case "pageLoad":
          t.pageLoad = assert.positiveInteger(ms,
              pprint`Expected ${type} to be a positive integer, got ${ms}`);
          break;

        default:
          throw new InvalidArgumentError("Unrecognised timeout: " + type);
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
    this.ftpProxy = null;
    this.ftpProxyPort = null;
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
        Preferences.set("network.proxy.type", 4);
        return true;

      case "direct":
        Preferences.set("network.proxy.type", 0);
        return true;

      case "manual":
        Preferences.set("network.proxy.type", 1);

        if (this.ftpProxy) {
          Preferences.set("network.proxy.ftp", this.ftpProxy);
          if (Number.isInteger(this.ftpProxyPort)) {
            Preferences.set("network.proxy.ftp_port", this.ftpProxyPort);
          }
        }

        if (this.httpProxy) {
          Preferences.set("network.proxy.http", this.httpProxy);
          if (Number.isInteger(this.httpProxyPort)) {
            Preferences.set("network.proxy.http_port", this.httpProxyPort);
          }
        }

        if (this.sslProxy) {
          Preferences.set("network.proxy.ssl", this.sslProxy);
          if (Number.isInteger(this.sslProxyPort)) {
            Preferences.set("network.proxy.ssl_port", this.sslProxyPort);
          }
        }

        if (this.socksProxy) {
          Preferences.set("network.proxy.socks", this.socksProxy);
          if (Number.isInteger(this.socksProxyPort)) {
            Preferences.set("network.proxy.socks_port", this.socksProxyPort);
          }
          if (this.socksVersion) {
            Preferences.set("network.proxy.socks_version", this.socksVersion);
          }
        }

        if (this.noProxy) {
          Preferences.set("network.proxy.no_proxies_on", this.noProxy.join(", "));
        }
        return true;

      case "pac":
        Preferences.set("network.proxy.type", 2);
        Preferences.set(
            "network.proxy.autoconfig_url", this.proxyAutoconfigUrl);
        return true;

      case "system":
        Preferences.set("network.proxy.type", 5);
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
      return hostname.includes(":") ? hostname.replace(/[\[\]]/g, "") : hostname;
    }

    // Parse hostname and optional port from host
    function fromHost(scheme, host) {
      assert.string(host,
          pprint`Expected proxy "host" to be a string, got ${host}`);

      if (host.includes("://")) {
        throw new InvalidArgumentError(`${host} contains a scheme`);
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
        throw new InvalidArgumentError(e.message);
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

      if (url.username != "" ||
          url.password != "" ||
          url.pathname != "/" ||
          url.search != "" ||
          url.hash != "") {
        throw new InvalidArgumentError(
            `${host} was not of the form host[:port]`);
      }

      return [hostname, port];
    }

    let p = new Proxy();
    if (typeof json == "undefined" || json === null) {
      return p;
    }

    assert.object(json, pprint`Expected "proxy" to be an object, got ${json}`);

    assert.in("proxyType", json,
        pprint`Expected "proxyType" in "proxy" object, got ${json}`);
    p.proxyType = assert.string(json.proxyType,
        pprint`Expected "proxyType" to be a string, got ${json.proxyType}`);

    switch (p.proxyType) {
      case "autodetect":
      case "direct":
      case "system":
        break;

      case "pac":
        p.proxyAutoconfigUrl = assert.string(json.proxyAutoconfigUrl,
            `Expected "proxyAutoconfigUrl" to be a string, ` +
            pprint`got ${json.proxyAutoconfigUrl}`);
        break;

      case "manual":
        if (typeof json.ftpProxy != "undefined") {
          [p.ftpProxy, p.ftpProxyPort] = fromHost("ftp", json.ftpProxy);
        }
        if (typeof json.httpProxy != "undefined") {
          [p.httpProxy, p.httpProxyPort] = fromHost("http", json.httpProxy);
        }
        if (typeof json.sslProxy != "undefined") {
          [p.sslProxy, p.sslProxyPort] = fromHost("https", json.sslProxy);
        }
        if (typeof json.socksProxy != "undefined") {
          [p.socksProxy, p.socksProxyPort] = fromHost("socks", json.socksProxy);
          p.socksVersion = assert.positiveInteger(json.socksVersion,
              pprint`Expected "socksVersion" to be a positive integer, got ${json.socksVersion}`);
        }
        if (typeof json.noProxy != "undefined") {
          let entries = assert.array(json.noProxy,
              pprint`Expected "noProxy" to be an array, got ${json.noProxy}`);
          p.noProxy = entries.map(entry => {
            assert.string(entry,
                pprint`Expected "noProxy" entry to be a string, got ${entry}`);
            return stripBracketsFromIpv6Hostname(entry);
          });
        }
        break;

      default:
        throw new InvalidArgumentError(
            `Invalid type of proxy: ${p.proxyType}`);
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
      ftpProxy: toHost(this.ftpProxy, this.ftpProxyPort),
      httpProxy: toHost(this.httpProxy, this.httpProxyPort),
      noProxy: excludes,
      sslProxy: toHost(this.sslProxy, this.sslProxyPort),
      socksProxy: toHost(this.socksProxy, this.socksProxyPort),
      socksVersion: this.socksVersion,
      proxyAutoconfigUrl: this.proxyAutoconfigUrl,
    });
  }

  toString() { return "[object Proxy]"; }
}

/** WebDriver session capabilities representation. */
class Capabilities extends Map {
  /** @class */
  constructor() {
    super([
      // webdriver
      ["browserName", appinfo.name],
      ["browserVersion", appinfo.version],
      ["platformName", Services.sysinfo.getProperty("name").toLowerCase()],
      ["platformVersion", Services.sysinfo.getProperty("version")],
      ["pageLoadStrategy", PageLoadStrategy.Normal],
      ["acceptInsecureCerts", false],
      ["timeouts", new Timeouts()],
      ["proxy", new Proxy()],

      // features
      ["rotatable", appinfo.name == "B2G"],

      // proprietary
      ["moz:accessibilityChecks", false],
      ["moz:headless", Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo).isHeadless],
      ["moz:processID", Services.appinfo.processID],
      ["moz:profile", maybeProfile()],
      ["moz:useNonSpecCompliantPointerOrigin", false],
      ["moz:webdriverClick", true],
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

  toString() { return "[object Capabilities]"; }

  /**
   * JSON serialisation of capabilities object.
   *
   * @return {Object.<string, ?>}
   */
  toJSON() {
    return marshal(this);
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
    assert.object(json,
        pprint`Expected "capabilities" to be an object, got ${json}"`);

    return Capabilities.match_(json);
  }

  // Matches capabilities as described by WebDriver.
  static match_(json = {}) {
    let matched = new Capabilities();

    for (let [k, v] of Object.entries(json)) {
      switch (k) {
        case "acceptInsecureCerts":
          assert.boolean(v,
              pprint`Expected ${k} to be a boolean, got ${v}`);
          matched.set("acceptInsecureCerts", v);
          break;

        case "pageLoadStrategy":
          if (v === null) {
            matched.set("pageLoadStrategy", PageLoadStrategy.Normal);
          } else {
            assert.string(v,
                pprint`Expected ${k} to be a string, got ${v}`);

            if (Object.values(PageLoadStrategy).includes(v)) {
              matched.set("pageLoadStrategy", v);
            } else {
              throw new InvalidArgumentError(
                  "Unknown page load strategy: " + v);
            }
          }

          break;

        case "proxy":
          let proxy = Proxy.fromJSON(v);
          matched.set("proxy", proxy);
          break;

        case "timeouts":
          let timeouts = Timeouts.fromJSON(v);
          matched.set("timeouts", timeouts);
          break;

        case "moz:accessibilityChecks":
          assert.boolean(v,
              pprint`Expected ${k} to be a boolean, got ${v}`);
          matched.set("moz:accessibilityChecks", v);
          break;

        case "moz:useNonSpecCompliantPointerOrigin":
          assert.boolean(v,
              pprint`Expected ${k} to be a boolean, got ${v}`);
          matched.set("moz:useNonSpecCompliantPointerOrigin", v);
          break;

        case "moz:webdriverClick":
          assert.boolean(v,
              pprint`Expected ${k} to be a boolean, got ${v}`);
          matched.set("moz:webdriverClick", v);
          break;
      }
    }

    return matched;
  }
}

this.Capabilities = Capabilities;
this.PageLoadStrategy = PageLoadStrategy;
this.Proxy = Proxy;
this.Timeouts = Timeouts;

// Specialisation of |JSON.stringify| that produces JSON-safe object
// literals, dropping empty objects and entries which values are undefined
// or null.  Objects are allowed to produce their own JSON representations
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

// Services.dirsvc is not accessible from content frame scripts,
// but we should not panic about that.
function maybeProfile() {
  try {
    return Services.dirsvc.get("ProfD", Ci.nsIFile).path;
  } catch (e) {
    return "<protected>";
  }
}
