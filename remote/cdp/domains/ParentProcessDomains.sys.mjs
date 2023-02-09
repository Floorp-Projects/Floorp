/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const ParentProcessDomains = {};

// eslint-disable-next-line mozilla/lazy-getter-object-name
ChromeUtils.defineESModuleGetters(ParentProcessDomains, {
  Browser: "chrome://remote/content/cdp/domains/parent/Browser.sys.mjs",
  Emulation: "chrome://remote/content/cdp/domains/parent/Emulation.sys.mjs",
  Fetch: "chrome://remote/content/cdp/domains/parent/Fetch.sys.mjs",
  Input: "chrome://remote/content/cdp/domains/parent/Input.sys.mjs",
  IO: "chrome://remote/content/cdp/domains/parent/IO.sys.mjs",
  Network: "chrome://remote/content/cdp/domains/parent/Network.sys.mjs",
  Page: "chrome://remote/content/cdp/domains/parent/Page.sys.mjs",
  Security: "chrome://remote/content/cdp/domains/parent/Security.sys.mjs",
  SystemInfo: "chrome://remote/content/cdp/domains/parent/SystemInfo.sys.mjs",
  Target: "chrome://remote/content/cdp/domains/parent/Target.sys.mjs",
});
