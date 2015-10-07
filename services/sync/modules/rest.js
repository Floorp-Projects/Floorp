/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/constants.js");

this.EXPORTED_SYMBOLS = ["SyncStorageRequest"];

const STORAGE_REQUEST_TIMEOUT = 5 * 60; // 5 minutes

/**
 * RESTRequest variant for use against a Sync storage server.
 */
this.SyncStorageRequest = function SyncStorageRequest(uri) {
  RESTRequest.call(this, uri);

  this.authenticator = null;
}
SyncStorageRequest.prototype = {

  __proto__: RESTRequest.prototype,

  _logName: "Sync.StorageRequest",

  /**
   * The string to use as the base User-Agent in Sync requests.
   * These strings will look something like
   * 
   *   Firefox/4.0 FxSync/1.8.0.20100101.mobile
   * 
   * or
   * 
   *   Firefox Aurora/5.0a1 FxSync/1.9.0.20110409.desktop
   */
  userAgent:
    Services.appinfo.name + "/" + Services.appinfo.version +  // Product.
    " FxSync/" + WEAVE_VERSION + "." +                        // Sync.
    Services.appinfo.appBuildID + ".",                        // Build.

  /**
   * Wait 5 minutes before killing a request.
   */
  timeout: STORAGE_REQUEST_TIMEOUT,

  dispatch: function dispatch(method, data, onComplete, onProgress) {
    // Compose a UA string fragment from the various available identifiers.
    if (Svc.Prefs.get("sendVersionInfo", true)) {
      let ua = this.userAgent + Svc.Prefs.get("client.type", "desktop");
      this.setHeader("user-agent", ua);
    }

    if (this.authenticator) {
      this.authenticator(this);
    } else {
      this._log.debug("No authenticator found.");
    }

    return RESTRequest.prototype.dispatch.apply(this, arguments);
  },

  onStartRequest: function onStartRequest(channel) {
    RESTRequest.prototype.onStartRequest.call(this, channel);
    if (this.status == this.ABORTED) {
      return;
    }

    let headers = this.response.headers;
    // Save the latest server timestamp when possible.
    if (headers["x-weave-timestamp"]) {
      SyncStorageRequest.serverTime = parseFloat(headers["x-weave-timestamp"]);
    }

    // This is a server-side safety valve to allow slowing down
    // clients without hurting performance.
    if (headers["x-weave-backoff"]) {
      Svc.Obs.notify("weave:service:backoff:interval",
                     parseInt(headers["x-weave-backoff"], 10));
    }

    if (this.response.success && headers["x-weave-quota-remaining"]) {
      Svc.Obs.notify("weave:service:quota:remaining",
                     parseInt(headers["x-weave-quota-remaining"], 10));
    }
  },

  onStopRequest: function onStopRequest(channel, context, statusCode) {
    if (this.status != this.ABORTED) {
      let resp = this.response;
      let contentLength = resp.headers ? resp.headers["content-length"] : "";

      if (resp.success && contentLength &&
          contentLength != resp.body.length) {
        this._log.warn("The response body's length of: " + resp.body.length +
                       " doesn't match the header's content-length of: " +
                       contentLength + ".");
      }
    }

    RESTRequest.prototype.onStopRequest.apply(this, arguments);
  }
};
