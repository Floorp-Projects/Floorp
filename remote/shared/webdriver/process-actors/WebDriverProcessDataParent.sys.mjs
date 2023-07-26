/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

/**
 * Register the WebDriverProcessData actor that holds session data.
 */
export function registerProcessDataActor() {
  try {
    ChromeUtils.registerProcessActor("WebDriverProcessData", {
      kind: "JSProcessActor",
      child: {
        esModuleURI:
          "chrome://remote/content/shared/webdriver/process-actors/WebDriverProcessDataChild.sys.mjs",
      },
      includeParent: true,
    });
  } catch (e) {
    if (e.name === "NotSupportedError") {
      lazy.logger.warn(`WebDriverProcessData actor is already registered!`);
    } else {
      throw e;
    }
  }
}

export function unregisterProcessDataActor() {
  ChromeUtils.unregisterProcessActor("WebDriverProcessData");
}
