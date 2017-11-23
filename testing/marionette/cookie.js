/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");

Cu.import("chrome://marionette/content/assert.js");
const {
  InvalidCookieDomainError,
  UnableToSetCookieError,
} = Cu.import("chrome://marionette/content/error.js", {});
const {pprint} = Cu.import("chrome://marionette/content/format.js", {});

this.EXPORTED_SYMBOLS = ["cookie"];

const IPV4_PORT_EXPR = /:\d+$/;

/** @namespace */
this.cookie = {
  manager: Services.cookies,
};

/**
 * @name Cookie
 *
 * @return {Object.<string, (number|boolean|string)>
 */

/**
 * Unmarshal a JSON Object to a cookie representation.
 *
 * Effectively this will run validation checks on |json|, which will
 * produce the errors expected by WebDriver if the input is not valid.
 *
 * @param {Object.<string, (number|boolean|string)>} json
 *     Cookie to be deserialised. <var>name</var> and <var>value</var>
 *     are required fields which must be strings. The <var>path</var> and
 *     <var>domain</var> fields are optional, but must be a string if
 *     provided.
 *     The <var>secure</var>, and <var>httpOnly</var> are similarly
 *     optional, but must be booleans. Likewise, the <var>expiry</var> field
 *     is optional but must be unsigned integer.
 *
 * @return {Cookie}
 *     Valid cookie object.
 *
 * @throws {InvalidArgumentError}
 *     If any of the properties are invalid.
 */
cookie.fromJSON = function(json) {
  let newCookie = {};

  assert.object(json, pprint`Expected cookie object, got ${json}`);

  newCookie.name = assert.string(json.name, "Cookie name must be string");
  newCookie.value = assert.string(json.value, "Cookie value must be string");

  if (typeof json.path != "undefined") {
    newCookie.path = assert.string(json.path, "Cookie path must be string");
  }
  if (typeof json.domain != "undefined") {
    newCookie.domain = assert.string(json.domain, "Cookie domain must be string");
  }
  if (typeof json.secure != "undefined") {
    newCookie.secure = assert.boolean(json.secure, "Cookie secure flag must be boolean");
  }
  if (typeof json.httpOnly != "undefined") {
    newCookie.httpOnly = assert.boolean(json.httpOnly, "Cookie httpOnly flag must be boolean");
  }
  if (typeof json.expiry != "undefined") {
    newCookie.expiry = assert.positiveInteger(json.expiry, "Cookie expiry must be a positive integer");
  }

  return newCookie;
};

/**
 * Insert cookie to the cookie store.
 *
 * @param {Cookie} newCookie
 *     Cookie to add.
 * @param {string=} restrictToHost
 *     Perform test that <var>newCookie</var>'s domain matches this.
 *
 * @throws {TypeError}
 *     If <var>name</var>, <var>value</var>, or <var>domain</var> are
 *     not present and of the correct type.
 * @throws {InvalidCookieDomainError}
 *     If <var>restrictToHost</var> is set and <var>newCookie</var>'s
 *     domain does not match.
 * @throws {UnableToSetCookieError}
 *     If an error occurred while trying to save the cookie.
 */
cookie.add = function(newCookie, {restrictToHost = null} = {}) {
  assert.string(newCookie.name, "Cookie name must be string");
  assert.string(newCookie.value, "Cookie value must be string");

  if (typeof newCookie.path == "undefined") {
    newCookie.path = "/";
  }

  let hostOnly = false;
  if (typeof newCookie.domain == "undefined") {
    hostOnly = true;
    newCookie.domain = restrictToHost;
  }
  assert.string(newCookie.domain, "Cookie domain must be string");
  if (newCookie.domain.substring(0, 1) === ".") {
    newCookie.domain = newCookie.domain.substring(1);
  }

  if (typeof newCookie.secure == "undefined") {
    newCookie.secure = false;
  }
  if (typeof newCookie.httpOnly == "undefined") {
    newCookie.httpOnly = false;
  }
  if (typeof newCookie.expiry == "undefined") {
    // The XPCOM interface requires the expiry field even for session cookies.
    newCookie.expiry = Number.MAX_SAFE_INTEGER;
    newCookie.session = true;
  } else {
    newCookie.session = false;
  }

  let isIpAddress = false;
  try {
    Services.eTLD.getPublicSuffixFromHost(newCookie.domain);
  } catch (e) {
    switch (e.result) {
      case Cr.NS_ERROR_HOST_IS_IP_ADDRESS:
        isIpAddress = true;
        break;
      default:
        throw new InvalidCookieDomainError(newCookie.domain);
    }
  }

  if (!hostOnly && !isIpAddress) {
    // only store this as a domain cookie if the domain was specified in the
    // request and it wasn't an IP address.
    newCookie.domain = "." + newCookie.domain;
  }

  if (restrictToHost) {
    if (!restrictToHost.endsWith(newCookie.domain) &&
        ("." + restrictToHost) !== newCookie.domain &&
        restrictToHost !== newCookie.domain) {
      throw new InvalidCookieDomainError(`Cookies may only be set ` +
          `for the current domain (${restrictToHost})`);
    }
  }

  // remove port from domain, if present.
  // unfortunately this catches IPv6 addresses by mistake
  // TODO: Bug 814416
  newCookie.domain = newCookie.domain.replace(IPV4_PORT_EXPR, "");

  try {
    cookie.manager.add(
        newCookie.domain,
        newCookie.path,
        newCookie.name,
        newCookie.value,
        newCookie.secure,
        newCookie.httpOnly,
        newCookie.session,
        newCookie.expiry,
        {} /* origin attributes */);
  } catch (e) {
    throw new UnableToSetCookieError(e);
  }
};

/**
 * Remove cookie from the cookie store.
 *
 * @param {Cookie} toDelete
 *     Cookie to remove.
 */
cookie.remove = function(toDelete) {
  cookie.manager.remove(
      toDelete.domain,
      toDelete.name,
      toDelete.path,
      false,
      {} /* originAttributes */);
};

/**
 * Iterates over the cookies for the current <var>host</var>.  You may
 * optionally filter for specific paths on that <var>host</var> by
 * specifying a path in <var>currentPath</var>.
 *
 * @param {string} host
 *     Hostname to retrieve cookies for.
 * @param {string=} [currentPath="/"] currentPath
 *     Optionally filter the cookies for <var>host</var> for the
 *     specific path.  Defaults to "<tt>/</tt>", meaning all cookies
 *     for <var>host</var> are included.
 *
 * @return {Iterable.<Cookie>}
 *     Iterator.
 */
cookie.iter = function* (host, currentPath = "/") {
  assert.string(host, "host must be string");
  assert.string(currentPath, "currentPath must be string");

  const isForCurrentPath = path => currentPath.indexOf(path) != -1;

  let en = cookie.manager.getCookiesFromHost(host, {});
  while (en.hasMoreElements()) {
    let cookie = en.getNext().QueryInterface(Ci.nsICookie2);
    // take the hostname and progressively shorten
    let hostname = host;
    do {
      if ((cookie.host == "." + hostname || cookie.host == hostname) &&
          isForCurrentPath(cookie.path)) {
        let data = {
          "name": cookie.name,
          "value": cookie.value,
          "path": cookie.path,
          "domain": cookie.host,
          "secure": cookie.isSecure,
          "httpOnly": cookie.isHttpOnly,
        };

        if (!cookie.isSession) {
          data.expiry = cookie.expiry;
        }

        yield data;
      }
      hostname = hostname.replace(/^.*?\./, "");
    } while (hostname.indexOf(".") != -1);
  }
};
