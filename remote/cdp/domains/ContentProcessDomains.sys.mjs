/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const ContentProcessDomains = {};

// eslint-disable-next-line mozilla/lazy-getter-object-name
ChromeUtils.defineESModuleGetters(ContentProcessDomains, {
  DOM: "chrome://remote/content/cdp/domains/content/DOM.sys.mjs",
  Emulation: "chrome://remote/content/cdp/domains/content/Emulation.sys.mjs",
  Input: "chrome://remote/content/cdp/domains/content/Input.sys.mjs",
  Log: "chrome://remote/content/cdp/domains/content/Log.sys.mjs",
  Network: "chrome://remote/content/cdp/domains/content/Network.sys.mjs",
  Page: "chrome://remote/content/cdp/domains/content/Page.sys.mjs",
  Performance:
    "chrome://remote/content/cdp/domains/content/Performance.sys.mjs",
  Runtime: "chrome://remote/content/cdp/domains/content/Runtime.sys.mjs",
  Security: "chrome://remote/content/cdp/domains/content/Security.sys.mjs",
});
