/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/Services.jsm");

function burnCPOW(msg) {
  dump(`Addon: content burnCPU start ${Math.sin(Math.random())}\n`);
  let start = content.performance.now();
  let ignored = [];
  while (content.performance.now() - start < 5000) {
    ignored[ignored.length % 2] = ignored.length;
  }
  dump(`Addon: content burnCPU done: ${content.performance.now() - start}\n`);
}

if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
  sendAsyncMessage("test-addonwatcher-cpow:init", {}, {burnCPOW});
}

addMessageListener("test-addonwatcher-burn-some-content-cpu", burnCPOW);
