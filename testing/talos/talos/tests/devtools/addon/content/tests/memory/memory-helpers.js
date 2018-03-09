/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getToolbox, runTest } = require("chrome://damp/content/tests/head");

exports.saveHeapSnapshot = async function(label) {
  let toolbox = getToolbox();
  let panel = toolbox.getCurrentPanel();
  let memoryFront = panel.panelWin.gFront;

  let test = runTest(label + ".saveHeapSnapshot");
  let heapSnapshotFilePath = await memoryFront.saveHeapSnapshot();
  test.done();

  return heapSnapshotFilePath;
};

exports.readHeapSnapshot = function(label, snapshotFilePath) {
  const ChromeUtils = require("ChromeUtils");
  let test = runTest(label + ".readHeapSnapshot");
  let snapshot = ChromeUtils.readHeapSnapshot(snapshotFilePath);
  test.done();

  return Promise.resolve(snapshot);
};

exports.takeCensus = function(label, snapshot) {
  let test = runTest("complicated.takeCensus");
  snapshot.takeCensus({
    breakdown: {
      by: "coarseType",
      objects: {
        by: "objectClass",
        then: { by: "count", bytes: true, count: true },
        other: { by: "count", bytes: true, count: true }
      },
      strings: {
        by: "internalType",
        then: { by: "count", bytes: true, count: true }
      },
      scripts: {
        by: "internalType",
        then: { by: "count", bytes: true, count: true }
      },
      other: {
        by: "internalType",
        then: { by: "count", bytes: true, count: true }
      }
    }
  });

  test.done();

  return Promise.resolve();
};
