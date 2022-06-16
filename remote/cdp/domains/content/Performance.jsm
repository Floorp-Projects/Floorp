/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Performance"];

const { ContentProcessDomain } = ChromeUtils.import(
  "chrome://remote/content/cdp/domains/ContentProcessDomain.jsm"
);

class Performance extends ContentProcessDomain {
  constructor(session) {
    super(session);
    this.enabled = false;
  }

  destructor() {
    this.disable();

    super.destructor();
  }

  // commands

  async enable() {
    if (!this.enabled) {
      this.enabled = true;
    }
  }

  disable() {
    if (this.enabled) {
      this.enabled = false;
    }
  }
}
