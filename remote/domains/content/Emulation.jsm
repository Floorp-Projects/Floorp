/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Emulation"];

const { ContentProcessDomain } = ChromeUtils.import(
  "chrome://remote/content/domains/ContentProcessDomain.jsm"
);

class Emulation extends ContentProcessDomain {
  // commands

  setDeviceMetricsOverride() {}

  setTouchEmulationEnabled() {}

  /**
   * Internal methods: the following methods are not part of CDP;
   * note the _ prefix.
   */

  /**
   * Waits until the viewport has reached the new dimensions.
   */
  async _awaitViewportDimensions({ width, height }) {
    const win = this.content;

    if (win.innerWidth === width && win.innerHeight === height) {
      return;
    }

    await new Promise(resolve => {
      win.addEventListener("resize", function resized() {
        if (win.innerWidth === width && win.innerHeight === height) {
          win.removeEventListener("resize", resized);
          resolve();
        }
      });
    });
  }

  _setCustomUserAgent(userAgent) {
    this.docShell.customUserAgent = userAgent;
  }

  _setDPPXOverride(dppx) {
    this.docShell.contentViewer.overrideDPPX = dppx;
  }
}
