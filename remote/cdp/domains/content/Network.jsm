/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Network"];

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ContentProcessDomain:
    "chrome://remote/content/cdp/domains/ContentProcessDomain.jsm",
});

class Network extends lazy.ContentProcessDomain {
  // commands

  /**
   * Internal methods: the following methods are not part of CDP;
   * note the _ prefix.
   */

  _updateLoadFlags(flags) {
    this.docShell.defaultLoadFlags = flags;
  }
}
