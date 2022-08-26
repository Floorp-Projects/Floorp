/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Emulation"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { Domain } = ChromeUtils.import(
  "chrome://remote/content/cdp/domains/Domain.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  NetUtil: "resource://gre/modules/NetUtil.jsm",
});

const MAX_WINDOW_SIZE = 10000000;

class Emulation extends Domain {
  destructor() {
    this.setUserAgentOverride({ userAgent: "", platform: "" });

    super.destructor();
  }

  /**
   * Overrides the values of device screen dimensions.
   *
   * Values as modified are:
   *   - window.screen.width
   *   - window.screen.height
   *   - window.innerWidth
   *   - window.innerHeight
   *   - "device-width"/"device-height"-related CSS media query results
   *
   * @param {Object} options
   * @param {number} options.width
   *     Overriding width value in pixels. 0 disables the override.
   * @param {number} options.height
   *     Overriding height value in pixels. 0 disables the override.
   * @param {number} options.deviceScaleFactor
   *     Overriding device scale factor value. 0 disables the override.
   * @param {number} options.mobile [not supported]
   *     Whether to emulate a mobile device. This includes viewport meta tag,
   *     overlay scrollbars, text autosizing and more.
   * @param {number} options.screenOrientation
   *     Screen orientation override [not supported]
   */
  async setDeviceMetricsOverride(options = {}) {
    const { width, height, deviceScaleFactor } = options;

    if (
      width < 0 ||
      width > MAX_WINDOW_SIZE ||
      height < 0 ||
      height > MAX_WINDOW_SIZE
    ) {
      throw new TypeError(
        `Width and height values must be positive, not greater than ${MAX_WINDOW_SIZE}`
      );
    }

    if (typeof deviceScaleFactor != "number") {
      throw new TypeError("deviceScaleFactor: number expected");
    }

    if (deviceScaleFactor < 0) {
      throw new TypeError("deviceScaleFactor: must be positive");
    }

    const { tab } = this.session.target;
    const { linkedBrowser: browser } = tab;

    const { browsingContext } = this.session.target;
    browsingContext.overrideDPPX = deviceScaleFactor;

    // With a value of 0 the current size is used
    const { layoutViewport } = await this.session.execute(
      this.session.id,
      "Page",
      "getLayoutMetrics"
    );

    const targetWidth = width > 0 ? width : layoutViewport.clientWidth;
    const targetHeight = height > 0 ? height : layoutViewport.clientHeight;

    browser.style.setProperty("min-width", targetWidth + "px");
    browser.style.setProperty("max-width", targetWidth + "px");
    browser.style.setProperty("min-height", targetHeight + "px");
    browser.style.setProperty("max-height", targetHeight + "px");

    // Wait until the viewport has been resized
    await this.executeInChild("_awaitViewportDimensions", {
      width: targetWidth,
      height: targetHeight,
    });
  }

  /**
   * Enables touch on platforms which do not support them.
   *
   * @param {Object} options
   * @param {boolean} options.enabled
   *     Whether the touch event emulation should be enabled.
   * @param {number=} options.maxTouchPoints [not yet supported]
   *     Maximum touch points supported. Defaults to one.
   */
  async setTouchEmulationEnabled(options = {}) {
    const { enabled } = options;

    if (typeof enabled != "boolean") {
      throw new TypeError(
        "Invalid parameters (enabled: boolean value expected)"
      );
    }

    const { browsingContext } = this.session.target;
    if (enabled) {
      browsingContext.touchEventsOverride = "enabled";
    } else {
      browsingContext.touchEventsOverride = "none";
    }
  }

  /**
   * Allows overriding user agent with the given string.
   *
   * @param {Object} options
   * @param {string} options.userAgent
   *     User agent to use.
   * @param {string=} options.acceptLanguage [not yet supported]
   *     Browser langugage to emulate.
   * @param {string=} options.platform
   *     The platform navigator.platform should return.
   */
  async setUserAgentOverride(options = {}) {
    const { userAgent, platform } = options;

    if (typeof userAgent != "string") {
      throw new TypeError(
        "Invalid parameters (userAgent: string value expected)"
      );
    }

    if (!["undefined", "string"].includes(typeof platform)) {
      throw new TypeError("platform: string value expected");
    }

    const { browsingContext } = this.session.target;

    if (!userAgent.length) {
      browsingContext.customUserAgent = null;
    } else if (this._isValidHTTPRequestHeaderValue(userAgent)) {
      browsingContext.customUserAgent = userAgent;
    } else {
      throw new TypeError("Invalid characters found in userAgent");
    }

    if (platform?.length > 0) {
      browsingContext.customPlatform = platform;
    } else {
      browsingContext.customPlatform = null;
    }
  }

  _isValidHTTPRequestHeaderValue(value) {
    try {
      const channel = lazy.NetUtil.newChannel({
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
