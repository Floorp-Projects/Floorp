/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module consolidates various code and data update requests, so flags
 * can be set, Telemetry collected, etc. in a central place.
 */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyGlobalGetters(this, ["XMLHttpRequest"]);

var EXPORTED_SYMBOLS = ["ServiceRequest"];

const logger = Log.repository.getLogger("ServiceRequest");
logger.level = Log.Level.Debug;
logger.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));

/**
 * ServiceRequest is intended to be a drop-in replacement for current users
 * of XMLHttpRequest.
 *
 * @param {Object} options - Options for underlying XHR, e.g. { mozAnon: bool }
 */
class ServiceRequest extends XMLHttpRequest {
  constructor(options) {
    super(options);
  }
  /**
   * Opens an XMLHttpRequest, and sets the NSS "beConservative" flag.
   * Requests are always async.
   *
   * @param {String} method - HTTP method to use, e.g. "GET".
   * @param {String} url - URL to open.
   * @param {Object} options - Additional options { bypassProxy: bool }.
   */
  open(method, url, options) {
    super.open(method, url, true);

    if (super.channel instanceof Ci.nsIHttpChannelInternal) {
      let internal = super.channel.QueryInterface(Ci.nsIHttpChannelInternal);
      // Disable cutting edge features, like TLS 1.3, where middleboxes might brick us
      internal.beConservative = true;
      // Disable use of proxy for this request if necessary.
      if (options?.bypassProxy && this.bypassProxyEnabled) {
        internal.bypassProxy = true;
      }
    }
  }

  get bypassProxy() {
    let { channel } = this;
    return channel.QueryInterface(Ci.nsIHttpChannelInternal).bypassProxy;
  }

  get isProxied() {
    let { channel } = this;
    return !!(channel instanceof Ci.nsIProxiedChannel && channel.proxyInfo);
  }

  get bypassProxyEnabled() {
    return Services.prefs.getBoolPref("network.proxy.allow_bypass", true);
  }
}
