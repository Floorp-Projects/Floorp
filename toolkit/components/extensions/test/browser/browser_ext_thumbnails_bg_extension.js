"use strict";

/* import-globals-from ../../../thumbnails/test/head.js */
loadTestSubscript("../../../thumbnails/test/head.js");

// The service that creates thumbnails of webpages in the background loads a
// web page in the background (with several features disabled). Extensions
// should be able to observe requests, but not run content scripts.
add_task(async function test_thumbnails_background_visibility_to_extensions() {
  const iframeUrl = "http://example.com/?iframe";
  const testPageUrl = bgTestPageURL({ iframe: iframeUrl });
  // ^ testPageUrl is http://mochi.test:8888/.../thumbnails_background.sjs?...

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          // ":8888" omitted due to bug 1362809.
          matches: [
            "http://mochi.test/*/thumbnails_background.sjs*",
            "http://example.com/?iframe*",
          ],
          js: ["contentscript.js"],
          run_at: "document_start",
          all_frames: true,
        },
      ],
      permissions: [
        "webRequest",
        "webRequestBlocking",
        "http://example.com/*",
        "http://mochi.test/*",
      ],
    },
    files: {
      "contentscript.js": () => {
        // Content scripts are not expected to be run in the page of the
        // thumbnail service, so this should never execute.
        new Image().src = "http://example.com/?unexpected-content-script";
        browser.test.fail("Content script ran in thumbs, unexpectedly.");
      },
    },
    background() {
      let requests = [];
      browser.webRequest.onBeforeRequest.addListener(
        ({ url, tabId, frameId, type }) => {
          browser.test.assertEq(-1, tabId, "Thumb page is not a tab");
          // We want to know if frameId is 0 or non-negative (or possibly -1).
          if (type === "sub_frame") {
            browser.test.assertTrue(frameId > 0, `frame ${frameId} for ${url}`);
          } else {
            browser.test.assertEq(0, frameId, `frameId for ${type} ${url}`);
          }
          requests.push({ type, url });
        },
        {
          types: ["main_frame", "sub_frame", "image"],
          urls: ["*://*/*"],
        },
        ["blocking"]
      );
      browser.test.onMessage.addListener(msg => {
        browser.test.assertEq("get-results", msg, "expected message");
        browser.test.sendMessage("webRequest-results", requests);
      });
    },
  });

  await extension.startup();

  ok(!thumbnailExists(testPageUrl), "Thumbnail should not be cached yet.");

  await bgCapture(testPageUrl);
  ok(thumbnailExists(testPageUrl), "Thumbnail should be cached after capture");
  removeThumbnail(testPageUrl);

  extension.sendMessage("get-results");
  Assert.deepEqual(
    await extension.awaitMessage("webRequest-results"),
    [
      {
        type: "main_frame",
        url: testPageUrl,
      },
      {
        type: "sub_frame",
        url: iframeUrl,
      },
    ],
    "Expected requests via webRequest"
  );

  await extension.unload();
});
