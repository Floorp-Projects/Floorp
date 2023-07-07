/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  Marionette: "chrome://remote/content/components/Marionette.sys.mjs",
});

class BrowserModule extends Module {
  constructor(messageHandler) {
    super(messageHandler);
  }

  destroy() {}

  /**
   * Commands
   */

  /**
   * Terminate all WebDriver sessions and clean up automation state in the remote browser instance.
   *
   * Session clean up and actual broser closure will happen later in WebDriverBiDiConnection class.
   */
  async close() {
    // TODO Bug 1838269. Enable browser.close command for the case of classic + bidi session, when
    // session ending for this type of session is supported.
    if (lazy.Marionette.running) {
      throw new lazy.error.UnsupportedOperationError(
        "Closing browser with the session which was started with Webdriver classic is not supported," +
          "you can use Webdriver classic session delete command which will also close the browser."
      );
    }
  }
}

// To export the class as lower-case
export const browser = BrowserModule;
