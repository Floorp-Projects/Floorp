/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * An API for being informed of slow add-ons and tabs
 * (content process scripts).
 */

const { utils: Cu, classes: Cc, interfaces: Ci } = Components;
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

/**
 * `true` if this is a content process, `false` otherwise.
 */
let isContent = Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;

if (isContent) {

const { PerformanceWatcher } = Cu.import("resource://gre/modules/PerformanceWatcher.jsm", {});

let toMsg = function(alerts) {
  let result = [];
  for (let {source, details} of alerts) {
    // Convert xpcom values to serializable data.
    let serializableSource = {};
    for (let k of ["groupId", "name", "addonId", "windowId", "isSystem", "processId", "isContentProcess"]) {
      serializableSource[k] = source[k];
    }

    let serializableDetails = {};
    for (let k of ["reason", "highestJank", "highestCPOW"]) {
      serializableDetails[k] = details[k];
    }
    result.push({source:serializableSource, details:serializableDetails});
  }
  return result;
}

PerformanceWatcher.addPerformanceListener({addonId: "*"}, alerts => {
  Services.cpmm.sendAsyncMessage("performancewatcher-propagate-notifications",
    {addons: toMsg(alerts)}
  );
});

PerformanceWatcher.addPerformanceListener({windowId: 0}, alerts => {
  Services.cpmm.sendAsyncMessage("performancewatcher-propagate-notifications",
    {windows: toMsg(alerts)}
  );
});

}
