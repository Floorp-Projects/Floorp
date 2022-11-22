/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let AboutWindowsMessages = null;

function onLoad() {}

try {
  AboutWindowsMessages = Cc["@mozilla.org/about-windowsmessages;1"].getService(
    Ci.nsIAboutWindowsMessages
  );
  document.addEventListener("DOMContentLoaded", onLoad, { once: true });
} catch (ex) {
  // Do nothing if we fail to create a singleton instance,
  // showing the default no-module message.
  Cu.reportError(ex);
}
