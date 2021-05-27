/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let AboutThirdParty = null;

function onLoad() {}

try {
  AboutThirdParty = Cc["@mozilla.org/about-thirdparty;1"].getService(
    Ci.nsIAboutThirdParty
  );
  document.addEventListener("DOMContentLoaded", onLoad, { once: true });
} catch (ex) {
  // Do nothing if we fail to create a singleton instance,
  // showing the default no-module message.
  Cu.reportError(ex);
}
