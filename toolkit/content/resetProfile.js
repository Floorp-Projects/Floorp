/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// NB: this file can be loaded from aboutSupport.xhtml or from the
// resetProfile.xul dialog, and so Cu may or may not exist already.
// Proceed with caution:
if (!("Cu" in window)) {
  window.Cu = Components.utils;
}

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/ResetProfile.jsm");

function onResetProfileAccepted() {
  let retVals = window.arguments[0];
  retVals.reset = true;
}
