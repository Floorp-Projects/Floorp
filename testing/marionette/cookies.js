/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("chrome://marionette/content/error.js");

const logger = Log.repository.getLogger("Marionette");

this.EXPORTED_SYMBOLS = ["Cookies"];

const IPV4_PORT_EXPR = /:\d+$/;

/**
 * Interface for manipulating cookies from content space.
 */
this.Cookies = class {

  /**
   * @param {function(): Document} documentFn
   *     Closure that returns the current content document.
   * @param {Proxy(SyncChromeSender)} chromeProxy
   *     A synchronous proxy interface to chrome space.
   */
  constructor(documentFn, chromeProxy) {
    this.documentFn_ = documentFn;
    this.chrome = chromeProxy;
  }

  get document() {
    return this.documentFn_();
  }

  [Symbol.iterator]() {
    let path = this.document.location.pathname || "/";
    let cs = this.chrome.getVisibleCookies(path, this.document.location.hostname)[0];
    return cs[Symbol.iterator]();
  }

  /**
   * Add a new cookie to a content document.
   *
   * @param {string} name
   *     Cookie key.
   * @param {string} value
   *     Cookie value.
   * @param {Object.<string, ?>} opts
   *     An object with the optional fields {@code domain}, {@code path},
   *     {@code secure}, {@code httpOnly}, and {@code expiry}.
   *
   * @return {Object.<string, ?>}
   *     A serialisation of the cookie that was added.
   *
   * @throws UnableToSetCookieError
   *     If the document's content type isn't HTML, the current document's
   *     domain is a mismatch to the cookie's provided domain, or there
   *     otherwise was issues with the input data.
   */
  add(name, value, opts={}) {
    if (typeof this.document == "undefined" || !this.document.contentType.match(/html/i)) {
      throw new UnableToSetCookieError(
          "You may only set cookies on HTML documents: " + this.document.contentType);
    }

    if (!opts.expiry) {
      // date twenty years into future, in seconds
      let date = new Date();
      let now = new Date(Date.now());
      date.setYear(now.getFullYear() + 20);
      opts.expiry = date.getTime() / 1000;
    }

    if (!opts.domain) {
      opts.domain = this.document.location.host;
    } else if (this.document.location.host.indexOf(opts.domain) < 0) {
      throw new InvalidCookieDomainError(
          "You may only set cookies for the current domain");
    }

    // remove port from domain, if present.
    // unfortunately this catches IPv6 addresses by mistake
    // TODO: Bug 814416
    opts.domain = opts.domain.replace(IPV4_PORT_EXPR, "");

    let cookie = {
      domain: opts.domain,
      path: opts.path,
      name: name,
      value: value,
      secure: opts.secure,
      httpOnly: opts.httpOnly,
      session: false,
      expiry: opts.expiry,
    };
    if (!this.chrome.addCookie(cookie)) {
      throw new UnableToSetCookieError();
    }

    return cookie;
  }

  /**
   * Delete cookie by reference or by name.
   *
   * @param {(string|Object.<string, ?>)} cookie
   *     Name of cookie or cookie object.
   *
   * @throws {UnknownError}
   *     If unable to delete the cookie.
   */
  delete(cookie) {
    let name;
    if (cookie.hasOwnProperty("name")) {
      name = cookie.name;
    } else {
      name = cookie;
    }

    for (let candidate of this) {
      if (candidate.name == name) {
        if (!this.chrome.deleteCookie(candidate)) {
          throw new UnknownError("Unable to delete cookie by name: " + name);
        }
      }
    }
  }
};
