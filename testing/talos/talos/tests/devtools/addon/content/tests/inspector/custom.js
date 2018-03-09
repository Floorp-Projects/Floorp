/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { reloadInspectorAndLog } = require("chrome://damp/content/tests/inspector/inspector-helpers");
const { openToolboxAndLog, closeToolboxAndLog, runTest, testSetup,
        testTeardown, PAGES_BASE_URL } = require("chrome://damp/content/tests/head");

module.exports = async function() {
  await testSetup(PAGES_BASE_URL + "custom/inspector/index.html");

  let toolbox = await openToolboxAndLog("custom.inspector", "inspector");
  await reloadInspectorAndLog("custom", toolbox);
  await selectNodeWithManyRulesAndLog(toolbox);
  await closeToolboxAndLog("custom.inspector", toolbox);

  await testTeardown();
};

/**
 * Measure the time necessary to select a node and display the rule view when many rules
 * match the element.
 */
async function selectNodeWithManyRulesAndLog(toolbox) {
  let inspector = toolbox.getPanel("inspector");

  // Local helper to select a node front and wait for the ruleview to be refreshed.
  let selectNodeFront = (nodeFront) => {
    let onRuleViewRefreshed = inspector.once("rule-view-refreshed");
    inspector.selection.setNodeFront(nodeFront);
    return onRuleViewRefreshed;
  };

  let initialNodeFront = inspector.selection.nodeFront;

  // Retrieve the node front for the test node.
  let root = await inspector.walker.getRootNode();
  let referenceNodeFront = await inspector.walker.querySelector(root, ".no-css-rules");
  let testNodeFront = await inspector.walker.querySelector(root, ".many-css-rules");

  // Select test node and measure the time to display the rule view with many rules.
  dump("Selecting .many-css-rules test node front\n");
  let test = runTest("custom.inspector.manyrules.selectnode");
  await selectNodeFront(testNodeFront);
  test.done();

  // Select reference node and measure the time to empty the rule view.
  dump("Move the selection to a node with no rules\n");
  test = runTest("custom.inspector.manyrules.deselectnode");
  await selectNodeFront(referenceNodeFront);
  test.done();

  await selectNodeFront(initialNodeFront);
}
