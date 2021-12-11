/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  reloadInspectorAndLog,
  selectNodeFront,
} = require("damp-test/tests/inspector/inspector-helpers");
const {
  closeToolboxAndLog,
  garbageCollect,
  recordPendingPaints,
  runTest,
  testSetup,
  testTeardown,
  PAGES_BASE_URL,
} = require("damp-test/tests/head");

const { gDevTools } = require("devtools/client/framework/devtools");

const TEST_URL = PAGES_BASE_URL + "custom/inspector/index.html";

module.exports = async function() {
  const tab = await testSetup(TEST_URL, { disableCache: true });

  const domReference = await getContentDOMReference("#initial-node", tab);
  let toolbox = await openToolboxWithInspectNode(domReference, tab);

  await reloadInspectorAndLog("custom", toolbox);

  await selectNodeWithManyRulesAndLog(toolbox);

  await selectNodeWithManyVariablesAndLog(toolbox);

  await collapseExpandAllAndLog(toolbox);

  await closeToolboxAndLog("custom.inspector", toolbox);

  // Bug 1590308: Wait one second and force an additional GC to reduce the
  // side effects of the inspector custom test on the rest of the suite.
  await new Promise(r => setTimeout(r, 1000));
  await garbageCollect();

  await testTeardown();
};

// Retrieve the contentDOMReference for a provided selector that should be
// available in the content page of the provided tab.
async function getContentDOMReference(selector, tab) {
  dump("Retrieve the ContentDOMReference for a given selector");
  return new Promise(resolve => {
    const messageManager = tab.linkedBrowser.messageManager;

    messageManager.addMessageListener("get-dom-reference-done", e => {
      const domReference = e.data;
      resolve(domReference);
    });

    const contentMethod = function(_selector) {
      const { ContentDOMReference } = ChromeUtils.import(
        "resource://gre/modules/ContentDOMReference.jsm"
      );
      const iframe = content.document.querySelector("iframe");
      const win = iframe.contentWindow;
      const element = win.document.querySelector(_selector);
      const domReference = ContentDOMReference.get(element);
      sendAsyncMessage("get-dom-reference-done", domReference);
    };

    const wrappedMethod = encodeURIComponent(`function () {
      (${contentMethod})("${selector}");
     }`);

    messageManager.loadFrameScript(`data:,(${wrappedMethod})()`, false);
  });
}

async function openToolboxWithInspectNode(domReference, tab) {
  dump("Open the toolbox using InspectNode\n");

  const test = runTest(`custom.inspector.open.DAMP`);

  // Wait for "toolbox-created" to easily get access to the created toolbox.
  const onToolboxCreated = gDevTools.once("toolbox-created");

  await gDevTools.inspectNode(tab, domReference);
  const toolbox = await onToolboxCreated;
  test.done();

  // Wait for all pending paints to settle.
  await recordPendingPaints("custom.inspector.open", toolbox);

  // Toolbox creates many objects. See comment in head.js openToolboxAndLog.
  await garbageCollect();

  return toolbox;
}

async function getRootNodeFront(inspector) {
  const root = await inspector.walker.getRootNode();
  const iframeNodeFront = await inspector.walker.querySelector(root, "iframe");

  // Using iframes, retrieve the document in the iframe front children.
  const { nodes } = await inspector.walker.children(iframeNodeFront);
  return nodes[0];
}

/**
 * Measure the time necessary to select a node and display the rule view when many rules
 * match the element.
 */
async function selectNodeWithManyRulesAndLog(toolbox) {
  let inspector = toolbox.getPanel("inspector");

  let initialNodeFront = inspector.selection.nodeFront;

  // Retrieve the node front for the test node.
  let root = await getRootNodeFront(inspector);
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

/**
 * Measure the time necessary to select a node and display the rule view when many
 * CSS variables apply to the element.
 */
async function selectNodeWithManyVariablesAndLog(toolbox) {
  let inspector = toolbox.getPanel("inspector");
  let initialNodeFront = inspector.selection.nodeFront;

  // Retrieve the node front for the test node.
  let root = await getRootNodeFront(inspector);
  let testNodeFront = await inspector.walker.querySelector(
    root,
    ".many-css-variables"
  );

  // Select test node and measure the time to display the rule view with many variables.
  dump("Selecting .many-css-variables test node front\n");
  let test = runTest("custom.inspector.manycssvariables.selectnode");
  await selectNodeFront(inspector, testNodeFront);
  test.done();

  await selectNodeFront(inspector, initialNodeFront);
}

async function collapseExpandAllAndLog(toolbox) {
  let inspector = toolbox.getPanel("inspector");

  let initialNodeFront = inspector.selection.nodeFront;
  let root = await getRootNodeFront(inspector);

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
