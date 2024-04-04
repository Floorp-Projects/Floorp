"use strict";

const server = createHttpServer({
  hosts: ["example.net", "example.org"],
});
server.registerDirectory("/data/", do_get_file("data"));

// By default, fission.webContentIsolationStrategy=1 (IsolateHighValue). When
// Fission is enabled on Android, the pref value 2 (IsolateHighValue) will be
// used instead. Set to 1 (IsolateEverything) to make sure the subframe in this
// test gets its own process, independently of the default prefs on Android.
Services.prefs.setIntPref("fission.webContentIsolationStrategy", 1);

add_task(async function test_process_switch_cross_origin_frame() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.org/*/file_iframe.html"],
          all_frames: true,
          js: ["cs.js"],
        },
      ],
    },

    files: {
      "cs.js"() {
        browser.test.assertEq(
          location.href,
          "http://example.org/data/file_iframe.html",
          "url is ok"
        );

        // frameId is the BrowsingContext ID in practice.
        let frameId = browser.runtime.getFrameId(window);
        browser.test.sendMessage("content-script-loaded", frameId);
      },
    },
  });

  await extension.startup();

  const contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.net/data/file_with_xorigin_frame.html"
  );

  const browserProcessId =
    contentPage.browser.browsingContext.currentWindowGlobal.domProcess.childID;

  const scriptFrameId = await extension.awaitMessage("content-script-loaded");

  const children = contentPage.browser.browsingContext.children.map(bc => ({
    browsingContextId: bc.id,
    processId: bc.currentWindowGlobal.domProcess.childID,
  }));

  Assert.equal(children.length, 1);
  Assert.equal(scriptFrameId, children[0].browsingContextId);

  if (contentPage.remoteSubframes) {
    Assert.notEqual(browserProcessId, children[0].processId);
  } else {
    Assert.equal(browserProcessId, children[0].processId);
  }

  await contentPage.close();
  await extension.unload();
});
