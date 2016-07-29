/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* eslint-disable mozilla/balanced-listeners */

add_task(function* test_DOMContentLoaded_in_generated_background_page() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      function reportListener(event) {
        browser.test.sendMessage("eventname", event.type);
      }
      document.addEventListener("DOMContentLoaded", reportListener);
      window.addEventListener("load", reportListener);
    },
  });

  yield extension.startup();
  equal("DOMContentLoaded", yield extension.awaitMessage("eventname"));
  equal("load", yield extension.awaitMessage("eventname"));

  yield extension.unload();
});
