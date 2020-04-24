/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Network"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

const { Domain } = ChromeUtils.import(
  "chrome://remote/content/domains/Domain.jsm"
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
  [Ci.nsIContentPolicy.TYPE_REFRESH]: "Refresh",
  [Ci.nsIContentPolicy.TYPE_XBL]: "Xbl",
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
  }

  disable() {
    if (!this.enabled) {
      return;
    }
    this.session.networkObserver.stopTrackingBrowserNetwork(
      this.session.target.browser
    );
    this.session.networkObserver.off("request", this._onRequest);
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
    if (hostname.length == 0) {
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
    // Bug 1605354 - Add support for options.urls
    const urls = [this.session.target.url];

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

        cookies.push(data);
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
    if (hostname.length == 0) {
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
        sameSiteMap.get(cookie.sameSite)
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
    const topFrame = getLoadContext(httpChannel).topFrameElement;
    const request = {
      url: httpChannel.URI.spec,
      urlFragment: undefined,
      method: httpChannel.requestMethod,
      headers: [],
      postData: undefined,
      hasPostData: false,
      mixedContentType: undefined,
      initialPriority: undefined,
      referrerPolicy: undefined,
      isLinkPreload: false,
    };
    let loaderId = undefined;
    let causeType = Ci.nsIContentPolicy.TYPE_OTHER;
    let causeUri = topFrame.currentURI.spec;
    if (httpChannel.loadInfo) {
      causeType = httpChannel.loadInfo.externalContentPolicyType;
      const { loadingPrincipal } = httpChannel.loadInfo;
      if (loadingPrincipal && loadingPrincipal.URI) {
        causeUri = loadingPrincipal.URI.spec;
      }
      if (causeType == Ci.nsIContentPolicy.TYPE_DOCUMENT) {
        // Puppeteer expect this specialy of CDP where loaderId = requestId
        // for the toplevel document request
        loaderId = String(httpChannel.channelId);
      }
    }
    this.emit("Network.requestWillBeSent", {
      requestId: String(httpChannel.channelId),
      loaderId,
      documentURL: causeUri,
      request,
      timestamp: undefined,
      wallTime: undefined,
      initiator: undefined,
      redirectResponse: undefined,
      type: LOAD_CAUSE_STRINGS[causeType] || "unknown",
      frameId: topFrame.browsingContext.id.toString(),
      hasUserGesture: undefined,
    });
  }
}

function getLoadContext(httpChannel) {
  let loadContext = null;
  try {
    if (httpChannel.notificationCallbacks) {
      loadContext = httpChannel.notificationCallbacks.getInterface(
        Ci.nsILoadContext
      );
    }
  } catch (e) {}
  try {
    if (!loadContext && httpChannel.loadGroup) {
      loadContext = httpChannel.loadGroup.notificationCallbacks.getInterface(
        Ci.nsILoadContext
      );
    }
  } catch (e) {}
  return loadContext;
}
