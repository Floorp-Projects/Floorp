/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Firefox Sync.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *   Richard Newman <rnewman@mozilla.com>
 *   Dan Mills <thunder@mozilla.com>
 *   Anant Narayanan <anant@kix.in>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/constants.js");

const EXPORTED_SYMBOLS = ["SyncStorageRequest"];

const STORAGE_REQUEST_TIMEOUT = 5 * 60; // 5 minutes

/**
 * RESTRequest variant for use against a Sync storage server.
 */
function SyncStorageRequest(uri) {
  RESTRequest.call(this, uri);
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

    let authenticator = Identity.getRESTRequestAuthenticator();
    if (authenticator) {
      authenticator(this);
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
  }
};
