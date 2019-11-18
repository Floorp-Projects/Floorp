/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Emulation"];

const { Domain } = ChromeUtils.import(
  "chrome://remote/content/domains/Domain.jsm"
);
const { UnsupportedError } = ChromeUtils.import(
  "chrome://remote/content/Error.jsm"
);
const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const PREF_USER_AGENT_OVERRIDE = "general.useragent.override";

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
  setUserAgentOverride(options = {}) {
    const { userAgent } = options;

    if (options.acceptLanguage) {
      throw new UnsupportedError("'acceptLanguage' not supported");
    }
    if (options.platform) {
      throw new UnsupportedError("'platform' not supported");
    }

    // TODO: Set the user agent for the current session only (Bug 1596136)
    if (userAgent.length == 0) {
      Services.prefs.clearUserPref(PREF_USER_AGENT_OVERRIDE);
    } else if (this._isValidHTTPRequestHeaderValue(userAgent)) {
      Services.prefs.setStringPref(PREF_USER_AGENT_OVERRIDE, userAgent);
    } else {
      throw new Error("Invalid characters found in userAgent");
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
