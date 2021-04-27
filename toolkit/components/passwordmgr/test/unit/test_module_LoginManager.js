/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests the LoginManager module.
 */

"use strict";

const { LoginManager } = ChromeUtils.import(
  "resource://gre/modules/LoginManager.jsm"
);

add_task(async function test_ensureCurrentSyncID() {
  let loginManager = new LoginManager();
  await loginManager.setSyncID(1);
  await loginManager.setLastSync(100);

  // test calling ensureCurrentSyncID with the current sync ID
  Assert.equal(await loginManager.ensureCurrentSyncID(1), 1);
  Assert.equal(await loginManager.getSyncID(), 1, "sync ID shouldn't change");
  Assert.equal(
    await loginManager.getLastSync(),
    100,
    "last sync shouldn't change"
  );

  // test calling ensureCurrentSyncID with the different sync ID
  Assert.equal(await loginManager.ensureCurrentSyncID(2), 2);
  Assert.equal(await loginManager.getSyncID(), 2, "sync ID should be updated");
  Assert.equal(
    await loginManager.getLastSync(),
    0,
    "last sync should be reset"
  );
});
