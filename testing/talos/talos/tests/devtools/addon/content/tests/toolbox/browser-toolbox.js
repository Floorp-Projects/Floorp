/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { runTest, testSetup, testTeardown } = require("../head");

const { DevToolsClient } = require("devtools/client/devtools-client");

const TEST_URL =
  "data:text/html,browser-toolbox-test<script>console.log('test page message');</script>";

module.exports = async function() {
  Services.prefs.setBoolPref("devtools.chrome.enabled", true);
  Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);
  Services.prefs.setBoolPref("devtools.debugger.prompt-connection", false);
  Services.prefs.setBoolPref(
    "devtools.browsertoolbox.enable-test-server",
    true
  );
  Services.prefs.setBoolPref("devtools.browsertoolbox.fission", true);
  Services.prefs.setCharPref("devtools.browsertoolbox.scope", "everything");
  // Ensure that the test page message will be visible
  Services.prefs.setBoolPref("devtools.browserconsole.contentMessages", true);

  // Open the browser toolbox on the options panel,
  // so that we can switch to all other panel individually and measure their opening time
  Services.prefs.setCharPref("devtools.browsertoolbox.panel", "options");

  await testSetup(TEST_URL);

  let test = runTest(`browser-toolbox.start-process.DAMP`, true);
  const { BrowserToolboxLauncher } = ChromeUtils.importESModule(
    "resource://devtools/client/framework/browser-toolbox/Launcher.sys.mjs"
  );
  const process = await new Promise(resolve => {
    BrowserToolboxLauncher.init({
      overwritePreferences: true,
      onRun: resolve,
    });
  });
  test.done();

  test = runTest(`browser-toolbox.connect.DAMP`, true);
  const consoleFront = await connectToBrowserToolbox();
  // Wait for the options panel to be fully initialized
  await evaluateInBrowserToolbox(consoleFront, [], async function() {
    /* global gToolbox */
    await gToolbox.selectTool("options");
  });
  test.done();

  test = runTest(`browser-toolbox.debugger-ready.DAMP`, true);
  await evaluateInBrowserToolbox(consoleFront, [], function() {
    /* global waitFor, findSource */
    this.findSource = (dbg, url) => {
      const sources = dbg.selectors.getSourceList(dbg.store.getState());
      return sources.find(s => (s.url || "").includes(url));
    };
    this.waitFor = async fn => {
      let rv;
      let count = 0;
      while (true) {
        try {
          rv = await fn();
          if (rv) {
            return rv;
          }
        } catch (e) {
          if (count > 100) {
            throw new Error("timeout on " + fn + " -- " + e + "\n");
          }
        }
        if (count > 100) {
          throw new Error("timeout on " + fn + "\n");
        }
        count++;

        await new Promise(r => setTimeout(r, 25));
      }
    };
  });

  await evaluateInBrowserToolbox(consoleFront, [TEST_URL], async function(
    testUrl
  ) {
    dump("Wait for debugger to initialize\n");
    const panel = await gToolbox.selectTool("jsdebugger");
    const { dbg } = panel.panelWin;
    dump("Wait for tab source in the content process\n");
    const source = await waitFor(() => findSource(dbg, testUrl));

    dump("Select this source\n");
    const cx = dbg.selectors.getContext(dbg.store.getState());
    dbg.actions.selectLocation(cx, { sourceId: source.id, line: 1 });
    await waitFor(() => {
      const source = dbg.selectors.getSelectedSource(dbg.store.getState());
      if (!source) {
        return false;
      }
      const sourceTextContent = dbg.selectors.getSelectedSourceTextContent(
        dbg.store.getState()
      );
      if (!sourceTextContent) {
        return false;
      }
      return true;
    });
  });
  test.done();

  test = runTest(`browser-toolbox.inspector-ready.DAMP`, true);
  await evaluateInBrowserToolbox(consoleFront, [], async function() {
    await gToolbox.selectTool("inspector");
  });
  test.done();

  test = runTest(`browser-toolbox.webconsole-ready.DAMP`, true);
  await evaluateInBrowserToolbox(consoleFront, [], async function() {
    const { hud } = await gToolbox.selectTool("webconsole");
    dump("Wait for test page console message to appear\n");
    await waitFor(() =>
      Array.from(
        hud.ui.window.document.querySelectorAll(".message-body")
      ).some(el => el.innerText.includes("test page message"))
    );
  });
  test.done();

  test = runTest(`browser-toolbox.styleeditor-ready.DAMP`, true);
  await evaluateInBrowserToolbox(consoleFront, [], async function() {
    await gToolbox.selectTool("styleeditor");
  });
  test.done();

  test = runTest(`browser-toolbox.close-process.DAMP`, true);
  await process.close();
  test.done();

  Services.prefs.clearUserPref("devtools.chrome.enabled");
  Services.prefs.clearUserPref("devtools.debugger.remote-enabled");
  Services.prefs.clearUserPref("devtools.debugger.prompt-connection");
  Services.prefs.clearUserPref("devtools.browsertoolbox.enable-test-server");
  Services.prefs.clearUserPref("devtools.browsertoolbox.fission");
  Services.prefs.clearUserPref("devtools.browsertoolbox.panel");
  Services.prefs.clearUserPref("devtools.browserconsole.contentMessages");
  Services.prefs.clearUserPref("devtools.browsertoolbox.scope");

  await testTeardown();
};

async function connectToBrowserToolbox() {
  let transport;
  while (true) {
    try {
      transport = await DevToolsClient.socketConnect({
        host: "localhost",
        port: 6001,
        webSocket: false,
      });
      break;
    } catch (e) {
      await new Promise(r => setTimeout(r, 100));
    }
  }

  const client = new DevToolsClient(transport);
  await client.connect();

  const descriptorFront = await client.mainRoot.getMainProcess();
  const target = await descriptorFront.getTarget();
  return target.getFront("console");
}

async function evaluateInBrowserToolbox(consoleFront, arg, fn) {
  const argString = JSON.stringify(arg);
  const onEvaluationResult = consoleFront.once("evaluationResult");
  await consoleFront.evaluateJSAsync({
    text: `(${fn}).apply(null,${argString})`,
    mapped: { await: true },
  });
  const result = await onEvaluationResult;
  if (result.topLevelAwaitRejected) {
    throw new Error("evaluation failed");
  }

  if (result.exceptionMessage) {
    throw new Error(result.exceptionMessage);
  }

  return result;
}
