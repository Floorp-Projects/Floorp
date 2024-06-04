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

// A hacky way to ensure all GamePad related services are running by the time we
// want to know about GamePads. This will attach a listener for this window to
// the GamePadManager, creating it if it doesn't exist. When GamePadManager is
// created (either now or earlier) it will also create a GamePadEventChannel
// between the GamePadManager and the parent process. When that's created, the
// Parent will create a GamePadPlatformService if it's not already created, and
// when that Service gets created it kicks off a background thread to monitor
// for gamepads attached to the machine. We need to give that background thread
// time to run so all the data is there when we request it.
navigator.getGamepads();

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
