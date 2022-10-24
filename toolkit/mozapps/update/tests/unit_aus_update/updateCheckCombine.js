/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

/**
 * This test checks that multiple foreground update checks are combined into
 * a single web request.
 */

add_task(async function setup() {
  setupTestCommon();
  start_httpserver();
  setUpdateURL(gURLData + gHTTPHandlerPath);
  setUpdateChannel("test_channel");

  let patches = getRemotePatchString({});
  let updateString = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updateString);
});

add_task(async function testUpdateCheckCombine() {
  gUpdateCheckCount = 0;
  let check1 = gUpdateChecker.checkForUpdates(gUpdateChecker.FOREGROUND_CHECK);
  let check2 = gUpdateChecker.checkForUpdates(gUpdateChecker.FOREGROUND_CHECK);

  let result1 = await check1.result;
  let result2 = await check2.result;
  Assert.ok(result1.succeeded, "Check 1 should have succeeded");
  Assert.ok(result2.succeeded, "Check 2 should have succeeded");
  Assert.equal(gUpdateCheckCount, 1, "Should only have made a single request");
});

add_task(async function finish() {
  stop_httpserver(doTestFinish);
});
