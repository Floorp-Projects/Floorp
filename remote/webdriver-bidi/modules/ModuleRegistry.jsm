/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const modules = {
  root: {},
  "windowglobal-in-root": {},
  windowglobal: {},
};

// Modules are not exported here and the lazy getters are only here to avoid
// errors in browser_all_files_referenced.js
XPCOMUtils.defineLazyModuleGetters(modules.root, {
  session: "chrome://remote/content/webdriver-bidi/modules/root/session.jsm",
});

// Upcoming modules in windowglobal-in-root or windowglobal will be set via:
//
// XPCOMUtils.defineLazyModuleGetters(modules["windowglobal-in-root"], {
//   log:
//     "chrome://remote/content/webdriver-bidi/modules/windowglobal-in-root/log.jsm",
// });

// XPCOMUtils.defineLazyModuleGetters(modules.windowglobal, {
//   log: "chrome://remote/content/webdriver-bidi/modules/windowglobal/log.jsm",
// });
