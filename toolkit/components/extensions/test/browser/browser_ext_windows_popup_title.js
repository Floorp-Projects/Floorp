"use strict";

// Check that extension popup windows contain the name of the extension
// as well as the title of the loaded document, but not the URL.
add_task(async function test_popup_title() {
  const name = "custom_title_number_9_please";
  const docTitle = "popup-test-title";

  const extensionWithImplicitHostPermission = ExtensionTestUtils.loadExtension({
    manifest: {
      name,
    },
    async background() {
      let popup;

      // Called after the popup loads
      browser.runtime.onMessage.addListener(async ({ docTitle }) => {
        const name = browser.runtime.getManifest().name;
        const { id } = await popup;
        const { title } = await browser.windows.get(id);

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

        // share window data with other extensions
        browser.test.sendMessage("windowData", {
          id: id,
          fullTitle: title,
        });

        browser.test.onMessage.addListener(async message => {
          if (message === "cleanup") {
            await browser.windows.remove(id);
            browser.test.sendMessage("finishedCleanup");
          }
        });

        browser.test.sendMessage("done");
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

  const extensionWithoutPermissions = ExtensionTestUtils.loadExtension({
    async background() {
      const { id } = await new Promise(resolve => {
        browser.test.onMessage.addListener(message => {
          resolve(message);
        });
      });

      const { title } = await browser.windows.get(id);

      browser.test.assertEq(
        title,
        undefined,
        "popup window must not include title"
      );

      browser.test.sendMessage("done");
    },
  });

  const extensionWithTabsPermission = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    async background() {
      const { id, fullTitle } = await new Promise(resolve => {
        browser.test.onMessage.addListener(message => {
          resolve(message);
        });
      });

      const { title } = await browser.windows.get(id);

      browser.test.assertEq(
        title,
        fullTitle,
        "popup title equals expected title"
      );

      browser.test.sendMessage("done");
    },
  });

  await extensionWithoutPermissions.startup();
  await extensionWithTabsPermission.startup();
  await extensionWithImplicitHostPermission.startup();

  const windowData = await extensionWithImplicitHostPermission.awaitMessage(
    "windowData"
  );

  extensionWithoutPermissions.sendMessage(windowData);
  extensionWithTabsPermission.sendMessage(windowData);

  await extensionWithoutPermissions.awaitMessage("done");
  await extensionWithTabsPermission.awaitMessage("done");
  await extensionWithImplicitHostPermission.awaitMessage("done");

  extensionWithImplicitHostPermission.sendMessage("cleanup");
  await extensionWithImplicitHostPermission.awaitMessage("finishedCleanup");

  await extensionWithoutPermissions.unload();
  await extensionWithTabsPermission.unload();
  await extensionWithImplicitHostPermission.unload();
});
