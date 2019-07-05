/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  reloadInspectorAndLog,
  selectNodeFront,
} = require("./inspector-helpers");
const {
  openToolboxAndLog,
  closeToolboxAndLog,
  runTest,
  testSetup,
  testTeardown,
  PAGES_BASE_URL,
} = require("../head");

module.exports = async function() {
  await testSetup(PAGES_BASE_URL + "custom/inspector/index.html");

  let toolbox = await openToolboxAndLog("custom.inspector", "inspector");
  await reloadInspectorAndLog("custom", toolbox);

  await selectNodeWithManyRulesAndLog(toolbox);

  await collapseExpandAllAndLog(toolbox);

  await closeToolboxAndLog("custom.inspector", toolbox);

  await testTeardown();
};

/**
 * Measure the time necessary to select a node and display the rule view when many rules
 * match the element.
 */
async function selectNodeWithManyRulesAndLog(toolbox) {
  let inspector = toolbox.getPanel("inspector");

  let initialNodeFront = inspector.selection.nodeFront;

  // Retrieve the node front for the test node.
  let root = await inspector.walker.getRootNode();
  let referenceNodeFront = await inspector.walker.querySelector(
    root,
    ".no-css-rules"
  );
  let testNodeFront = await inspector.walker.querySelector(
    root,
    ".many-css-rules"
  );

  // Select test node and measure the time to display the rule view with many rules.
  dump("Selecting .many-css-rules test node front\n");
  let test = runTest("custom.inspector.manyrules.selectnode");
  await selectNodeFront(inspector, testNodeFront);
  test.done();

  // Select reference node and measure the time to empty the rule view.
  dump("Move the selection to a node with no rules\n");
  test = runTest("custom.inspector.manyrules.deselectnode");
  await selectNodeFront(inspector, referenceNodeFront);
  test.done();

  await selectNodeFront(inspector, initialNodeFront);
}

async function collapseExpandAllAndLog(toolbox) {
  let inspector = toolbox.getPanel("inspector");

  let initialNodeFront = inspector.selection.nodeFront;
  let root = await inspector.walker.getRootNode();

  dump("Select expand-many-children node\n");
  let many = await inspector.walker.querySelector(
    root,
    ".expand-many-children"
  );
  await selectNodeFront(inspector, many);

  dump("Expand all children of expand-many-children\n");
  let test = runTest("custom.inspector.expandall.manychildren");
  await inspector.markup.expandAll(many);
  test.done();

  dump("Collapse all children of expand-many-children\n");
  test = runTest("custom.inspector.collapseall.manychildren");
  await inspector.markup.collapseAll(many);
  test.done();

  dump("Select expand-balanced node\n");
  let balanced = await inspector.walker.querySelector(root, ".expand-balanced");
  await selectNodeFront(inspector, balanced);

  dump("Expand all children of expand-balanced\n");
  test = runTest("custom.inspector.expandall.balanced");
  await inspector.markup.expandAll(balanced);
  test.done();

  dump("Collapse all children of expand-balanced\n");
  test = runTest("custom.inspector.collapseall.balanced");
  await inspector.markup.collapseAll(balanced);
  test.done();

  await selectNodeFront(inspector, initialNodeFront);
}
