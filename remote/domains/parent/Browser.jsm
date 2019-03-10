/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Browser"];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {Domain} = ChromeUtils.import("chrome://remote/content/domains/Domain.jsm");

class Browser extends Domain {
  getVersion() {
    return {
      protocolVersion: "1",
      product: "Firefox",
      revision: "1",
      userAgent: "Firefox",
      jsVersion: "1.8.5",
    };
  }

  close() {
    Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);
  }
}
