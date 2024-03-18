/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1713701 - Shim Integral Ad Science iaspet.js
 *
 * Some sites use iaspet to place content, often together with Google Publisher
 * Tags. This shim prevents breakage when the script is blocked.
 */

if (!window.__iasPET?.VERSION) {
  let queue = window?.__iasPET?.queue;
  if (!Array.isArray(queue)) {
    queue = [];
  }

  const response = JSON.stringify({
    brandSafety: {},
    slots: {},
  });

  function run(cmd) {
    try {
      cmd?.dataHandler?.(response);
    } catch (_) {}
  }

  queue.push = run;

  window.__iasPET = {
    VERSION: "1.16.18",
    queue,
    sessionId: "",
    setTargetingForAppNexus() {},
    setTargetingForGPT() {},
    start() {},
  };

  while (queue.length) {
    run(queue.shift());
  }
}
