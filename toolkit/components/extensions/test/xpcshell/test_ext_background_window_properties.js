/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testBackgroundWindowProperties() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      let expectedValues = {
        screenX: 0,
        screenY: 0,
        outerWidth: 0,
        outerHeight: 0,
      };

      for (let k in window) {
        try {
          if (k in expectedValues) {
            browser.test.assertEq(expectedValues[k], window[k],
                                  `should return the expected value for window property: ${k}`);
          } else {
            void window[k];
          }
        } catch (e) {
          browser.test.assertEq(null, e, `unexpected exception accessing window property: ${k}`);
        }
      }

      browser.test.notifyPass("background.testWindowProperties.done");
    },
  });
  yield extension.startup();
  yield extension.awaitFinish("background.testWindowProperties.done");
  yield extension.unload();
});
