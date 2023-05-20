/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../file_ime_state_test_helper.js */
/* import-globals-from ../file_test_ime_state_on_readonly_change.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/widget/tests/browser/file_ime_state_test_helper.js",
  this
);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/widget/tests/browser/file_test_ime_state_on_readonly_change.js",
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

      const tester = new IMEStateOnReadonlyChangeTester();
      for (
        let i = 0;
        i < IMEStateOnReadonlyChangeTester.numberOfTextControlTypes;
        i++
      ) {
        const expectedResultBefore = await SpecialPowers.spawn(
          browser,
          [i],
          aIndex => {
            content.wrappedJSObject.runner =
              content.wrappedJSObject.createIMEStateOnReadonlyChangeTester(
                aIndex
              );
            return content.wrappedJSObject.runner.prepareToRun(
              aIndex,
              content.window,
              content.document.body
            );
          }
        );
        tester.checkBeforeRun(expectedResultBefore, tipWrapper);
        const expectedResultOfMakingTextControlReadonly =
          await SpecialPowers.spawn(browser, [], () => {
            return content.wrappedJSObject.runner.runToMakeTextControlReadonly();
          });
        tester.checkResultOfMakingTextControlReadonly(
          expectedResultOfMakingTextControlReadonly
        );
        const expectedResultOfMakingTextControlEditable =
          await SpecialPowers.spawn(browser, [], () => {
            return content.wrappedJSObject.runner.runToMakeTextControlEditable();
          });
        tester.checkResultOfMakingTextControlEditable(
          expectedResultOfMakingTextControlEditable
        );
        tipWrapper.clearFocusBlurNotifications();
        tester.clear();
      }
    }
  );
});
