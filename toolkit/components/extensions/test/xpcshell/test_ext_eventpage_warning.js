"use strict";

AddonTestUtils.init(this);
// This test expects and checks deprecation warnings.
ExtensionTestUtils.failOnSchemaWarnings(false);

function createEventPageExtension(eventPage) {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      background: eventPage,
    },
    files: {
      "event_page_script.js"() {
        browser.test.log("running event page as background script");
        browser.test.sendMessage("running", 1);
      },
      "event-page.html": `<!DOCTYPE html>
        <html><head>
          <meta charset="utf-8">
          <script src="event_page_script.js"><\/script>
        </head></html>`,
    },
  });
}

add_task(async function test_eventpages() {
  let testCases = [
    {
      message: "testing event page running as a background page",
      eventPage: {
        page: "event-page.html",
        persistent: false,
      },
    },
    {
      message: "testing event page scripts running as a background page",
      eventPage: {
        scripts: ["event_page_script.js"],
        persistent: false,
      },
    },
    {
      message: "testing additional unrecognized properties on background page",
      eventPage: {
        scripts: ["event_page_script.js"],
        nonExistentProp: true,
      },
    },
    {
      message: "testing persistent background page",
      eventPage: {
        page: "event-page.html",
        persistent: true,
      },
    },
    {
      message:
        "testing scripts with persistent background running as a background page",
      eventPage: {
        scripts: ["event_page_script.js"],
        persistent: true,
      },
    },
  ];

  let { messages } = await promiseConsoleOutput(async () => {
    for (let test of testCases) {
      info(test.message);

      let extension = createEventPageExtension(test.eventPage);
      await extension.startup();
      let x = await extension.awaitMessage("running");
      equal(x, 1, "got correct value from extension");
      await extension.unload();
    }
  });
  AddonTestUtils.checkMessages(
    messages,
    {
      expected: [
        { message: /Event pages are not currently supported./ },
        { message: /Event pages are not currently supported./ },
        {
          message: /Reading manifest: Warning processing background.nonExistentProp: An unexpected property was found/,
        },
      ],
    },
    true
  );
});
