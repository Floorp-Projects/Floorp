/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

addEventListener("load", function(event) {
  let subframe = event.target != content.document;
  sendAsyncMessage("browser-test-utils:loadEvent",
    {subframe: subframe, url: event.target.documentURI});
}, true);

