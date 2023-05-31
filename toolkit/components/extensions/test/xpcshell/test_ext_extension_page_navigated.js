/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

const server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });

server.registerPathHandler("/", (request, response) => {
  response.write(`<!DOCTYPE html>
    <html>
      <head>
       <meta charset="utf-8">
       <title>test webpage</title>
      </head>
    </html>
  `);
});

function createTestExtPage({ script }) {
  return `<!DOCTYPE html>
    <html>
      <head>
       <meta charset="utf-8">
       <script src="${script}"></script>
      </head>
    </html>
  `;
}

function createTestExtPageScript(name) {
  return `(${async function (pageName) {
    browser.webRequest.onBeforeRequest.addListener(
      details => {
        browser.test.log(
          `${pageName} got a webRequest.onBeforeRequest event: ${details.url}`
        );
        browser.test.sendMessage(`event-received:${pageName}`);
      },
      { urls: ["http://example.com/request*"] }
    );

    // Calling an API implemented in the parent process to make sure
    // the webRequest.onBeforeRequest listener is got registered in
    // the parent process by the time the test is going to expect that
    // listener to intercept a test web request.
    await browser.runtime.getBrowserInfo();
    browser.test.sendMessage(`page-loaded:${pageName}`);
  }})("${name}");`;
}

const getExtensionContextIdAndURL = extensionId => {
  const { ExtensionProcessScript } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionProcessScript.sys.mjs"
  );
  let extWindow = this.content.window;
  let extChild = ExtensionProcessScript.getExtensionChild(extensionId);

  let contextIds = [];
  let contextURLs = [];
  for (let ctx of extChild.views) {
    if (ctx.contentWindow === extWindow) {
      // Only one is expected, but we collect details from all
      // the ones that match to make sure the test will fails
      // in case there are unexpected multiple extension contexts
      // associated to the same contentWindow.
      contextIds.push(ctx.contextId);
      contextURLs.push(ctx.contentWindow.location.href);
    }
  }
  return { contextIds, contextURLs };
};

const getExtensionContextStatusByContextId = (
  extensionId,
  extPageContextId
) => {
  const { ExtensionProcessScript } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionProcessScript.sys.mjs"
  );
  let extChild = ExtensionProcessScript.getExtensionChild(extensionId);

  let context;
  for (let ctx of extChild.views) {
    if (ctx.contextId === extPageContextId) {
      context = ctx;
    }
  }
  return context?.active;
};

add_task(async function test_extension_page_sameprocess_navigation() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "http://example.com/*"],
    },
    files: {
      "extpage1.html": createTestExtPage({ script: "extpage1.js" }),
      "extpage1.js": createTestExtPageScript("extpage1"),
      "extpage2.html": createTestExtPage({ script: "extpage2.js" }),
      "extpage2.js": createTestExtPageScript("extpage2"),
    },
  });

  await extension.startup();

  const policy = WebExtensionPolicy.getByID(extension.id);

  const extPageURL1 = policy.extension.baseURI.resolve("extpage1.html");
  const extPageURL2 = policy.extension.baseURI.resolve("extpage2.html");

  info("Opening extension page in a first browser element");
  const extPage = await ExtensionTestUtils.loadContentPage(extPageURL1);
  await extension.awaitMessage("page-loaded:extpage1");

  const { contextIds, contextURLs } = await extPage.spawn(
    [extension.id],
    getExtensionContextIdAndURL
  );

  Assert.deepEqual(
    contextURLs,
    [extPageURL1],
    `Found an extension context with the expected page url`
  );

  ok(
    contextIds[0],
    `Found an extension context with contextId ${contextIds[0]}`
  );
  ok(
    contextIds.length,
    `There should be only one extension context for a given content window, found ${contextIds.length}`
  );

  const [contextId] = contextIds;

  await ExtensionTestUtils.fetch(
    "http://example.com",
    "http://example.com/request1"
  );
  await extension.awaitMessage("event-received:extpage1");

  info("Load a second extension page in the same browser element");
  await extPage.loadURL(extPageURL2);
  await extension.awaitMessage("page-loaded:extpage2");

  let active;

  let { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
    // We only expect extpage2 to be able to receive API events.
    await ExtensionTestUtils.fetch(
      "http://example.com",
      "http://example.com/request2"
    );
    await extension.awaitMessage("event-received:extpage2");

    active = await extPage.spawn(
      [extension.id, contextId],
      getExtensionContextStatusByContextId
    );
  });

  if (
    Services.appinfo.sessionHistoryInParent &&
    WebExtensionPolicy.isExtensionProcess
  ) {
    // When the extension are running in the main process while the webpages run
    // in a separate child process, the extension page doesn't enter the BFCache
    // because nsFrameLoader::changeRemotenessCommon bails out due to retainPaint
    // being computed as true (see
    // https://searchfox.org/mozilla-central/rev/24c1cdc33ccce692612276cd0d3e9a44f6c22fd3/dom/base/nsFrameLoaderOwner.cpp#185-196
    // ).
    equal(active, undefined, "extension page context should not exist anymore");
  } else {
    equal(
      active,
      false,
      "extension page context is expected to be inactive while moved into the BFCache"
    );
  }

  if (typeof active === "boolean") {
    AddonTestUtils.checkMessages(
      messages,
      {
        forbidden: [
          // We should not have tried to deserialize the event data for the extension page
          // that got moved into the BFCache (See Bug 1499129).
          {
            message:
              /StructureCloneHolder.deserialize: Argument 1 is not an object/,
          },
        ],
        expected: [
          // If the extension page is expected to be in the BFCache, then we expect to see
          // a warning message logged for the ignored listener.
          {
            message:
              /Ignored listener for inactive context .* path=webRequest.onBeforeRequest/,
          },
        ],
      },
      "Expect no StructureCloneHolder error due to trying to send the event to inactive context"
    );
  }

  await extPage.close();
  await extension.unload();
});

add_task(async function test_extension_page_context_navigated_to_web_page() {
  const extension = ExtensionTestUtils.loadExtension({
    files: {
      "extpage.html": createTestExtPage({ script: "extpage.js" }),
      "extpage.js": function () {
        dump("loaded extension page\n");
        window.addEventListener(
          "pageshow",
          () => {
            browser.test.log("Extension page got a pageshow event");
            browser.test.sendMessage("extpage:pageshow");
          },
          { once: true }
        );
        window.addEventListener(
          "pagehide",
          () => {
            browser.test.log("Extension page got a pagehide event");
            browser.test.sendMessage("extpage:pagehide");
          },
          { once: true }
        );
      },
    },
  });

  await extension.startup();

  const policy = WebExtensionPolicy.getByID(extension.id);

  const extPageURL = policy.extension.baseURI.resolve("extpage.html");
  const webPageURL = "http://example.com/";

  info("Opening extension page in a browser element");
  const extPage = await ExtensionTestUtils.loadContentPage(extPageURL);
  await extension.awaitMessage("extpage:pageshow");

  const { contextIds, contextURLs } = await extPage.spawn(
    [extension.id],
    getExtensionContextIdAndURL
  );

  Assert.deepEqual(
    contextURLs,
    [extPageURL],
    `Found an extension context with the expected page url`
  );

  ok(
    contextIds[0],
    `Found an extension context with contextId ${contextIds[0]}`
  );
  ok(
    contextIds.length,
    `There should be only one extension context for a given content window, found ${contextIds.length}`
  );

  const [contextId] = contextIds;

  info("Load a webpage in the same browser element");
  await extPage.loadURL(webPageURL);
  await extension.awaitMessage("extpage:pagehide");

  info("Open extension page in a second browser element");
  const extPage2 = await ExtensionTestUtils.loadContentPage(extPageURL);
  await extension.awaitMessage("extpage:pageshow");

  let active = await extPage2.spawn(
    [extension.id, contextId],
    getExtensionContextStatusByContextId
  );

  if (WebExtensionPolicy.isExtensionProcess) {
    // When the extension are running in the main process while the webpages run
    // in a separate child process, the extension page doesn't enter the BFCache
    // because nsFrameLoader::changeRemotenessCommon bails out due to retainPaint
    // being computed as true (see
    // https://searchfox.org/mozilla-central/rev/24c1cdc33ccce692612276cd0d3e9a44f6c22fd3/dom/base/nsFrameLoaderOwner.cpp#185-196
    // ).
    equal(active, undefined, "extension page context should not exist anymore");
  } else if (Services.appinfo.sessionHistoryInParent) {
    // When SHIP is enabled and the extensions runs in their own child extension
    // process, the BFCache is managed entirely from the parent process and the
    // extension page is expected to be able to enter the BFCache.
    equal(
      active,
      false,
      "extension page context is expected to be inactive while moved into the BFCache"
    );
  } else {
    // With the extension running in a separate child process but fission disabled,
    // we expect the extension page to don't enter the BFCache.
    equal(active, undefined, "extension page context should not exist anymore");
  }

  if (active === false) {
    info(
      "Navigating to more web pages to confirm the extension page have been evicted from the BFCache"
    );
    for (let i = 2; i < 5; i++) {
      const url = `${webPageURL}/page${i}`;
      info(`Navigating to ${url}`);
      await extPage.loadURL(url);
    }
    equal(
      await extPage2.spawn(
        [extension.id, contextId],
        getExtensionContextStatusByContextId
      ),
      undefined,
      "extension page context should have been evicted"
    );
  }

  info("Cleanup and exit test");

  await Promise.all([
    extPage.close(),
    extPage2.close(),
    extension.awaitMessage("extpage:pagehide"),
  ]);
  await extension.unload();
});
