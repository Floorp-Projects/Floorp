/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../file_ime_state_test_helper.js */
/* import-globals-from ../file_test_ime_state_on_input_type_change.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/widget/tests/browser/file_ime_state_test_helper.js",
  this
);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/widget/tests/browser/file_test_ime_state_on_input_type_change.js",
  this
);
add_task(async function () {
  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/widget/tests/browser/file_ime_state_tests.html",
    async function (browser) {
      const tipWrapper = new TIPWrapper(window);
      ok(
        tipWrapper.isAvailable(),
        "TextInputProcessor should've been initialized"
      );

      for (
        let srcIndex = 0;
        srcIndex < IMEStateOnInputTypeChangeTester.numberOfTests;
        srcIndex++
      ) {
        const tester = new IMEStateOnInputTypeChangeTester(srcIndex);
        for (
          let destIndex = 0;
          destIndex < IMEStateOnInputTypeChangeTester.numberOfTests;
          destIndex++
        ) {
          const expectedResultBefore = await SpecialPowers.spawn(
            browser,
            [srcIndex, destIndex],
            (aSrcIndex, aDestIndex) => {
              content.wrappedJSObject.runner =
                content.wrappedJSObject.createIMEStateOnInputTypeChangeTester(
                  aSrcIndex
                );
              return content.wrappedJSObject.runner.prepareToRun(
                aDestIndex,
                content.window,
                content.document.body
              );
            }
          );
          if (expectedResultBefore === false) {
            continue;
          }
          tester.checkBeforeRun(expectedResultBefore, tipWrapper);
          const expectedResult = await SpecialPowers.spawn(browser, [], () => {
            return content.wrappedJSObject.runner.run();
          });
          tester.checkResult(expectedResultBefore, expectedResult);
          await SpecialPowers.spawn(browser, [], () => {
            return content.wrappedJSObject.runner.clear();
          });
          tipWrapper.clearFocusBlurNotifications();
        }
        tester.clear();
      }
    }
  );
});
