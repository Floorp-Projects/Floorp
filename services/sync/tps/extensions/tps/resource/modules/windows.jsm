/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* This is a JavaScript module (JSM) to be imported via
   ChromeUtils.import() and acts as a singleton.
   Only the following listed symbols will exposed on import, and only when
   and where imported. */

const EXPORTED_SYMBOLS = ["BrowserWindows"];

var BrowserWindows = {
  /**
   * Add
   *
   * Opens a new window. Throws on error.
   *
   * @param aPrivate The private option.
   * @return nothing
   */
  Add(aPrivate, fn) {
    return new Promise(resolve => {
      let mainWindow = Services.wm.getMostRecentWindow("navigator:browser");
      let win = mainWindow.OpenBrowserWindow({ private: aPrivate });
      win.addEventListener(
        "load",
        function() {
          resolve(win);
        },
        { once: true }
      );
    });
  },
};
