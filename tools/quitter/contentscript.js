/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-env webextensions */

const Quitter = {
  async quit() {
    // This can be called before the background page has loaded,
    // so we need to wait for it.
    browser.runtime.sendMessage("quit").catch(() => {
      setTimeout(Quitter.quit, 100);
    });
  },
};

window.wrappedJSObject.Quitter = cloneInto(Quitter, window, {
  cloneFunctions: true,
});
