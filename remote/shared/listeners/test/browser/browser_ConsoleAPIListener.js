/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const TESTS = [
  { method: "log", args: ["log1"] },
  { method: "log", args: ["log2", "log3"] },
  { method: "log", args: [[1, 2, 3], { someProperty: "someValue" }] },
  { method: "warn", args: ["warn1"] },
  { method: "error", args: ["error1"] },
  { method: "info", args: ["info1"] },
  { method: "debug", args: ["debug1"] },
  { method: "trace", args: ["trace1"] },
  { method: "assert", args: [false, "assert1"] },
];

const TEST_PAGE = "https://example.com/document-builder.sjs?html=tab";

add_task(async function test_method_and_arguments() {
  for (const { method, args } of TESTS) {
    // Use a dedicated tab for each test to avoid cached messages.
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, TEST_PAGE);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    info(`Test ConsoleApiListener for ${JSON.stringify({ method, args })}`);

    const listenerId = await listenToConsoleAPIMessage();
    await useConsoleInContent(method, args);
    const {
      arguments: msgArguments,
      level,
      timeStamp,
      stacktrace,
    } = await getConsoleAPIMessage(listenerId);

    if (method == "assert") {
      // console.assert() consumes first argument.
      args.shift();
    }

    is(
      msgArguments.length,
      args.length,
      "Message event has the expected number of arguments"
    );
    for (let i = 0; i < args.length; i++) {
      Assert.deepEqual(
        msgArguments[i],
        args[i],
        `Message event has the expected argument at index ${i}`
      );
    }
    is(level, method, "Message event has the expected level");
    ok(Number.isInteger(timeStamp), "Message event has a valid timestamp");

    if (["assert", "error", "warn", "trace"].includes(method)) {
      // Check stacktrace if method is allowed to contain one.
      if (method === "warn") {
        todo(
          Array.isArray(stacktrace),
          "stacktrace is of expected type Array (Bug 1744705)"
        );
      } else {
        ok(Array.isArray(stacktrace), "stacktrace is of expected type Array");
        Assert.greaterOrEqual(
          stacktrace.length,
          1,
          "stack trace contains at least one frame"
        );
      }
    } else {
      is(typeof stacktrace, "undefined", "stack trace is is not present");
    }

    gBrowser.removeTab(gBrowser.selectedTab);
  }
});

add_task(async function test_stacktrace() {
  const script = `
    function foo() { console.error("cheese"); }
    function bar() { foo(); }
    bar();
  `;

  const listenerId = await listenToConsoleAPIMessage();
  await createScriptNode(script);
  const { stacktrace } = await getConsoleAPIMessage(listenerId);

  ok(Array.isArray(stacktrace), "stacktrace is of expected type Array");

  // First 3 frames are from the test script.
  Assert.greaterOrEqual(
    stacktrace.length,
    3,
    "stack trace contains at least 3 frames"
  );
  checkStackFrame(stacktrace[0], "about:blank", "foo", 2, 30);
  checkStackFrame(stacktrace[1], "about:blank", "bar", 3, 22);
  checkStackFrame(stacktrace[2], "about:blank", "", 4, 5);
});

function useConsoleInContent(method, args) {
  info(`Call console API: console.${method}("${args.join('", "')}");`);
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [method, args],
    (_method, _args) => {
      content.console[_method].apply(content.console, _args);
    }
  );
}

function listenToConsoleAPIMessage() {
  info("Listen to a console api message in content");
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    const innerWindowId = content.windowGlobalChild.innerWindowId;
    const { ConsoleAPIListener } = ChromeUtils.importESModule(
      "chrome://remote/content/shared/listeners/ConsoleAPIListener.sys.mjs"
    );
    const consoleAPIListener = new ConsoleAPIListener(innerWindowId);
    const onMessage = consoleAPIListener.once("message");
    consoleAPIListener.startListening();

    const listenerId = Math.random();
    content[listenerId] = { consoleAPIListener, onMessage };
    return listenerId;
  });
}

function getConsoleAPIMessage(listenerId) {
  info("Retrieve the message event captured for listener: " + listenerId);
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [listenerId],
    async _listenerId => {
      const { consoleAPIListener, onMessage } = content[_listenerId];
      const message = await onMessage;

      consoleAPIListener.destroy();

      return message;
    }
  );
}

function checkStackFrame(
  frame,
  filename,
  functionName,
  lineNumber,
  columnNumber
) {
  is(frame.filename, filename, "Received expected filename for frame");
  is(
    frame.functionName,
    functionName,
    "Received expected function name for frame"
  );
  is(frame.lineNumber, lineNumber, "Received expected line for frame");
  is(frame.columnNumber, columnNumber, "Received expected column for frame");
}
