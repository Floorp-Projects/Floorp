/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { openToolboxAndLog, closeToolboxAndLog, reloadPageAndLog, testSetup,
        testTeardown, SIMPLE_URL } = require("../head");
const { saveHeapSnapshot, readHeapSnapshot, takeCensus } = require("./memory-helpers");

module.exports = async function() {
  await testSetup(SIMPLE_URL);

  const toolbox = await openToolboxAndLog("simple.memory", "memory");
  await reloadPageAndLog("simple.memory", toolbox);

  let heapSnapshotFilePath = await saveHeapSnapshot("simple");
  let snapshot = await readHeapSnapshot("simple", heapSnapshotFilePath);
  await takeCensus("simple", snapshot);

  await closeToolboxAndLog("simple.memory", toolbox);
  await testTeardown();
};

