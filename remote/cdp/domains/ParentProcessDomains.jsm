/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ParentProcessDomains"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const ParentProcessDomains = {};

XPCOMUtils.defineLazyModuleGetters(ParentProcessDomains, {
  Browser: "chrome://remote/content/cdp/domains/parent/Browser.jsm",
  Emulation: "chrome://remote/content/cdp/domains/parent/Emulation.jsm",
  Fetch: "chrome://remote/content/cdp/domains/parent/Fetch.jsm",
  Input: "chrome://remote/content/cdp/domains/parent/Input.jsm",
  IO: "chrome://remote/content/cdp/domains/parent/IO.jsm",
  Network: "chrome://remote/content/cdp/domains/parent/Network.jsm",
  Page: "chrome://remote/content/cdp/domains/parent/Page.jsm",
  Security: "chrome://remote/content/cdp/domains/parent/Security.jsm",
  Target: "chrome://remote/content/cdp/domains/parent/Target.jsm",
});
