/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

Cu.import("chrome://marionette/content/assert.js");
const {
  error,
  InvalidCookieDomainError,
} = Cu.import("chrome://marionette/content/error.js", {});

this.EXPORTED_SYMBOLS = ["cookie"];

const IPV4_PORT_EXPR = /:\d+$/;

this.cookie = {
  manager: Services.cookies,
};

/**
 * Unmarshal a JSON Object to a cookie representation.
 *
 * Effectively this will run validation checks on |json|, which will
 * produce the errors expected by WebDriver if the input is not valid.
 *
 * @param {Map.<string, (number|boolean|string)> json
 *     Cookie to be deserialised.  |name| and |value| are required fields
 *     which must be strings.  The |path| field is optional, but must
 *     be a string if provided.  The |secure|, |httpOnly|, and |session|
 *     fields are similarly optional, but must be booleans.  Likewise,
 *     the |expiry| field is optional but must be unsigned integer.
 *
 * @return {Map.<string, (number|boolean|string)>
 *     Valid cookie object.
 *
 * @throws {InvalidArgumentError}
 *     If any of the properties are invalid.
 */
cookie.fromJSON = function(json) {
  let newCookie = {};

  assert.object(json, error.pprint`Expected cookie object, got ${json}`);

  newCookie.name = assert.string(json.name, "Cookie name must be string");
  newCookie.value = assert.string(json.value, "Cookie value must be string");

  if (typeof json.path != "undefined") {
    newCookie.path = assert.string(json.path, "Cookie path must be string");
  }
  if (typeof json.secure != "undefined") {
    newCookie.secure = assert.boolean(json.secure, "Cookie secure flag must be boolean");
  }
  if (typeof json.httpOnly != "undefined") {
    newCookie.httpOnly = assert.boolean(json.httpOnly, "Cookie httpOnly flag must be boolean");
  }
  if (typeof json.session != "undefined") {
    newCookie.session = assert.boolean(json.session, "Cookie session flag must be boolean");
  }
  if (typeof json.expiry != "undefined") {
    newCookie.expiry = assert.positiveInteger(json.expiry, "Cookie expiry must be a positive integer");
  }

  return newCookie;
};

/**
 * Insert cookie to the cookie store.
 *
 * @param {Map.<string, (string|number|boolean)} newCookie
 *     Cookie to add.
 * @param {Map.<string, ?>} opts
 *     Optional parameters:
 *
 *       restrictToHost (string)
 *         Perform test that |newCookie|'s domain matches this.
 *
 * @throws {TypeError}
 *     If |name|, |value|, or |domain| are not present and of the
 *     correct type.
 * @throws {InvalidCookieDomainError}
 *     If |restrictToHost| is set and |newCookie|'s domain does not match.
 */
cookie.add = function(newCookie, opts = {}) {
  assert.string(newCookie.name, "Cookie name must be string");
  assert.string(newCookie.value, "Cookie value must be string");
  assert.string(newCookie.domain, "Cookie domain must be string");

  if (typeof newCookie.path == "undefined") {
    newCookie.path = "/";
  }

  if (typeof newCookie.expiry == "undefined") {
    // twenty years into the future
    let date = new Date();
    let now = new Date(Date.now());
    date.setYear(now.getFullYear() + 20);
    newCookie.expiry = date.getTime() / 1000;
  }

  if (opts.restrictToHost) {
    assert.in("restrictToHost", opts,
        "Missing cookie domain for host restriction test");

    if (newCookie.domain !== opts.restrictToHost) {
      throw new InvalidCookieDomainError(
          `Cookies may only be set ` +
          ` for the current domain (${opts.restrictToHost})`);
    }
  }

  // remove port from domain, if present.
  // unfortunately this catches IPv6 addresses by mistake
  // TODO: Bug 814416
  newCookie.domain = newCookie.domain.replace(IPV4_PORT_EXPR, "");

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
};

/**
 * Remove cookie from the cookie store.
 *
 * @param {Map.<string, (string|number|boolean)} toDelete
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
 * Iterates over the cookies for the current |host|.  You may optionally
 * filter for specific paths on that |host| by specifying a path in
 * |currentPath|.
 *
 * @param {string} host
 *     Hostname to retrieve cookies for.
 * @param {string=} currentPath
 *     Optionally filter the cookies for |host| for the specific path.
 *     Defautls to "/", meaning all cookies for |host| are included.
 *
 * @return {[Symbol.Iterator]}
 *     Iterator.
 */
cookie.iter = function*(host, currentPath = "/") {
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
        yield {
          "name": cookie.name,
          "value": cookie.value,
          "path": cookie.path,
          "domain": cookie.host,
          "secure": cookie.isSecure,
          "httpOnly": cookie.isHttpOnly,
          "expiry": cookie.expiry,
        };
      }
      hostname = hostname.replace(/^.*?\./, "");
    } while (hostname.indexOf(".") != -1);
  }
};
