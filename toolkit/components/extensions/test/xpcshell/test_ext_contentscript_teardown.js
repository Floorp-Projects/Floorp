"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

add_task(async function test_contentscript_reload_and_unload() {
  let extensionData = {
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/data/file_sample.html"],
          js: ["contentscript.js"],
        },
      ],
    },

    files: {
      "contentscript.js"() {
        browser.test.sendMessage("contentscript-run");
      },
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

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

  const tabUrl = "http://example.com/data/file_sample.html";
  let contentPage = await ExtensionTestUtils.loadContentPage(tabUrl);

  await extension.awaitMessage("contentscript-run");

  let contextEvents = events.splice(0);
  equal(
    contextEvents.length,
    1,
    "ExtensionContext state change after loading a content script"
  );
  equal(
    contextEvents[0].eventType,
    "load",
    "Create ExtensionContext for content script"
  );
  equal(contextEvents[0].url, tabUrl, "ExtensionContext URL = page");

  await contentPage.spawn(null, () => {
    this.content.location.reload();
  });
  await extension.awaitMessage("contentscript-run");

  contextEvents = events.splice(0);
  equal(
    contextEvents.length,
    2,
    "ExtensionContext state changes after reloading a content script"
  );
  equal(contextEvents[0].eventType, "unload", "Unload old ExtensionContext");
  equal(contextEvents[0].url, tabUrl, "ExtensionContext URL = page");
  equal(
    contextEvents[1].eventType,
    "load",
    "Create new ExtensionContext for content script"
  );
  equal(contextEvents[1].url, tabUrl, "ExtensionContext URL = page");

  await contentPage.close();

  contextEvents = events.splice(0);
  equal(
    contextEvents.length,
    1,
    "ExtensionContext state change after unloading a content script"
  );
  equal(
    contextEvents[0].eventType,
    "unload",
    "Unload ExtensionContext after closing the tab with the content script"
  );
  equal(contextEvents[0].url, tabUrl, "ExtensionContext URL = page");

  await extension.unload();
});
