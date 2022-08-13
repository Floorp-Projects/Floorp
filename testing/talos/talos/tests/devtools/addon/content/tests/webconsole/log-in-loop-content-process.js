/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  logTestResult,
  testSetup,
  testTeardown,
  PAGES_BASE_URL,
} = require("damp-test/tests/head");
const {
  CommandsFactory,
} = require("devtools/shared/commands/commands-factory");

module.exports = async function() {
  let TOTAL_MESSAGES = 1000;
  let tab = await testSetup(PAGES_BASE_URL + "custom/console/bulklog.html");
  let messageManager = tab.linkedBrowser.messageManager;

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();
  await commands.resourceCommand.watchResources(
    [commands.resourceCommand.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable: () => {},
    }
  );

  // Load a frame script using a data URI so we can do logs from the page, running in
  // content process.
  messageManager.loadFrameScript(
    `data:application/javascript,(${encodeURIComponent(
      `function () {
        addMessageListener("do-logs", function ({data}) {
          const s = Cu.now();
          content.wrappedJSObject.doLogs(data, ${TOTAL_MESSAGES});
          sendAsyncMessage('logs-done',  Cu.now() - s);
          ChromeUtils.addProfilerMarker(
            "DAMP",
            { startTime: s, category: "Test" },
            "console.log-in-loop-content-process-" + data
          );
        });
      }`
    )})()`,
    true
  );

  const tests = [
    "string",
    "number",
    "bigint",
    "null",
    "undefined",
    "nan",
    "bool",
    "infinity",
    "symbol",
    "array",
    "typedarray",
    "set",
    "map",
    "object",
    "node",
    "nodelist",
    "promise",
    "error",
    "document",
    "window",
    "date",
    // longString used to be impacted by the number of actors created, so we let it at the
    // end of the list to make sure we have a sizable number of actors managed in the server.
    "longstring",
  ];

  for (const test of tests) {
    const onContentProcessLogsDone = new Promise(resolve => {
      messageManager.addMessageListener("logs-done", function onLogsDone(msg) {
        messageManager.removeMessageListener("logs-done", onLogsDone);
        resolve(msg.data);
      });
    });

    const label = "console.log-in-loop-content-process-" + test;
    dump(`Start "${label}"\n`);
    messageManager.sendAsyncMessage("do-logs", test);
    const contentProcessConsoleAPIDuration = await onContentProcessLogsDone;
    logTestResult(label, contentProcessConsoleAPIDuration);
  }

  commands.destroy();
  await testTeardown();
};
