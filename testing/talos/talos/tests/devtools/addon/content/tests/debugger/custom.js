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
} = require("../head");
const {
  createContext,
  findSource,
  hoverOnToken,
  openDebuggerAndLog,
  pauseDebugger,
  reloadDebuggerAndLog,
  removeBreakpoints,
  resume,
  selectSource,
  step,
  waitForText,
  waitUntil,
} = require("./debugger-helpers");

const IFRAME_BASE_URL =
  "http://damp.top.com/tests/devtools/addon/content/pages/";
const EXPECTED = {
  sources: 107,
  file: "App.js",
  sourceURL: `${IFRAME_BASE_URL}custom/debugger/static/js/App.js`,
  text: "import React, { Component } from 'react';",
};

const EXPECTED_FUNCTION = "window.hitBreakpoint()";

const TEST_URL = PAGES_BASE_URL + "custom/debugger/index.html";

module.exports = async function() {
  const tab = await testSetup(TEST_URL, { disableCache: true });
  Services.prefs.setBoolPref("devtools.debugger.features.map-scopes", false);

  const toolbox = await openDebuggerAndLog("custom", EXPECTED);
  await reloadDebuggerAndLog("custom", toolbox, EXPECTED);

  // these tests are only run on custom.jsdebugger
  await pauseDebuggerAndLog(tab, toolbox, EXPECTED_FUNCTION);
  await stepDebuggerAndLog(tab, toolbox, EXPECTED_FUNCTION);

  await testProjectSearch(tab, toolbox);
  await testPreview(tab, toolbox, EXPECTED_FUNCTION);
  await testOpeningLargeMinifiedFile(tab, toolbox);

  await closeToolboxAndLog("custom.jsdebugger", toolbox);

  Services.prefs.clearUserPref("devtools.debugger.features.map-scopes");
  await testTeardown();
};

async function pauseDebuggerAndLog(tab, toolbox, testFunction) {
  dump("Waiting for debugger panel\n");
  const panel = await toolbox.getPanelWhenReady("jsdebugger");

  dump("Creating context\n");
  const dbg = await createContext(panel);

  const pauseLocation = { line: 22, file: "App.js" };

  dump("Pausing debugger\n");
  let test = runTest("custom.jsdebugger.pause.DAMP");
  await pauseDebugger(dbg, tab, testFunction, pauseLocation);
  test.done();

  await removeBreakpoints(dbg);
  await resume(dbg);
  await garbageCollect();
}

async function stepDebuggerAndLog(tab, toolbox, testFunction) {
  const panel = await toolbox.getPanelWhenReady("jsdebugger");
  const dbg = await createContext(panel);
  const stepCount = 2;

  /*
   * Each Step test has a max step count of at least 200;
   * see https://github.com/codehag/debugger-talos-example/blob/master/src/ and the specific test
   * file for more information
   */

  const stepTests = [
    {
      location: { line: 10194, file: "step-in-test.js" },
      key: "stepIn",
    },
    {
      location: { line: 16, file: "step-over-test.js" },
      key: "stepOver",
    },
    {
      location: { line: 998, file: "step-out-test.js" },
      key: "stepOut",
    },
  ];

  for (const stepTest of stepTests) {
    await pauseDebugger(dbg, tab, testFunction, stepTest.location);
    const test = runTest(`custom.jsdebugger.${stepTest.key}.DAMP`);
    for (let i = 0; i < stepCount; i++) {
      await step(dbg, stepTest.key);
    }
    test.done();
    await removeBreakpoints(dbg);
    await resume(dbg);
    await garbageCollect();
  }
}

async function testProjectSearch(tab, toolbox) {
  const panel = await toolbox.getPanelWhenReady("jsdebugger");
  const dbg = await createContext(panel);
  const cx = dbg.selectors.getContext(dbg.getState());

  dump("Executing project search\n");
  const test = runTest(`custom.jsdebugger.project-search.DAMP`);
  await dbg.actions.setActiveSearch("project");
  await dbg.actions.searchSources(cx, "return");
  await dbg.actions.closeActiveSearch();
  test.done();
  await garbageCollect();
}

async function testPreview(tab, toolbox, testFunction) {
  const panel = await toolbox.getPanelWhenReady("jsdebugger");
  const dbg = await createContext(panel);
  const cx = dbg.selectors.getContext(dbg.getState());
  const pauseLocation = { line: 22, file: "App.js" };

  let test = runTest("custom.jsdebugger.preview.DAMP");
  await pauseDebugger(dbg, tab, testFunction, pauseLocation);
  await hoverOnToken(dbg, cx, "window.hitBreakpoint", "window");
  dbg.actions.clearPreview(cx);
  test.done();

  await removeBreakpoints(dbg);
  await resume(dbg);
  await garbageCollect();
}

async function testOpeningLargeMinifiedFile(tab, toolbox) {
  dump("Waiting for debugger panel\n");
  const panel = await toolbox.getPanelWhenReady("jsdebugger");

  dump("Creating context\n");
  const dbg = await createContext(panel);

  dump("Add minified.js (large minified file)\n");
  const file = `${IFRAME_BASE_URL}custom/debugger/static/js/minified.js`;

  const messageManager = tab.linkedBrowser.messageManager;

  // We don't want to impact the other tests from this file, so we add a new big minified
  // file from the content process instead of having it directly in iframe.html.
  messageManager.loadFrameScript(
    `data:application/javascript,(${encodeURIComponent(
      `function () {
        const scriptEl = content.document.createElement("script");
        scriptEl.setAttribute("type", "text/javascript");
        scriptEl.setAttribute("src", "${file}");
        content.document.body.append(scriptEl);
      }`
    )})()`,
    true
  );

  dump("Wait until source is available\n");
  await waitUntil(() => findSource(dbg, file));

  const fileFirstChars = `(()=>{var e,t,n,r,o={82603`;

  dump("Open minified.js (large minified file)\n");
  const test = runTest("custom.jsdebugger.open-large-minified-file.DAMP");
  await selectSource(dbg, file);
  await waitForText(dbg, file, fileFirstChars);
  test.done();

  dbg.actions.closeTabs(dbg.selectors.getContext(dbg.getState()), [file]);

  await garbageCollect();
}
