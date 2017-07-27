/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");

Cu.import("chrome://marionette/content/assert.js");
const {
  error,
  InvalidArgumentError,
} = Cu.import("chrome://marionette/content/error.js", {});

this.EXPORTED_SYMBOLS = ["session"];

const logger = Log.repository.getLogger("Marionette");
const {pprint} = error;

// Enable testing this module, as Services.appinfo.* is not available
// in xpcshell tests.
const appinfo = {name: "<missing>", version: "<missing>"};
try { appinfo.name = Services.appinfo.name.toLowerCase(); } catch (e) {}
try { appinfo.version = Services.appinfo.version; } catch (e) {}

/**
 * State associated with a WebDriver session.
 *
 * @namespace
 */
this.session = {};

/** Representation of WebDriver session timeouts. */
session.Timeouts = class {
  constructor() {
    // disabled
    this.implicit = 0;
    // five mintues
    this.pageLoad = 300000;
    // 30 seconds
    this.script = 30000;
  }

  toString() { return "[object session.Timeouts]"; }

  /** Marshals timeout durations to a JSON Object. */
  toJSON() {
    return {
      implicit: this.implicit,
      pageLoad: this.pageLoad,
      script: this.script,
    };
  }

  static fromJSON(json) {
    assert.object(json);
    let t = new session.Timeouts();

    for (let [typ, ms] of Object.entries(json)) {
      switch (typ) {
        case "implicit":
          t.implicit = assert.positiveInteger(ms);
          break;

        case "script":
          t.script = assert.positiveInteger(ms);
          break;

        case "pageLoad":
          t.pageLoad = assert.positiveInteger(ms);
          break;

        default:
          throw new InvalidArgumentError("Unrecognised timeout: " + typ);
      }
    }

    return t;
  }
};

/**
 * Enum of page loading strategies.
 *
 * @enum
 */
session.PageLoadStrategy = {
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
session.Proxy = class {
  /** @class */
  constructor() {
    this.proxyType = null;
    this.httpProxy = null;
    this.httpProxyPort = null;
    this.sslProxy = null;
    this.sslProxyPort = null;
    this.ftpProxy = null;
    this.ftpProxyPort = null;
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
      case "manual":
        Preferences.set("network.proxy.type", 1);
        if (this.httpProxy && this.httpProxyPort) {
          Preferences.set("network.proxy.http", this.httpProxy);
          Preferences.set("network.proxy.http_port", this.httpProxyPort);
        }
        if (this.sslProxy && this.sslProxyPort) {
          Preferences.set("network.proxy.ssl", this.sslProxy);
          Preferences.set("network.proxy.ssl_port", this.sslProxyPort);
        }
        if (this.ftpProxy && this.ftpProxyPort) {
          Preferences.set("network.proxy.ftp", this.ftpProxy);
          Preferences.set("network.proxy.ftp_port", this.ftpProxyPort);
        }
        if (this.socksProxy) {
          Preferences.set("network.proxy.socks", this.socksProxy);
          Preferences.set("network.proxy.socks_port", this.socksProxyPort);
          if (this.socksVersion) {
            Preferences.set("network.proxy.socks_version", this.socksVersion);
          }
        }
        return true;

      case "pac":
        Preferences.set("network.proxy.type", 2);
        Preferences.set(
            "network.proxy.autoconfig_url", this.proxyAutoconfigUrl);
        return true;

      case "autodetect":
        Preferences.set("network.proxy.type", 4);
        return true;

      case "system":
        Preferences.set("network.proxy.type", 5);
        return true;

      case "noproxy":
        Preferences.set("network.proxy.type", 0);
        return true;

      default:
        return false;
    }
  }

  toString() { return "[object session.Proxy]"; }

  /**
   * @return {Object.<string, (number|string)>}
   *     JSON serialisation of proxy object.
   */
  toJSON() {
    return marshal({
      proxyType: this.proxyType,
      httpProxy: this.httpProxy,
      httpProxyPort: this.httpProxyPort,
      sslProxy: this.sslProxy,
      sslProxyPort: this.sslProxyPort,
      ftpProxy: this.ftpProxy,
      ftpProxyPort: this.ftpProxyPort,
      socksProxy: this.socksProxy,
      socksProxyPort: this.socksProxyPort,
      socksProxyVersion: this.socksProxyVersion,
      proxyAutoconfigUrl: this.proxyAutoconfigUrl,
    });
  }

  /**
   * @param {Object.<string, ?>} json
   *     JSON Object to unmarshal.
   */
  static fromJSON(json) {
    let p = new session.Proxy();
    if (typeof json == "undefined" || json === null) {
      return p;
    }

    assert.object(json);

    assert.in("proxyType", json);
    p.proxyType = json.proxyType;

    if (json.proxyType == "manual") {
      if (typeof json.httpProxy != "undefined") {
        p.httpProxy = assert.string(json.httpProxy);
        p.httpProxyPort = assert.positiveInteger(json.httpProxyPort);
      }

      if (typeof json.sslProxy != "undefined") {
        p.sslProxy = assert.string(json.sslProxy);
        p.sslProxyPort = assert.positiveInteger(json.sslProxyPort);
      }

      if (typeof json.ftpProxy != "undefined") {
        p.ftpProxy = assert.string(json.ftpProxy);
        p.ftpProxyPort = assert.positiveInteger(json.ftpProxyPort);
      }

      if (typeof json.socksProxy != "undefined") {
        p.socksProxy = assert.string(json.socksProxy);
        p.socksProxyPort = assert.positiveInteger(json.socksProxyPort);
        p.socksProxyVersion = assert.positiveInteger(json.socksProxyVersion);
      }
    }

    if (typeof json.proxyAutoconfigUrl != "undefined") {
      p.proxyAutoconfigUrl = assert.string(json.proxyAutoconfigUrl);
    }

    return p;
  }
};

/** WebDriver session capabilities representation. */
session.Capabilities = class extends Map {
  /** @class */
  constructor() {
    super([
      // webdriver
      ["browserName", appinfo.name],
      ["browserVersion", appinfo.version],
      ["platformName", Services.sysinfo.getProperty("name").toLowerCase()],
      ["platformVersion", Services.sysinfo.getProperty("version")],
      ["pageLoadStrategy", session.PageLoadStrategy.Normal],
      ["acceptInsecureCerts", false],
      ["timeouts", new session.Timeouts()],
      ["proxy", new session.Proxy()],

      // features
      ["rotatable", appinfo.name == "B2G"],

      // proprietary
      ["specificationLevel", 0],
      ["moz:processID", Services.appinfo.processID],
      ["moz:profile", maybeProfile()],
      ["moz:accessibilityChecks", false],
    ]);
  }

  /**
   * @param {string} key
   *     Capability name.
   * @param {(string|number|boolean)} value
   *     JSON-safe capability value.
   */
  set(key, value) {
    if (key === "timeouts" && !(value instanceof session.Timeouts)) {
      throw new TypeError();
    } else if (key === "proxy" && !(value instanceof session.Proxy)) {
      throw new TypeError();
    }

    return super.set(key, value);
  }

  /** @return {string} */
  toString() { return "[object session.Capabilities]"; }

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
   * @param {Object.<string, ?>=} json
   *     WebDriver capabilities.
   * @param {boolean=} merge
   *     If providing <var>json</var> with <tt>desiredCapabilities</tt> or
   *     <tt>requiredCapabilities</tt> fields, or both, it should be
   *     set to true to merge these before parsing.  This indicates that
   *     the input provided is from a client and not from
   *     {@link session.Capabilities#toJSON}.
   *
   * @return {session.Capabilities}
   *     Internal representation of WebDriver capabilities.
   */
  static fromJSON(json, {merge = false} = {}) {
    if (typeof json == "undefined" || json === null) {
      json = {};
    }
    assert.object(json);

    if (merge) {
      json = session.Capabilities.merge_(json);
    }
    return session.Capabilities.match_(json);
  }

  // Processes capabilities as described by WebDriver.
  static merge_(json) {
    for (let entry of [json.desiredCapabilities, json.requiredCapabilities]) {
      if (typeof entry == "undefined" || entry === null) {
        continue;
      }
      assert.object(entry,
          error.pprint`Expected ${entry} to be a capabilities object`);
    }

    let desired = json.desiredCapabilities || {};
    let required = json.requiredCapabilities || {};

    // One level deep union merge of desired- and required capabilities
    // with preference on required
    return Object.assign({}, desired, required);
  }

  // Matches capabilities as described by WebDriver.
  static match_(caps = {}) {
    let matched = new session.Capabilities();

    const defined = v => typeof v != "undefined" && v !== null;
    const wildcard = v => v === "*";

    // Iff |actual| provides some value, or is a wildcard or an exact
    // match of |expected|.  This means it can be null or undefined,
    // or "*", or "firefox".
    function stringMatch(actual, expected) {
      return !defined(actual) || (wildcard(actual) || actual === expected);
    }

    for (let [k, v] of Object.entries(caps)) {
      switch (k) {
        case "browserName":
          let bname = matched.get("browserName");
          if (!stringMatch(v, bname)) {
            throw new TypeError(
                pprint`Given browserName ${v}, but my name is ${bname}`);
          }
          break;

        // TODO(ato): bug 1326397
        case "browserVersion":
          let bversion = matched.get("browserVersion");
          if (!stringMatch(v, bversion)) {
            throw new TypeError(
                pprint`Given browserVersion ${v}, ` +
                pprint`but current version is ${bversion}`);
          }
          break;

        case "platformName":
          let pname = matched.get("platformName");
          if (!stringMatch(v, pname)) {
            throw new TypeError(
                pprint`Given platformName ${v}, ` +
                pprint`but current platform is ${pname}`);
          }
          break;

        // TODO(ato): bug 1326397
        case "platformVersion":
          let pversion = matched.get("platformVersion");
          if (!stringMatch(v, pversion)) {
            throw new TypeError(
                pprint`Given platformVersion ${v}, ` +
                pprint`but current platform version is ${pversion}`);
          }
          break;

        case "acceptInsecureCerts":
          assert.boolean(v);
          matched.set("acceptInsecureCerts", v);
          break;

        case "pageLoadStrategy":
          if (v === null) {
            matched.set("pageLoadStrategy", session.PageLoadStrategy.Normal);
          } else {
            assert.string(v);

            if (Object.values(session.PageLoadStrategy).includes(v)) {
              matched.set("pageLoadStrategy", v);
            } else {
              throw new InvalidArgumentError(
                  "Unknown page load strategy: " + v);
            }
          }

          break;

        case "proxy":
          let proxy = session.Proxy.fromJSON(v);
          matched.set("proxy", proxy);
          break;

        case "timeouts":
          let timeouts = session.Timeouts.fromJSON(v);
          matched.set("timeouts", timeouts);
          break;

        case "specificationLevel":
          assert.positiveInteger(v);
          matched.set("specificationLevel", v);
          break;

        case "moz:accessibilityChecks":
          assert.boolean(v);
          matched.set("moz:accessibilityChecks", v);
          break;
      }
    }

    return matched;
  }
};

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
