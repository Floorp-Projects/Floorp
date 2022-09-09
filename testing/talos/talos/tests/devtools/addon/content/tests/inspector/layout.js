/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolbox,
  closeToolbox,
  runTest,
  testSetup,
  testTeardown,
  SIMPLE_URL,
} = require("damp-test/tests/head");

module.exports = async function() {
  let tab = await testSetup(SIMPLE_URL);
  let messageManager = tab.linkedBrowser.messageManager;

  // Backup current sidebar tab preference
  let sidebarTab = Services.prefs.getCharPref(
    "devtools.inspector.activeSidebar"
  );

  // Set layoutview as the current inspector sidebar tab.
  Services.prefs.setCharPref("devtools.inspector.activeSidebar", "layoutview");

  // Setup test page. It is a simple page containing 5000 regular nodes and 10 grid
  // containers.
  await new Promise(resolve => {
    messageManager.addMessageListener("setup-test-done", resolve);

    const NODES = 5000;
    const GRID_NODES = 10;
    messageManager.loadFrameScript(
      "data:,(" +
        encodeURIComponent(
          `function () {
        let div = content.document.createElement("div");
        div.innerHTML =
          new Array(${NODES}).join("<div></div>") +
          new Array(${GRID_NODES}).join("<div style='display:grid'></div>");
        content.document.body.appendChild(div);
        sendSyncMessage("setup-test-done");
      }`
        ) +
        ")()",
      false
    );
  });

  // Record the time needed to open the toolbox.
  let test = runTest("inspector.layout.open");
  await openToolbox("inspector");
  test.done();

  await closeToolbox();

  // Restore sidebar tab preference.
  Services.prefs.setCharPref("devtools.inspector.activeSidebar", sidebarTab);

  await testTeardown();
};
