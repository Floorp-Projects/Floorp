"use strict";

// Check that extension popup windows contain the name of the extension
// as well as the title of the loaded document, but not the URL.
add_task(async function test_popup_title() {
  const name = "custom_title_number_9_please";
  const docTitle = "popup-test-title";

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name,
      permissions: [],
    },

    async background() {
      let { id } = await browser.windows.create({
        url: "/index.html",
        type: "popup",
      });

      // Called after the outer test code checks the actual window title.
      browser.test.onMessage.addListener(() => {
        browser.windows.remove(id);
        browser.test.notifyPass("popup-window-title");
      });

      browser.test.sendMessage("check-title", { id });
    },
    files: {
      "index.html": `<title>${docTitle}</title>`,
    },
  });

  extension.onMessage("check-title", ({ id }) => {
    const {
      document: { title },
    } = Services.wm.getOuterWindowWithId(id);

    ok(title.includes(name), "poup title must include extension name");
    ok(
      title.includes(docTitle),
      "popup title must include extension document title"
    );
    ok(
      !title.includes("moz-extension:"),
      "popup title must not include extension URL"
    );

    extension.sendMessage("all-done");
  });

  await extension.startup();
  await extension.awaitFinish("popup-window-title");
  await extension.unload();
});
