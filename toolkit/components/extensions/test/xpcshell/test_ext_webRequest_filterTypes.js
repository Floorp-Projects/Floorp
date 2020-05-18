"use strict";

AddonTestUtils.init(this);

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/", (request, response) => {
  response.setHeader("Content-Tpe", "text/plain", false);
  response.write("OK");
});

add_task(async function test_all_webRequest_ResourceTypes() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "*://example.com/*"],
    },
    background() {
      browser.test.onMessage.addListener(async msg => {
        browser.webRequest[msg.event].addListener(
          () => {},
          { urls: ["*://example.com/*"], ...msg.filter },
          ["blocking"]
        );
        // Call an API method implemented in the parent process to
        // be sure that the webRequest listener has been registered
        // in the parent process as well.
        await browser.runtime.getBrowserInfo();
        browser.test.sendMessage(`webRequest-listener-registered`);
      });
    },
  });

  await extension.startup();

  const { Schemas } = ChromeUtils.import("resource://gre/modules/Schemas.jsm");
  const webRequestSchema = Schemas.privilegedSchemaJSON
    .get("chrome://extensions/content/schemas/web_request.json")
    .deserialize({});
  const ResourceType = webRequestSchema[1].types.filter(
    type => type.id == "ResourceType"
  )[0];
  ok(
    ResourceType && ResourceType.enum,
    "Found ResourceType in the web_request.json schema"
  );
  info(
    "Register webRequest.onBeforeRequest event listener for all supported ResourceType"
  );

  let { messages } = await promiseConsoleOutput(async () => {
    ExtensionTestUtils.failOnSchemaWarnings(false);
    extension.sendMessage({
      event: "onBeforeRequest",
      filter: {
        // Verify that the resourceType not supported is going to be ignored
        // and all the ones supported does not trigger a ChannelWrapper.matches
        // exception once the listener is being triggered.
        types: [].concat(ResourceType.enum, "not-supported-resource-type"),
      },
    });
    await extension.awaitMessage("webRequest-listener-registered");
    ExtensionTestUtils.failOnSchemaWarnings();

    await ExtensionTestUtils.fetch(
      "http://example.com/dummy",
      "http://example.com"
    );
  });

  AddonTestUtils.checkMessages(messages, {
    expected: [
      { message: /Warning processing types: .* "not-supported-resource-type"/ },
    ],
    forbidden: [{ message: /JavaScript Error: "ChannelWrapper.matches/ }],
  });
  info("No ChannelWrapper.matches errors have been logged");

  await extension.unload();
});
