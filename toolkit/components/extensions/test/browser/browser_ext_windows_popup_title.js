"use strict";

// Check that extension popup windows contain the name of the extension
// as well as the title of the loaded document, but not the URL.
add_task(async function test_popup_title() {
  const name = "custom_title_number_9_please";
  const docTitle = "popup-test-title";

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name,
      permissions: ["tabs"],
    },

    async background() {
      let popup;

      // Called after the popup loads
      browser.runtime.onMessage.addListener(async ({ docTitle }) => {
        const { id } = await popup;
        const { title } = await browser.windows.get(id);
        browser.windows.remove(id);

        browser.test.assertTrue(
          title.includes(name),
          "popup title must include extension name"
        );
        browser.test.assertTrue(
          title.includes(docTitle),
          "popup title must include extension document title"
        );
        browser.test.assertFalse(
          title.includes("moz-extension:"),
          "popup title must not include extension URL"
        );

        browser.test.notifyPass("popup-window-title");
      });

      popup = browser.windows.create({
        url: "/index.html",
        type: "popup",
      });
    },
    files: {
      "index.html": `<!doctype html>
        <meta charset="utf-8">
        <title>${docTitle}</title>,
        <script src="index.js"></script>
      `,
      "index.js": `addEventListener(
          "load",
          () => browser.runtime.sendMessage({docTitle: document.title})
         );`,
    },
  });

  await extension.startup();
  await extension.awaitFinish("popup-window-title");
  await extension.unload();
});
