/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { utils: Cu, classes: Cc, interfaces: Ci } = Components;

addMessageListener("compartments-test:setMonitoring", msg => {
  let stopwatchMonitoring = Cu.stopwatchMonitoring;
  Cu.stopwatchMonitoring = msg.data;
  sendAsyncMessage("compartments-test:setMonitoring", stopwatchMonitoring);
});

addMessageListener("compartments-test:getCompartments", () => {
  let compartmentInfo = Cc["@mozilla.org/compartment-info;1"]
          .getService(Ci.nsICompartmentInfo);
  let data = compartmentInfo.getCompartments();
  let result = [];
  for (let i = 0; i < data.length; ++i) {
    let xpcom = data.queryElementAt(i, Ci.nsICompartment);
    let object = {};
    for (let k of Object.keys(xpcom)) {
      let value = xpcom[k];
      if (typeof value != "function") {
        object[k] = value;
      }
    }
    object.frames = [];
    for (let i = 0; i < Ci.nsICompartment.MISSED_FRAMES_RANGE; ++i) {
      object.frames[i] = xpcom.getMissedFrames(i);
    }
    result.push(object);
  }
  sendAsyncMessage("compartments-test:getCompartments", result);
});
