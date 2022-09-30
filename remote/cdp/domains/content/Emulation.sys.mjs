/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContentProcessDomain } from "chrome://remote/content/cdp/domains/ContentProcessDomain.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AnimationFramePromise: "chrome://remote/content/shared/Sync.sys.mjs",
});

export class Emulation extends ContentProcessDomain {
  // commands

  /**
   * Internal methods: the following methods are not part of CDP;
   * note the _ prefix.
   */

  /**
   * Waits until the viewport has reached the new dimensions.
   */
  async _awaitViewportDimensions({ width, height }) {
    const win = this.content;
    let resized;

    // Updates for background tabs are throttled, and we also we have to make
    // sure that the new browser dimensions have been received by the content
    // process. As such wait for the next animation frame.
    await lazy.AnimationFramePromise(win);

    const checkBrowserSize = () => {
      if (win.innerWidth === width && win.innerHeight === height) {
        resized();
      }
    };

    return new Promise(resolve => {
      resized = resolve;

      win.addEventListener("resize", checkBrowserSize);

      // Trigger a layout flush in case none happened yet.
      checkBrowserSize();
    }).finally(() => {
      win.removeEventListener("resize", checkBrowserSize);
    });
  }
}
