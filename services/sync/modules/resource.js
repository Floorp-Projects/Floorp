/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["Resource"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { Observers } = ChromeUtils.import(
  "resource://services-common/observers.js"
);
const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);
const { Utils } = ChromeUtils.import("resource://services-sync/util.js");
const { setTimeout, clearTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);
/* global AbortController */

/*
 * Resource represents a remote network resource, identified by a URI.
 * Create an instance like so:
 *
 *   let resource = new Resource("http://foobar.com/path/to/resource");
 *
 * The 'resource' object has the following methods to issue HTTP requests
 * of the corresponding HTTP methods:
 *
 *   get(callback)
 *   put(data, callback)
 *   post(data, callback)
 *   delete(callback)
 */
function Resource(uri) {
  this._log = Log.repository.getLogger(this._logName);
  this._log.manageLevelFromPref("services.sync.log.logger.network.resources");
  this.uri = uri;
  this._headers = {};
}
// (static) Caches the latest server timestamp (X-Weave-Timestamp header).
Resource.serverTime = null;

XPCOMUtils.defineLazyPreferenceGetter(
  Resource,
  "SEND_VERSION_INFO",
  "services.sync.sendVersionInfo",
  true
);
Resource.prototype = {
  _logName: "Sync.Resource",

  /**
   * Callback to be invoked at request time to add authentication details.
   * If the callback returns a promise, it will be awaited upon.
   *
   * By default, a global authenticator is provided. If this is set, it will
   * be used instead of the global one.
   */
  authenticator: null,

  // Wait 5 minutes before killing a request.
  ABORT_TIMEOUT: 300000,

  // Headers to be included when making a request for the resource.
  // Note: Header names should be all lower case, there's no explicit
  // check for duplicates due to case!
  get headers() {
    return this._headers;
  },
  set headers(_) {
    throw new Error("headers can't be mutated directly. Please use setHeader.");
  },
  setHeader(header, value) {
    this._headers[header.toLowerCase()] = value;
  },

  // URI representing this resource.
  get uri() {
    return this._uri;
  },
  set uri(value) {
    if (typeof value == "string") {
      this._uri = CommonUtils.makeURI(value);
    } else {
      this._uri = value;
    }
  },

  // Get the string representation of the URI.
  get spec() {
    if (this._uri) {
      return this._uri.spec;
    }
    return null;
  },

  /**
   * @param {string} method HTTP method
   * @returns {Headers}
   */
  async _buildHeaders(method) {
    const headers = new Headers(this._headers);

    if (Resource.SEND_VERSION_INFO) {
      headers.append("user-agent", Utils.userAgent);
    }

    if (this.authenticator) {
      const result = await this.authenticator(this, method);
      if (result && result.headers) {
        for (const [k, v] of Object.entries(result.headers)) {
          headers.append(k.toLowerCase(), v);
        }
      }
    } else {
      this._log.debug("No authenticator found.");
    }

    // PUT and POST are treated differently because they have payload data.
    if (("PUT" == method || "POST" == method) && !headers.has("content-type")) {
      headers.append("content-type", "text/plain");
    }

    if (this._log.level <= Log.Level.Trace) {
      for (const [k, v] of headers) {
        if (k == "authorization" || k == "x-client-state") {
          this._log.trace(`HTTP Header ${k}: ***** (suppressed)`);
        } else {
          this._log.trace(`HTTP Header ${k}: ${v}`);
        }
      }
    }

    if (!headers.has("accept")) {
      headers.append("accept", "application/json;q=0.9,*/*;q=0.2");
    }

    return headers;
  },

  /**
   * @param {string} method HTTP method
   * @param {string} data HTTP body
   * @param {object} signal AbortSignal instance
   * @returns {Request}
   */
  async _createRequest(method, data, signal) {
    const headers = await this._buildHeaders(method);
    const init = {
      cache: "no-store", // No cache.
      headers,
      method,
      signal,
      mozErrors: true, // Return nsresult error codes instead of a generic
      // NetworkError when fetch rejects.
    };

    if (data) {
      if (!(typeof data == "string" || data instanceof String)) {
        data = JSON.stringify(data);
      }
      this._log.debug(`${method} Length: ${data.length}`);
      this._log.trace(`${method} Body: ${data}`);
      init.body = data;
    }
    return new Request(this.uri.spec, init);
  },

  /**
   * @param {string} method HTTP method
   * @param {string} [data] HTTP body
   * @returns {Response}
   */
  async _doRequest(method, data = null) {
    const controller = new AbortController();
    const request = await this._createRequest(method, data, controller.signal);
    const responsePromise = fetch(request); // Rejects on network failure.
    let didTimeout = false;
    const timeoutId = setTimeout(() => {
      didTimeout = true;
      this._log.error(
        `Request timed out after ${this.ABORT_TIMEOUT}ms. Aborting.`
      );
      controller.abort();
    }, this.ABORT_TIMEOUT);
    let response;
    try {
      response = await responsePromise;
    } catch (e) {
      this._log.warn(`${method} request to ${this.uri.spec} failed`, e);
      if (!didTimeout) {
        throw e;
      }
      throw Components.Exception(
        "Request aborted (timeout)",
        Cr.NS_ERROR_NET_TIMEOUT
      );
    } finally {
      clearTimeout(timeoutId);
    }
    return this._processResponse(response, method);
  },

  async _processResponse(response, method) {
    const data = await response.text();
    this._logResponse(response, method, data);
    this._processResponseHeaders(response);

    const ret = {
      data,
      url: response.url,
      status: response.status,
      success: response.ok,
      headers: {},
    };
    for (const [k, v] of response.headers) {
      ret.headers[k] = v;
    }

    // Make a lazy getter to convert the json response into an object.
    // Note that this can cause a parse error to be thrown far away from the
    // actual fetch, so be warned!
    XPCOMUtils.defineLazyGetter(ret, "obj", () => {
      try {
        return JSON.parse(ret.data);
      } catch (ex) {
        this._log.warn("Got exception parsing response body", ex);
        // Stringify to avoid possibly printing non-printable characters.
        this._log.debug(
          "Parse fail: Response body starts",
          (ret.data + "").slice(0, 100)
        );
        throw ex;
      }
    });

    return ret;
  },

  _logResponse(response, method, data) {
    const { status, ok: success, url } = response;

    // Log the status of the request.
    this._log.debug(
      `${method} ${success ? "success" : "fail"} ${status} ${url}`
    );

    // Additionally give the full response body when Trace logging.
    if (this._log.level <= Log.Level.Trace) {
      this._log.trace(`${method} body`, data);
    }

    if (!success) {
      this._log.warn(
        `${method} request to ${url} failed with status ${status}`
      );
    }
  },

  _processResponseHeaders({ headers, ok: success }) {
    if (headers.has("x-weave-timestamp")) {
      Resource.serverTime = parseFloat(headers.get("x-weave-timestamp"));
    }
    // This is a server-side safety valve to allow slowing down
    // clients without hurting performance.
    if (headers.has("x-weave-backoff")) {
      let backoff = headers.get("x-weave-backoff");
      this._log.debug(`Got X-Weave-Backoff: ${backoff}`);
      Observers.notify("weave:service:backoff:interval", parseInt(backoff, 10));
    }

    if (success && headers.has("x-weave-quota-remaining")) {
      Observers.notify(
        "weave:service:quota:remaining",
        parseInt(headers.get("x-weave-quota-remaining"), 10)
      );
    }
  },

  get() {
    return this._doRequest("GET");
  },

  put(data) {
    return this._doRequest("PUT", data);
  },

  post(data) {
    return this._doRequest("POST", data);
  },

  delete() {
    return this._doRequest("DELETE");
  },
};
