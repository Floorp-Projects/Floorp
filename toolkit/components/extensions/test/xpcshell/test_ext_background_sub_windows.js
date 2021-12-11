/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testBackgroundWindow() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.log("background script executed");

      browser.test.sendMessage("background-script-load");

      let img = document.createElement("img");
      img.src =
        "data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7";
      document.body.appendChild(img);

      img.onload = () => {
        browser.test.log("image loaded");

        let iframe = document.createElement("iframe");
        iframe.src = "about:blank?1";

        iframe.onload = () => {
          browser.test.log("iframe loaded");
          setTimeout(() => {
            browser.test.notifyPass("background sub-window test done");
          }, 0);
        };
        document.body.appendChild(iframe);
      };
    },
  });

  let loadCount = 0;
  extension.onMessage("background-script-load", () => {
    loadCount++;
  });

  await extension.startup();

  await extension.awaitFinish("background sub-window test done");

  equal(loadCount, 1, "background script loaded only once");

  await extension.unload();
});
