/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ContentProcessDomains"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const ContentProcessDomains = {};

XPCOMUtils.defineLazyModuleGetters(ContentProcessDomains, {
  DOM: "chrome://remote/content/domains/content/DOM.jsm",
  Emulation: "chrome://remote/content/domains/content/Emulation.jsm",
  Input: "chrome://remote/content/domains/content/Input.jsm",
  Log: "chrome://remote/content/domains/content/Log.jsm",
  Network: "chrome://remote/content/domains/content/Network.jsm",
  Page: "chrome://remote/content/domains/content/Page.jsm",
  Performance: "chrome://remote/content/domains/content/Performance.jsm",
  Runtime: "chrome://remote/content/domains/content/Runtime.jsm",
  Security: "chrome://remote/content/domains/content/Security.jsm",
});
