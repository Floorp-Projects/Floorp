/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { openToolbox, closeToolbox, testSetup, testTeardown, runTest,
        SIMPLE_URL } = require("../head");

const protocol = require("devtools/shared/protocol");
const { dampTestSpec } = require("./spec");

// Test parameters
const ATTRIBUTES = 10;
const STRING_SIZE = 1000;
const ARRAY_SIZE = 50;
const REPEAT = 300;

const DampTestFront = protocol.FrontClassWithSpec(dampTestSpec, {
  initialize(client, tabForm) {
    this.actorID = tabForm.dampTestActor;
    protocol.Front.prototype.initialize.call(this, client);
    // Root owns itself.
    this.manage(this);
  }
});

module.exports = async function() {
  let tab = await testSetup(SIMPLE_URL);
  let messageManager = tab.linkedBrowser.messageManager;

  // Register a test actor within the content process
  messageManager.loadFrameScript("data:,(" + encodeURIComponent(
    `function () {
      const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});

      const { DebuggerServer } = require("devtools/server/main");
      DebuggerServer.registerModule("chrome://damp/content/tests/server/actor.js", {
        prefix: "dampTest",
        constructor: "DampTestActor",
        type: { target: true }
      });
    }`
  ) + ")()", true);

  // Create test payloads
  let bigString = "";
  for (let i = 0; i < STRING_SIZE; i++) {
    bigString += "x";
  }

  let bigObject = {};
  for (let i = 0; i < ATTRIBUTES; i++) {
    bigObject["attribute-" + i] = bigString;
  }

  let bigArray = Array.from({length: ARRAY_SIZE}, (_, i) => bigObject);

  // Open against options to avoid noise from tools
  let toolbox = await openToolbox("options");

  // Instanciate a front for this test actor
  let { target } = toolbox;
  let front = DampTestFront(target.client, target.form);

  // Execute the core of this test, call one method multiple times
  // and listen for an event sent by this method
  let test = runTest("server.protocoljs.DAMP");
  for (let i = 0; i < REPEAT; i++) {
    let onEvent = front.once("testEvent");
    await front.testMethod(bigArray, { option: bigArray }, ARRAY_SIZE);
    await onEvent;
  }
  test.done();

  await closeToolbox();
  await testTeardown();
};
