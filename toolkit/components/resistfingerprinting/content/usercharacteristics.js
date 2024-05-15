/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

var debugMsgs = [];
function debug(...args) {
  let msg = "";
  if (!args.length) {
    debugMsgs.push("");
    return;
  }

  let stringify = o => {
    if (typeof o == "string") {
      return o;
    }
    return JSON.stringify(o);
  };

  let stringifiedArgs = args.map(stringify);
  msg += stringifiedArgs.join(" ");
  debugMsgs.push(msg);
}

debug("Debug Line");
debug("Another debug line, with", { an: "object" });

// The first time we put a real value in here, please update browser_usercharacteristics.js
let output = {
  foo: "Hello World",
};

document.dispatchEvent(
  new CustomEvent("UserCharacteristicsDataDone", {
    bubbles: true,
    detail: {
      debug: debugMsgs,
      output,
    },
  })
);
