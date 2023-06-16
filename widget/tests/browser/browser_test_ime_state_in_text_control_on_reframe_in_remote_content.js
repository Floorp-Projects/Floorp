/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../file_ime_state_test_helper.js */
/* import-globals-from ../file_test_ime_state_in_text_control_on_reframe.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/widget/tests/browser/file_ime_state_test_helper.js",
  this
);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/widget/tests/browser/file_test_ime_state_in_text_control_on_reframe.js",
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

      await (async function test_ime_state_outside_contenteditable_on_readonly_change() {
        const tester = new IMEStateInTextControlOnReframeTester();
        await SpecialPowers.spawn(browser, [], () => {
          content.document.body.innerHTML = "<div contenteditable></div>";
          content.wrappedJSObject.runner =
            content.wrappedJSObject.createIMEStateInTextControlOnReframeTester();
        });
        for (
          let index = 0;
          index < IMEStateInTextControlOnReframeTester.numberOfTextControlTypes;
          index++
        ) {
          tipWrapper.clearFocusBlurNotifications();
          const expectedData1 = await SpecialPowers.spawn(
            browser,
            [index],
            aIndex => {
              return content.wrappedJSObject.runner.prepareToRun(
                aIndex,
                content.document,
                content.window
              );
            }
          );
          tipWrapper.typeA();
          await SpecialPowers.spawn(browser, [], () => {
            return new Promise(resolve =>
              content.window.requestAnimationFrame(() =>
                content.window.requestAnimationFrame(resolve)
              )
            );
          });
          tester.checkResultAfterTypingA(expectedData1, window, tipWrapper);

          const expectedData2 = await SpecialPowers.spawn(browser, [], () => {
            return content.wrappedJSObject.runner.prepareToRun2();
          });
          tipWrapper.typeA();
          await SpecialPowers.spawn(browser, [], () => {
            return new Promise(resolve =>
              content.window.requestAnimationFrame(() =>
                content.window.requestAnimationFrame(resolve)
              )
            );
          });
          tester.checkResultAfterTypingA2(expectedData2);
          tester.clear();
        }
      })();
    }
  );
});
