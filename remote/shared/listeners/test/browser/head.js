/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function clearConsole() {
  for (const tab of gBrowser.tabs) {
    await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
      Services.console.reset();
    });
  }
  Services.console.reset();
}

registerCleanupFunction(async () => {
  await clearConsole();
});

async function doGC() {
  // Run GC and CC a few times to make sure that as much as possible is freed.
  const numCycles = 3;
  for (let i = 0; i < numCycles; i++) {
    Cu.forceGC();
    Cu.forceCC();
    await new Promise(resolve => Cu.schedulePreciseShrinkingGC(resolve));
  }

  const MemoryReporter = Cc[
    "@mozilla.org/memory-reporter-manager;1"
  ].getService(Ci.nsIMemoryReporterManager);
  await new Promise(resolve => MemoryReporter.minimizeMemoryUsage(resolve));
}
