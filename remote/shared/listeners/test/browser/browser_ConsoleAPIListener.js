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
    const consoleMessage = await getConsoleAPIMessage(listenerId);
    is(consoleMessage.level, method, "Message event has the expected level");
    ok(
      Number.isInteger(consoleMessage.timeStamp),
      "Message event has a valid timestamp"
    );

    is(
      consoleMessage.arguments.length,
      args.length,
      "Message event has the expected number of arguments"
    );
    for (let i = 0; i < args.length; i++) {
      Assert.deepEqual(
        consoleMessage.arguments[i],
        args[i],
        `Message event has the expected argument at index ${i}`
      );
    }

    gBrowser.removeTab(gBrowser.selectedTab);
  }
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
    const { ConsoleAPIListener } = ChromeUtils.import(
      "chrome://remote/content/shared/listeners/ConsoleAPIListener.jsm"
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
      // Note: we cannot return message directly here as it contains a
      // `rawMessage` object which cannot be serialized by SpecialPowers.
      return {
        arguments: message.arguments,
        level: message.level,
        timeStamp: message.timeStamp,
      };
    }
  );
}
