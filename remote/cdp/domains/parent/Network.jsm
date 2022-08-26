/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Network"];

const { Domain } = ChromeUtils.import(
  "chrome://remote/content/cdp/domains/Domain.jsm"
);

const MAX_COOKIE_EXPIRY = Number.MAX_SAFE_INTEGER;

const LOAD_CAUSE_STRINGS = {
  [Ci.nsIContentPolicy.TYPE_INVALID]: "Invalid",
  [Ci.nsIContentPolicy.TYPE_OTHER]: "Other",
  [Ci.nsIContentPolicy.TYPE_SCRIPT]: "Script",
  [Ci.nsIContentPolicy.TYPE_IMAGE]: "Img",
  [Ci.nsIContentPolicy.TYPE_STYLESHEET]: "Stylesheet",
  [Ci.nsIContentPolicy.TYPE_OBJECT]: "Object",
  [Ci.nsIContentPolicy.TYPE_DOCUMENT]: "Document",
  [Ci.nsIContentPolicy.TYPE_SUBDOCUMENT]: "Subdocument",
  [Ci.nsIContentPolicy.TYPE_PING]: "Ping",
  [Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST]: "Xhr",
  [Ci.nsIContentPolicy.TYPE_OBJECT_SUBREQUEST]: "ObjectSubdoc",
  [Ci.nsIContentPolicy.TYPE_DTD]: "Dtd",
  [Ci.nsIContentPolicy.TYPE_FONT]: "Font",
  [Ci.nsIContentPolicy.TYPE_MEDIA]: "Media",
  [Ci.nsIContentPolicy.TYPE_WEBSOCKET]: "Websocket",
  [Ci.nsIContentPolicy.TYPE_CSP_REPORT]: "Csp",
  [Ci.nsIContentPolicy.TYPE_XSLT]: "Xslt",
  [Ci.nsIContentPolicy.TYPE_BEACON]: "Beacon",
  [Ci.nsIContentPolicy.TYPE_FETCH]: "Fetch",
  [Ci.nsIContentPolicy.TYPE_IMAGESET]: "Imageset",
  [Ci.nsIContentPolicy.TYPE_WEB_MANIFEST]: "WebManifest",
};

class Network extends Domain {
  constructor(session) {
    super(session);
    this.enabled = false;

    this._onRequest = this._onRequest.bind(this);
    this._onResponse = this._onResponse.bind(this);
  }

  destructor() {
    this.disable();

    super.destructor();
  }

  enable() {
    if (this.enabled) {
      return;
    }
    this.enabled = true;
    this.session.networkObserver.startTrackingBrowserNetwork(
      this.session.target.browser
    );
    this.session.networkObserver.on("request", this._onRequest);
    this.session.networkObserver.on("response", this._onResponse);
  }

  disable() {
    if (!this.enabled) {
      return;
    }
    this.session.networkObserver.stopTrackingBrowserNetwork(
      this.session.target.browser
    );
    this.session.networkObserver.off("request", this._onRequest);
    this.session.networkObserver.off("response", this._onResponse);
    this.enabled = false;
  }

  /**
   * Deletes browser cookies with matching name and url or domain/path pair.
   *
   * @param {Object} options
   * @param {string} name
   *     Name of the cookies to remove.
   * @param {string=} url
   *     If specified, deletes all the cookies with the given name
   *     where domain and path match provided URL.
   * @param {string=} domain
   *     If specified, deletes only cookies with the exact domain.
   * @param {string=} path
   *     If specified, deletes only cookies with the exact path.
   */
  async deleteCookies(options = {}) {
    const { domain, name, path = "/", url } = options;

    if (typeof name != "string") {
      throw new TypeError("name: string value expected");
    }

    if (!url && !domain) {
      throw new TypeError(
        "At least one of the url and domain needs to be specified"
      );
    }

    // Retrieve host. Check domain first because it has precedence.
    let hostname = domain || "";
    if (!hostname.length) {
      const cookieURL = new URL(url);
      if (!["http:", "https:"].includes(cookieURL.protocol)) {
        throw new TypeError("An http or https url must be specified");
      }
      hostname = cookieURL.hostname;
    }

    const cookiesFound = Services.cookies.getCookiesWithOriginAttributes(
      JSON.stringify({}),
      hostname
    );

    for (const cookie of cookiesFound) {
      if (cookie.name == name && cookie.path.startsWith(path)) {
        Services.cookies.remove(
          cookie.host,
          cookie.name,
          cookie.path,
          cookie.originAttributes
        );
      }
    }
  }

  /**
   * Activates emulation of network conditions.
   *
   * @param {Object} options
   * @param {boolean} offline
   *     True to emulate internet disconnection.
   */
  emulateNetworkConditions(options = {}) {
    const { offline } = options;

    if (typeof offline != "boolean") {
      throw new TypeError("offline: boolean value expected");
    }

    Services.io.offline = offline;
  }

  /**
   * Returns all browser cookies.
   *
   * Depending on the backend support, will return detailed cookie information in the cookies field.
   *
   * @param {Object} options
   *
   * @return {Array<Cookie>}
   *     Array of cookie objects.
   */
  async getAllCookies(options = {}) {
    const cookies = [];
    for (const cookie of Services.cookies.cookies) {
      cookies.push(_buildCookie(cookie));
    }

    return { cookies };
  }

  /**
   * Returns all browser cookies for the current URL.
   *
   * @param {Object} options
   * @param {Array<string>=} urls
   *     The list of URLs for which applicable cookies will be fetched.
   *     Defaults to the currently open URL.
   *
   * @return {Array<Cookie>}
   *     Array of cookie objects.
   */
  async getCookies(options = {}) {
    const { urls = this._getDefaultUrls() } = options;

    if (!Array.isArray(urls)) {
      throw new TypeError("urls: array expected");
    }

    for (const [index, url] of urls.entries()) {
      if (typeof url !== "string") {
        throw new TypeError(`urls: string value expected at index ${index}`);
      }
    }

    const cookies = [];
    for (let url of urls) {
      url = new URL(url);

      const secureProtocol = ["https:", "wss:"].includes(url.protocol);

      const cookiesFound = Services.cookies.getCookiesWithOriginAttributes(
        JSON.stringify({}),
        url.hostname
      );

      for (const cookie of cookiesFound) {
        // Ignore secure cookies for non-secure protocols
        if (cookie.isSecure && !secureProtocol) {
          continue;
        }

        // Ignore cookies which do not match the given path
        if (!url.pathname.startsWith(cookie.path)) {
          continue;
        }

        const builtCookie = _buildCookie(cookie);
        const duplicateCookie = cookies.some(value => {
          return (
            value.name === builtCookie.name &&
            value.path === builtCookie.path &&
            value.domain === builtCookie.domain
          );
        });

        if (duplicateCookie) {
          continue;
        }

        cookies.push(builtCookie);
      }
    }

    return { cookies };
  }

  /**
   * Sets a cookie with the given cookie data.
   *
   * Note that it may overwrite equivalent cookies if they exist.
   *
   * @param {Object} cookie
   * @param {string} name
   *     Cookie name.
   * @param {string} value
   *     Cookie value.
   * @param {string=} domain
   *     Cookie domain.
   * @param {number=} expires
   *     Cookie expiration date, session cookie if not set.
   * @param {boolean=} httpOnly
   *     True if cookie is http-only.
   * @param {string=} path
   *     Cookie path.
   * @param {string=} sameSite
   *     Cookie SameSite type.
   * @param {boolean=} secure
   *     True if cookie is secure.
   * @param {string=} url
   *     The request-URI to associate with the setting of the cookie.
   *     This value can affect the default domain and path values of the
   *     created cookie.
   *
   * @return {boolean}
   *     True if successfully set cookie.
   */
  setCookie(cookie) {
    if (typeof cookie.name != "string") {
      throw new TypeError("name: string value expected");
    }

    if (typeof cookie.value != "string") {
      throw new TypeError("value: string value expected");
    }

    if (
      typeof cookie.url == "undefined" &&
      typeof cookie.domain == "undefined"
    ) {
      throw new TypeError(
        "At least one of the url and domain needs to be specified"
      );
    }

    // Retrieve host. Check domain first because it has precedence.
    let hostname = cookie.domain || "";
    let cookieURL;
    let schemeType = Ci.nsICookie.SCHEME_UNSET;
    if (!hostname.length) {
      try {
        cookieURL = new URL(cookie.url);
      } catch (e) {
        return { success: false };
      }

      if (!["http:", "https:"].includes(cookieURL.protocol)) {
        throw new TypeError(`Invalid protocol ${cookieURL.protocol}`);
      }

      if (cookieURL.protocol == "https:") {
        cookie.secure = true;
        schemeType = Ci.nsICookie.SCHEME_HTTPS;
      } else {
        schemeType = Ci.nsICookie.SCHEME_HTTP;
      }

      hostname = cookieURL.hostname;
    }

    if (typeof cookie.path == "undefined") {
      cookie.path = "/";
    }

    let isSession = false;
    if (typeof cookie.expires == "undefined") {
      isSession = true;
      cookie.expires = MAX_COOKIE_EXPIRY;
    }

    const sameSiteMap = new Map([
      ["None", Ci.nsICookie.SAMESITE_NONE],
      ["Lax", Ci.nsICookie.SAMESITE_LAX],
      ["Strict", Ci.nsICookie.SAMESITE_STRICT],
    ]);

    let success = true;
    try {
      Services.cookies.add(
        hostname,
        cookie.path,
        cookie.name,
        cookie.value,
        cookie.secure,
        cookie.httpOnly || false,
        isSession,
        cookie.expires,
        {} /* originAttributes */,
        sameSiteMap.get(cookie.sameSite),
        schemeType
      );
    } catch (e) {
      success = false;
    }

    return { success };
  }

  /**
   * Sets given cookies.
   *
   * @param {Object} options
   * @param {Array.<Cookie>} cookies
   *     Cookies to be set.
   */
  setCookies(options = {}) {
    const { cookies } = options;

    if (!Array.isArray(cookies)) {
      throw new TypeError("Invalid parameters (cookies: array expected)");
    }

    cookies.forEach(cookie => {
      const { success } = this.setCookie(cookie);
      if (!success) {
        throw new Error("Invalid cookie fields");
      }
    });
  }

  /**
   * Toggles ignoring cache for each request. If true, cache will not be used.
   *
   * @param {Object} options
   * @param {boolean} options.cacheDisabled
   *     Cache disabled state.
   */
  async setCacheDisabled(options = {}) {
    const { cacheDisabled = false } = options;

    const { INHIBIT_CACHING, LOAD_BYPASS_CACHE, LOAD_NORMAL } = Ci.nsIRequest;

    let loadFlags = LOAD_NORMAL;
    if (cacheDisabled) {
      loadFlags = LOAD_BYPASS_CACHE | INHIBIT_CACHING;
    }

    await this.executeInChild("_updateLoadFlags", loadFlags);
  }

  /**
   * Allows overriding user agent with the given string.
   *
   * Redirected to Emulation.setUserAgentOverride.
   */
  setUserAgentOverride(options = {}) {
    const { id } = this.session;
    this.session.execute(id, "Emulation", "setUserAgentOverride", options);
  }

  _onRequest(eventName, httpChannel, data) {
    const wrappedChannel = ChannelWrapper.get(httpChannel);
    const urlFragment = httpChannel.URI.hasRef
      ? "#" + httpChannel.URI.ref
      : undefined;

    const request = {
      url: httpChannel.URI.specIgnoringRef,
      urlFragment,
      method: httpChannel.requestMethod,
      headers: headersAsObject(data.headers),
      postData: undefined,
      hasPostData: false,
      mixedContentType: undefined,
      initialPriority: undefined,
      referrerPolicy: undefined,
      isLinkPreload: false,
    };
    this.emit("Network.requestWillBeSent", {
      requestId: data.requestId,
      loaderId: data.loaderId,
      documentURL:
        wrappedChannel.documentURL || httpChannel.URI.specIgnoringRef,
      request,
      timestamp: Date.now() / 1000,
      wallTime: undefined,
      initiator: undefined,
      redirectResponse: undefined,
      type: LOAD_CAUSE_STRINGS[data.cause] || "unknown",
      frameId: data.frameId.toString(),
      hasUserGesture: undefined,
    });
  }

  _onResponse(eventName, httpChannel, data) {
    const wrappedChannel = ChannelWrapper.get(httpChannel);
    const headers = headersAsObject(data.headers);

    this.emit("Network.responseReceived", {
      requestId: data.requestId,
      loaderId: data.loaderId,
      timestamp: Date.now() / 1000,
      type: LOAD_CAUSE_STRINGS[data.cause] || "unknown",
      response: {
        url: httpChannel.URI.spec,
        status: data.status,
        statusText: data.statusText,
        headers,
        mimeType: wrappedChannel.contentType,
        requestHeaders: headersAsObject(data.requestHeaders),
        connectionReused: undefined,
        connectionId: undefined,
        remoteIPAddress: data.remoteIPAddress,
        remotePort: data.remotePort,
        fromDiskCache: data.fromCache,
        encodedDataLength: undefined,
        protocol: httpChannel.protocolVersion,
        securityDetails: data.securityDetails,
        // unknown, neutral, insecure, secure, info, insecure-broken
        securityState: "unknown",
      },
      frameId: data.frameId.toString(),
    });
  }

  /**
   * Creates an array of all Urls in the page context
   *
   * @param {Array<string>=} urls
   */
  _getDefaultUrls() {
    const urls = this.session.target.browsingContext
      .getAllBrowsingContextsInSubtree()
      .map(context => context.currentURI.spec);

    return urls;
  }
}

/**
 * Creates a CDP Network.Cookie from our internal cookie values
 *
 * @param {nsICookie} cookie
 *
 * @returns {Network.Cookie}
 *      A CDP Cookie
 */
function _buildCookie(cookie) {
  const data = {
    name: cookie.name,
    value: cookie.value,
    domain: cookie.host,
    path: cookie.path,
    expires: cookie.isSession ? -1 : cookie.expiry,
    // The size is the combined length of both the cookie name and value
    size: cookie.name.length + cookie.value.length,
    httpOnly: cookie.isHttpOnly,
    secure: cookie.isSecure,
    session: cookie.isSession,
  };

  if (cookie.sameSite) {
    const sameSiteMap = new Map([
      [Ci.nsICookie.SAMESITE_LAX, "Lax"],
      [Ci.nsICookie.SAMESITE_STRICT, "Strict"],
    ]);

    data.sameSite = sameSiteMap.get(cookie.sameSite);
  }

  return data;
}

/**
 * Given a array of possibly repeating header names, merge the values for
 * duplicate headers into a comma-separated list, or in some cases a
 * newline-separated list.
 *
 * e.g. { "Cache-Control": "no-cache,no-store" }
 *
 * Based on
 * https://hg.mozilla.org/mozilla-central/file/56c09d42f411246e407fe30418c27e67a6a44d29/netwerk/protocol/http/nsHttpHeaderArray.h
 *
 * @param {Array} headers
 *    Array of {name, value}
 * @returns {Object}
 *    Object where each key is a header name.
 */
function headersAsObject(headers) {
  const rv = {};
  headers.forEach(({ name, value }) => {
    name = name.toLowerCase();
    if (rv[name]) {
      const separator = [
        "set-cookie",
        "www-authenticate",
        "proxy-authenticate",
      ].includes(name)
        ? "\n"
        : ",";
      rv[name] += `${separator}${value}`;
    } else {
      rv[name] = value;
    }
  });
  return rv;
}
