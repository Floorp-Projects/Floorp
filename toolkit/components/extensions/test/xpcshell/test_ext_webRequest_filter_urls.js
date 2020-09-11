/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

AddonTestUtils.init(this);

add_task(async function test_invalid_urls_in_webRequest_filter() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "https://example.com/*"],
    },
    background() {
      browser.webRequest.onBeforeRequest.addListener(() => {}, {
        urls: ["htt:/example.com/*"],
        types: ["main_frame"],
      });
    },
  });
  let { messages } = await promiseConsoleOutput(async () => {
    await extension.startup();
    await extension.unload();
  });
  AddonTestUtils.checkMessages(
    messages,
    {
      expected: [
        {
          message: /ExtensionError: Invalid url pattern: htt:\/example.com\/*/,
        },
      ],
    },
    true
  );
});
