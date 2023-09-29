/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  closeToolboxAndLog,
  garbageCollect,
  runTest,
  testSetup,
  testTeardown,
  PAGES_BASE_URL,
  waitForDOMElement,
} = require("../head");
const {
  createContext,
  findSource,
  getCM,
  hoverOnToken,
  openDebuggerAndLog,
  pauseDebugger,
  reloadDebuggerAndLog,
  removeBreakpoints,
  resume,
  selectSource,
  step,
  waitForSource,
  waitForText,
  evalInFrame,
  waitUntil,
} = require("./debugger-helpers");

const IFRAME_BASE_URL =
  "http://damp.top.com/tests/devtools/addon/content/pages/";
const EXPECTED = {
  sources: 1134,
  file: "App.js",
  sourceURL: `${IFRAME_BASE_URL}custom/debugger/app-build/static/js/App.js`,
  text: "import React, { Component } from 'react';",
  threadsCount: 2,
};

const EXPECTED_FUNCTION = "window.hitBreakpoint()";

const TEST_URL = PAGES_BASE_URL + "custom/debugger/app-build/index.html";

module.exports = async function () {
  const tab = await testSetup(TEST_URL, { disableCache: true });

  const toolbox = await openDebuggerAndLog("custom", EXPECTED);

  dump("Waiting for debugger panel\n");
  const panel = await toolbox.getPanelWhenReady("jsdebugger");

  dump("Creating context\n");
  const dbg = await createContext(panel);

  // Note that all sources added via eval, and all sources added by this function
  // will be gone when reloading the page in the next step.
  await testAddingSources(dbg, tab, toolbox);

  // Reselect App.js as that's the source expected to be selected after page reload
  await selectSource(dbg, EXPECTED.file);

  await reloadDebuggerAndLog("custom", toolbox, EXPECTED);

  // these tests are only run on custom.jsdebugger
  await pauseDebuggerAndLog(dbg, tab, EXPECTED_FUNCTION);
  await stepDebuggerAndLog(dbg, tab, EXPECTED_FUNCTION);

  await testProjectSearch(dbg, tab);
  await testPreview(dbg, tab, EXPECTED_FUNCTION);
  await testOpeningLargeMinifiedFile(dbg, tab);
  await testPrettyPrint(dbg);

  await closeToolboxAndLog("custom.jsdebugger", toolbox);

  await testTeardown();
};

async function pauseDebuggerAndLog(dbg, tab, testFunction) {
  const pauseLocation = { line: 22, file: "App.js" };

  dump("Pausing debugger\n");
  let test = runTest("custom.jsdebugger.pause.DAMP");
  await pauseDebugger(dbg, tab, testFunction, pauseLocation);
  test.done();

  await removeBreakpoints(dbg);
  await resume(dbg);
  await garbageCollect();
}

async function stepDebuggerAndLog(dbg, tab, testFunction) {
  /*
   * See testing/talos/talos/tests/devtools/addon/content/pages/custom/debugger/app/src for the details
   * about the pages used for these tests.
   */

  const stepTests = [
    // This steps only once from the App.js into step-in-test.js.
    // This `stepInNewSource` should always run first to make sure `step-in-test.js` file
    // is loaded for the first time.
    {
      stepCount: 1,
      location: { line: 22, file: "App.js" },
      key: "stepInNewSource",
      stepType: "stepIn",
    },
    {
      stepCount: 2,
      location: { line: 10194, file: "step-in-test.js" },
      key: "stepIn",
      stepType: "stepIn",
    },
    {
      stepCount: 2,
      location: { line: 16, file: "step-over-test.js" },
      key: "stepOver",
      stepType: "stepOver",
    },
    {
      stepCount: 2,
      location: { line: 998, file: "step-out-test.js" },
      key: "stepOut",
      stepType: "stepOut",
    },
  ];

  for (const stepTest of stepTests) {
    await pauseDebugger(dbg, tab, testFunction, stepTest.location);
    const test = runTest(`custom.jsdebugger.${stepTest.key}.DAMP`);
    for (let i = 0; i < stepTest.stepCount; i++) {
      await step(dbg, stepTest.stepType);
    }
    test.done();
    await removeBreakpoints(dbg);
    await resume(dbg);
    await garbageCollect();
  }
}

async function testProjectSearch(dbg, tab) {
  dump("Executing project search\n");
  const test = runTest(`custom.jsdebugger.project-search.DAMP`);
  const firstSearchResultTest = runTest(
    `custom.jsdebugger.project-search.first-search-result.DAMP`
  );
  await dbg.actions.setPrimaryPaneTab("project");
  await dbg.actions.setActiveSearch("project");
  const searchInput = await waitForDOMElement(
    dbg.win.document.querySelector("body"),
    ".project-text-search .search-field input"
  );
  searchInput.focus();
  searchInput.value = "retur";
  // Only dispatch a true key event for the last character in order to trigger only one search
  const key = "n";
  searchInput.dispatchEvent(
    new dbg.win.KeyboardEvent("keydown", {
      bubbles: true,
      cancelable: true,
      view: dbg.win,
      charCode: key.charCodeAt(0),
    })
  );
  searchInput.dispatchEvent(
    new dbg.win.KeyboardEvent("keyup", {
      bubbles: true,
      cancelable: true,
      view: dbg.win,
      charCode: key.charCodeAt(0),
    })
  );
  searchInput.dispatchEvent(
    new dbg.win.KeyboardEvent("keypress", {
      bubbles: true,
      cancelable: true,
      view: dbg.win,
      charCode: key.charCodeAt(0),
    })
  );

  // Wait till the first search result match is rendered
  await waitForDOMElement(
    dbg.win.document.querySelector("body"),
    ".project-text-search .tree-node .result"
  );
  firstSearchResultTest.done();
  // Then wait for all results to be fetched and the loader spin to hide
  await waitUntil(() => {
    return !dbg.win.document.querySelector(
      ".project-text-search .search-field .loader.spin"
    );
  });
  await dbg.actions.closeActiveSearch();
  test.done();
  await garbageCollect();
}

async function testPreview(dbg, tab, testFunction) {
  const pauseLocation = { line: 22, file: "App.js" };

  let test = runTest("custom.jsdebugger.preview.DAMP");
  await pauseDebugger(dbg, tab, testFunction, pauseLocation);
  await hoverOnToken(dbg, "window.hitBreakpoint", "window");
  test.done();

  await removeBreakpoints(dbg);
  await resume(dbg);
  await garbageCollect();
}

async function testOpeningLargeMinifiedFile(dbg, tab) {
  const file = `${IFRAME_BASE_URL}custom/debugger/app-build/static/js/minified.js`;
  const fileFirstChars = `(()=>{var e,t,n,r,o={82603`;

  dump("Open minified.js (large minified file)\n");
  const fullTest = runTest(
    "custom.jsdebugger.open-large-minified-file.full-selection.DAMP"
  );
  const test = runTest("custom.jsdebugger.open-large-minified-file.DAMP");
  const onSelected = selectSource(dbg, file);
  await waitForText(dbg, fileFirstChars);
  test.done();
  await onSelected;
  fullTest.done();

  dbg.actions.closeTabs([file]);

  await garbageCollect();
}

async function testPrettyPrint(dbg) {
  // Close all existing tabs to have a clean state
  const state = dbg.getState();
  const tabURLs = dbg.selectors.getSourcesForTabs(state).map(t => t.url);
  await dbg.actions.closeTabs(tabURLs);

  // Disable source map so we can prettyprint main.js
  await dbg.actions.toggleSourceMapsEnabled(false);

  const fileUrl = `${IFRAME_BASE_URL}custom/debugger/app-build/static/js/main.js`;
  const formattedFileUrl = `${fileUrl}:formatted`;

  dump("Select minified file\n");
  await selectSource(dbg, fileUrl);

  dump("Wait until CodeMirror highlighting is done\n");
  const cm = getCM(dbg);
  // highlightFrontier is not documented but is an internal variable indicating the current
  // line that was just highlighted. This document has only 2 lines, so wait until both
  // are highlighted. Since there was an other document opened before, we need to do an
  // exact check to properly wait.
  await waitUntil(() => cm.doc.highlightFrontier === 2);

  const prettyPrintButton = await waitUntil(() => {
    return dbg.win.document.querySelector(".source-footer .prettyPrint.active");
  });

  dump("Click pretty-print button\n");
  const test = runTest("custom.jsdebugger.pretty-print.DAMP");
  prettyPrintButton.click();
  await waitForSource(dbg, formattedFileUrl);
  await waitForText(dbg, "!function (n) {\n");
  test.done();

  await dbg.actions.toggleSourceMapsEnabled(true);
  dbg.actions.closeTabs([fileUrl, formattedFileUrl]);

  await garbageCollect();
}

async function testAddingSources(dbg, tab, toolbox) {
  // Before running the test, select an existing source in the two folders
  // where we add sources so that the added sources are made visible in the SourceTree.
  await selectSource(dbg, "js/testfile.js?id=0");
  await selectSource(dbg, "js/subfolder/testsubfolder.js");

  // Disabled ResourceCommand throttling so that the source notified by the server
  // is immediately processed by the client and we process each new source quickly.
  // Otherwise each source processing is faster than the throttling and we would mostly measure the throttling.
  toolbox.commands.resourceCommand.throttlingDisabled = true;
  const test = runTest("custom.jsdebugger.adding-sources.DAMP");

  for (let i = 0; i < 15; i++) {
    // Load source from two distinct folders to extend coverage around the source tree
    const sourceFilename =
      (i % 2 == 0 ? "testfile.js" : "testsubfolder.js") + "?dynamic-" + i;
    const sourcePath =
      i % 2 == 0 ? sourceFilename : "subfolder/" + sourceFilename;

    await evalInFrame(
      tab,
      `
      const script = document.createElement("script");
      script.src = "./js/${sourcePath}";
      document.body.appendChild(script);
    `
    );
    dump(`Wait for new source '${sourceFilename}'\n`);
    // Wait for the source to be in the redux store to avoid executing expensive DOM selectors.
    await waitUntil(() => findSource(dbg, sourceFilename));
    await waitUntil(() => {
      return Array.from(
        dbg.win.document.querySelectorAll(".sources-list .tree-node")
      ).some(e => e.textContent.includes(sourceFilename));
    });
  }

  test.done();
  toolbox.commands.resourceCommand.throttlingDisabled = false;
}
