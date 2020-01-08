/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Emulation"];

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

const { Domain } = ChromeUtils.import(
  "chrome://remote/content/domains/Domain.jsm"
);

class Emulation extends Domain {
  destructor() {
    this.setUserAgentOverride({ userAgent: "" });

    super.destructor();
  }

  /**
   * Allows overriding user agent with the given string.
   *
   * @param {Object} options
   * @param {string} options.userAgent
   *     User agent to use.
   * @param {string=} options.acceptLanguage [not yet supported]
   *     Browser langugage to emulate.
   * @param {number=} options.platform [not yet supported]
   *     The platform navigator.platform should return.
   */
  async setUserAgentOverride(options = {}) {
    const { userAgent } = options;

    if (typeof userAgent != "string") {
      throw new TypeError(
        "Invalid parameters (userAgent: string value expected)"
      );
    }

    if (userAgent.length == 0) {
      await this.executeInChild("_setCustomUserAgent", null);
    } else if (this._isValidHTTPRequestHeaderValue(userAgent)) {
      await this.executeInChild("_setCustomUserAgent", userAgent);
    } else {
      throw new TypeError("Invalid characters found in userAgent");
    }
  }

  _isValidHTTPRequestHeaderValue(value) {
    try {
      const channel = NetUtil.newChannel({
        uri: "http://localhost",
        loadUsingSystemPrincipal: true,
      });
      channel.QueryInterface(Ci.nsIHttpChannel);
      channel.setRequestHeader("X-check", value, false);
      return true;
    } catch (e) {
      return false;
    }
  }
}
