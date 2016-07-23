/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_reload_generated_background_page() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      if (location.hash !== "#firstrun") {
        browser.test.sendMessage("first run");
        location.hash = "#firstrun";
        browser.test.assertEq("#firstrun", location.hash);
        location.reload();
      } else {
        browser.test.notifyPass("second run");
      }
    },
  });

  yield extension.startup();
  yield extension.awaitMessage("first run");
  yield extension.awaitFinish("second run");

  yield extension.unload();
});
