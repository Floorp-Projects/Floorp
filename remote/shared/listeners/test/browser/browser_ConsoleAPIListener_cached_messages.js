/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_PAGE = "https://example.com/document-builder.sjs?html=tab";

add_task(async function test_cached_messages() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, TEST_PAGE);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    const innerWindowId = content.windowGlobalChild.innerWindowId;
    const { ConsoleAPIListener } = ChromeUtils.importESModule(
      "chrome://remote/content/shared/listeners/ConsoleAPIListener.sys.mjs"
    );

    info("Log two messages before starting the ConsoleAPIListener");
    content.console.log("message_1");
    content.console.log("message_2");

    const listener = new ConsoleAPIListener(innerWindowId);
    const messages = [];

    // We will keep the onMessage callback attached to the ConsoleAPIListener
    // during the whole test to catch all the emitted events.
    const onMessage = (evtName, message) => messages.push(message.arguments[0]);

    listener.on("message", onMessage);
    listener.startListening();

    info("Wait until the 2 cached messages have been emitted");
    await ContentTaskUtils.waitForCondition(() => messages.length == 2);
    is(messages[0], "message_1");
    is(messages[1], "message_2");

    info("Stop listening and log another message");
    listener.stopListening();
    content.backup = { listener, messages, onMessage };
  });

  // Force a GC to check that old cached messages which have been garbage
  // collected are not re-displayed.
  await doGC();

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    const { listener, messages, onMessage } = content.backup;
    content.console.log("message_3");

    info("Start listening again and check the previous message is emitted");
    listener.startListening();
    await ContentTaskUtils.waitForCondition(() => messages.length == 3);
    is(messages[2], "message_3");

    info("Log another message and wait until it is emitted");
    content.console.log("message_4");
    await ContentTaskUtils.waitForCondition(() => messages.length == 4);
    is(messages[3], "message_4");

    listener.off("message", onMessage);
    listener.destroy();

    is(messages.length, 4, "Received 4 messages in total");
  });

  info("Reload the current tab and check only new messages are emitted");
  await BrowserTestUtils.reloadTab(gBrowser.selectedTab);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    const innerWindowId = content.windowGlobalChild.innerWindowId;
    const { ConsoleAPIListener } = ChromeUtils.importESModule(
      "chrome://remote/content/shared/listeners/ConsoleAPIListener.sys.mjs"
    );

    info("Log a message before creating the ConsoleAPIListener");
    content.console.log("new_message_1");

    const listener = new ConsoleAPIListener(innerWindowId);
    const newMessages = [];
    const onMessage = (evtName, message) =>
      newMessages.push(message.arguments[0]);
    listener.on("message", onMessage);

    info("Start listening and wait for the cached message");
    listener.startListening();
    await ContentTaskUtils.waitForCondition(() => newMessages.length == 1);
    is(newMessages[0], "new_message_1");

    info("Log another message and wait until it is emitted");
    content.console.log("new_message_2");
    await ContentTaskUtils.waitForCondition(() => newMessages.length == 2);
    is(newMessages[1], "new_message_2");

    listener.off("message", onMessage);
    listener.destroy();

    is(newMessages.length, 2, "Received 2 messages in total");
  });

  gBrowser.removeTab(gBrowser.selectedTab);
});
