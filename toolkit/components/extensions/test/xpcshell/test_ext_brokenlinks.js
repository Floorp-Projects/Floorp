/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/*
 * This test extension has a background script 'missing.js' that is missing
 * from the XPI. Such an extension should install/uninstall cleanly without
 * causing timeouts.
 */
add_task(async function testXPIMissingBackGroundScript() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      background: {
        scripts: ["missing.js"],
      },
    },
  });

  await extension.startup();
  await extension.unload();
  ok(true, "load/unload completed without timing out");
});

/*
 * This test extension includes a page with a missing script. The
 * extension should install/uninstall cleanly without causing hangs.
 */
add_task(async function testXPIMissingPageScript() {
  async function pageScript() {
    browser.test.sendMessage("pageReady");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("ready", browser.runtime.getURL("page.html"));
    },
    files: {
      "page.html": `<html><head>
                    <script src="missing.js"></script>
                    <script src="page.js"></script>
                    </head></html>`,
      "page.js": pageScript,
    },
  });

  await extension.startup();
  let url = await extension.awaitMessage("ready");
  let contentPage = await ExtensionTestUtils.loadContentPage(url);
  await extension.awaitMessage("pageReady");
  await extension.unload();
  await contentPage.close();

  ok(true, "load/unload completed without timing out");
});
