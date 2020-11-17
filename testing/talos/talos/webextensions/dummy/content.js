/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-env webextensions */

browser.runtime.sendMessage({
  msg: "Hello from content script",
  url: location.href,
});

browser.runtime.onMessage.addListener(msg => {
  return Promise.resolve({ code: "10-4", msg });
});
