/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { openToolboxAndLog, closeToolboxAndLog, reloadPageAndLog, testSetup,
        testTeardown, COMPLICATED_URL } = require("chrome://damp/content/tests/head");
const { saveHeapSnapshot, readHeapSnapshot, takeCensus } = require("chrome://damp/content/tests/memory/memory-helpers");

module.exports = async function() {
  await testSetup(COMPLICATED_URL);

  const toolbox = await openToolboxAndLog("complicated.memory", "memory");
  await reloadPageAndLog("complicated.memory", toolbox);

  let heapSnapshotFilePath = await saveHeapSnapshot("complicated");
  let snapshot = await readHeapSnapshot("complicated", heapSnapshotFilePath);
  await takeCensus("complicated", snapshot);

  await closeToolboxAndLog("complicated.memory", toolbox);
  await testTeardown();
};

