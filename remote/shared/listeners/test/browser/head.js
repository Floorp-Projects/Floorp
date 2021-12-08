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
