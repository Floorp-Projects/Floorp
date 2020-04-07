/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
const ROOT_URL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);
const TEST_URL = "about:license";
const TEST_URL2 = ROOT_URL + "ioactivity.html";

var gotSocket = false;
var gotFile = false;
var gotSqlite = false;
var gotEmptyData = false;

function processResults(results) {
  for (let data of results) {
    console.log(data.location);
    gotEmptyData = data.rx == 0 && data.tx == 0 && !gotEmptyData;
    gotSocket = data.location.startsWith("socket://127.0.0.1:") || gotSocket;
    gotFile = data.location.endsWith("aboutLicense.css") || gotFile;
    gotSqlite = data.location.endsWith("places.sqlite") || gotSqlite;
    // check for the write-ahead file as well
    gotSqlite = data.location.endsWith("places.sqlite-wal") || gotSqlite;
  }
}

add_task(async function testRequestIOActivity() {
  await SpecialPowers.pushPrefEnv({
    set: [["io.activity.enabled", true]],
  });
  waitForExplicitFinish();
  Services.obs.notifyObservers(null, "profile-initial-state");

  await BrowserTestUtils.withNewTab(TEST_URL, async function(browser) {
    await BrowserTestUtils.withNewTab(TEST_URL2, async function(browser) {
      let results = await ChromeUtils.requestIOActivity();
      processResults(results);

      ok(gotSocket, "A socket was used");
      // test deactivated for now
      // ok(gotFile, "A file was used");
      ok(gotSqlite, "A sqlite DB was used");
      ok(!gotEmptyData, "Every I/O event had data");
    });
  });
});
