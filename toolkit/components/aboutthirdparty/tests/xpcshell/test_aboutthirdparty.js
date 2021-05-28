/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async () => {
  // Make sure successive calls of collectSystemInfo() do not
  // cause anything bad.
  const kLoopCount = 100;
  const promises = [];
  for (let i = 0; i < kLoopCount; ++i) {
    promises.push(kATP.collectSystemInfo());
  }

  const collectSystemInfoResults = await Promise.allSettled(promises);
  Assert.equal(collectSystemInfoResults.length, kLoopCount);

  for (const result of collectSystemInfoResults) {
    Assert.ok(
      result.status == "fulfilled",
      "All results from collectSystemInfo() are resolved."
    );
  }
});
