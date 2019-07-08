"use strict";

add_task(async function test_extension_page_tabs_create_reload_and_close() {
  let events = [];
  {
    let { Management } = ChromeUtils.import(
      "resource://gre/modules/Extension.jsm",
      null
    );
    let record = (type, extensionContext) => {
      let eventType = type == "proxy-context-load" ? "load" : "unload";
      let url = extensionContext.uri.spec;
      let extensionId = extensionContext.extension.id;
      events.push({ eventType, url, extensionId });
    };

    Management.on("proxy-context-load", record);
    Management.on("proxy-context-unload", record);
    registerCleanupFunction(() => {
      Management.off("proxy-context-load", record);
      Management.off("proxy-context-unload", record);
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("tab-url", browser.runtime.getURL("page.html"));
    },
    files: {
      "page.html": `<!DOCTYPE html><meta charset="utf-8"><script src="page.js"></script>`,
      "page.js"() {
        browser.test.sendMessage("extension page loaded", document.URL);
      },
    },
  });

  await extension.startup();
  let tabURL = await extension.awaitMessage("tab-url");
  events.splice(0);

  let contentPage = await ExtensionTestUtils.loadContentPage(tabURL, {
    extension,
  });
  let extensionPageURL = await extension.awaitMessage("extension page loaded");
  equal(extensionPageURL, tabURL, "Loaded the expected URL");

  let contextEvents = events.splice(0);
  equal(contextEvents.length, 1, "ExtensionContext change for opening a tab");
  equal(contextEvents[0].eventType, "load", "create ExtensionContext for tab");
  equal(
    contextEvents[0].url,
    extensionPageURL,
    "ExtensionContext URL after tab creation should be tab URL"
  );

  await contentPage.spawn(null, () => {
    this.content.location.reload();
  });
  let extensionPageURL2 = await extension.awaitMessage("extension page loaded");

  equal(
    extensionPageURL,
    extensionPageURL2,
    "The tab's URL is expected to not change after a page reload"
  );

  contextEvents = events.splice(0);
  equal(contextEvents.length, 2, "ExtensionContext change after tab reload");
  equal(contextEvents[0].eventType, "unload", "unload old ExtensionContext");
  equal(
    contextEvents[0].url,
    extensionPageURL,
    "ExtensionContext URL before reload should be tab URL"
  );
  equal(
    contextEvents[1].eventType,
    "load",
    "create new ExtensionContext for tab"
  );
  equal(
    contextEvents[1].url,
    extensionPageURL2,
    "ExtensionContext URL after reload should be tab URL"
  );

  await contentPage.close();

  contextEvents = events.splice(0);
  equal(contextEvents.length, 1, "ExtensionContext after closing tab");
  equal(contextEvents[0].eventType, "unload", "unload tab's ExtensionContext");
  equal(
    contextEvents[0].url,
    extensionPageURL2,
    "ExtensionContext URL at closing tab should be tab URL"
  );

  await extension.unload();
});
