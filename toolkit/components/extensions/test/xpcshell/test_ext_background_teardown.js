"use strict";

add_task(async function test_background_reload_and_unload() {
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
      browser.test.onMessage.addListener(msg => {
        browser.test.assertEq("reload-background", msg);
        location.reload();
      });
      browser.test.sendMessage("background-url", location.href);
    },
  });

  await extension.startup();
  let backgroundUrl = await extension.awaitMessage("background-url");

  let contextEvents = events.splice(0);
  equal(
    contextEvents.length,
    1,
    "ExtensionContext state change after loading an extension"
  );
  equal(contextEvents[0].eventType, "load");
  equal(
    contextEvents[0].url,
    backgroundUrl,
    "The ExtensionContext should be the background page"
  );

  extension.sendMessage("reload-background");
  await extension.awaitMessage("background-url");

  contextEvents = events.splice(0);
  equal(
    contextEvents.length,
    2,
    "ExtensionContext state changes after reloading the background page"
  );
  equal(
    contextEvents[0].eventType,
    "unload",
    "Unload ExtensionContext of background page"
  );
  equal(
    contextEvents[0].url,
    backgroundUrl,
    "ExtensionContext URL = background"
  );
  equal(
    contextEvents[1].eventType,
    "load",
    "Create new ExtensionContext for background page"
  );
  equal(
    contextEvents[1].url,
    backgroundUrl,
    "ExtensionContext URL = background"
  );

  await extension.unload();

  contextEvents = events.splice(0);
  equal(
    contextEvents.length,
    1,
    "ExtensionContext state change after unloading the extension"
  );
  equal(
    contextEvents[0].eventType,
    "unload",
    "Unload ExtensionContext for background page after extension unloads"
  );
  equal(
    contextEvents[0].url,
    backgroundUrl,
    "ExtensionContext URL = background"
  );
});
