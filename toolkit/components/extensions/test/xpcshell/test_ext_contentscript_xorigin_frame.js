"use strict";

const server = createHttpServer({
  hosts: ["example.net", "example.org"],
});
server.registerDirectory("/data/", do_get_file("data"));

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

    background() {
      browser.runtime.onConnect.addListener(port => {
        port.onMessage.addListener(async () => {
          let { url, frameId } = port.sender;

          browser.test.assertTrue(frameId > 0, "sender frameId is ok");
          browser.test.assertTrue(
            url.endsWith("file_iframe.html"),
            "url is ok"
          );

          port.postMessage(frameId);
          port.disconnect();
        });
      });
    },

    files: {
      "cs.js"() {
        browser.test.assertEq(
          location.href,
          "http://example.org/data/file_iframe.html",
          "url is ok"
        );

        let frameId;
        let port = browser.runtime.connect();
        port.onMessage.addListener(response => {
          frameId = response;
        });
        port.onDisconnect.addListener(() => {
          browser.test.sendMessage("content-script-loaded", frameId);
        });
        port.postMessage("hello");
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
