/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_error_location() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      let { fileName } = new Error();

      browser.test.sendMessage("fileName", fileName);

      browser.runtime.sendMessage("Meh.", () => {});

      await browser.test.assertRejects(
        browser.runtime.sendMessage("Meh"),
        error => {
          return error.fileName === fileName && error.lineNumber === 9;
        }
      );

      browser.test.notifyPass("error-location");
    },
  });

  let fileName;
  const { messages } = await promiseConsoleOutput(async () => {
    await extension.startup();

    fileName = await extension.awaitMessage("fileName");

    await extension.awaitFinish("error-location");

    await extension.unload();
  });

  let [msg] = messages.filter(m => m.message.includes("Unchecked lastError"));

  equal(msg.sourceName, fileName, "Message source");
  equal(msg.lineNumber, 6, "Message line");

  let frame = msg.stack;
  if (frame) {
    equal(frame.source, fileName, "Frame source");
    equal(frame.line, 6, "Frame line");
    equal(frame.column, 23, "Frame column");
    equal(frame.functionDisplayName, "background", "Frame function name");
  }
});
