/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../file_ime_state_test_helper.js */
/* import-globals-from ../file_test_ime_state_on_focus_move.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/widget/tests/browser/file_ime_state_test_helper.js",
  this
);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/widget/tests/browser/file_test_ime_state_on_focus_move.js",
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

      // isnot is used in file_test_ime_state_on_focus_move.js, but it's not
      // defined as the alias of Assert.notEqual in browser-chrome tests.
      // Therefore, we need to define it here.
      // eslint-disable-next-line no-unused-vars
      const isnot = Assert.notEqual;

      async function runIMEStateOnFocusMoveTests(aDescription) {
        await (async function test_IMEState_without_focused_element() {
          const checker = new IMEStateWhenNoActiveElementTester(aDescription);
          const expectedData = await SpecialPowers.spawn(
            browser,
            [aDescription],
            description => {
              const runner =
                content.wrappedJSObject.createIMEStateWhenNoActiveElementTester(
                  description
                );
              return runner.run(content.document, content.window);
            }
          );
          checker.check(expectedData);
        })();
        for (
          let index = 0;
          index < IMEStateOnFocusMoveTester.numberOfTests;
          ++index
        ) {
          const checker = new IMEStateOnFocusMoveTester(aDescription, index);
          const expectedData = await SpecialPowers.spawn(
            browser,
            [aDescription, index],
            (description, aIndex) => {
              content.wrappedJSObject.runner =
                content.wrappedJSObject.createIMEStateOnFocusMoveTester(
                  description,
                  aIndex,
                  content.window
                );
              return content.wrappedJSObject.runner.prepareToRun(
                content.document.querySelector("div")
              );
            }
          );
          checker.prepareToCheck(expectedData, tipWrapper);
          await SpecialPowers.spawn(browser, [], () => {
            return content.wrappedJSObject.runner.run();
          });
          checker.check(expectedData);

          if (checker.canTestOpenCloseState(expectedData)) {
            for (const defaultOpenState of [false, true]) {
              const expectedOpenStateData = await SpecialPowers.spawn(
                browser,
                [],
                () => {
                  return content.wrappedJSObject.runner.prepareToRunOpenCloseTest(
                    content.document.querySelector("div")
                  );
                }
              );
              checker.prepareToCheckOpenCloseTest(
                defaultOpenState,
                expectedOpenStateData
              );
              await SpecialPowers.spawn(browser, [], () => {
                return content.wrappedJSObject.runner.runOpenCloseTest();
              });
              checker.checkOpenCloseTest(expectedOpenStateData);
            }
          }
          await SpecialPowers.spawn(browser, [], () => {
            content.wrappedJSObject.runner.destroy();
            content.wrappedJSObject.runner = undefined;
          });
          checker.destroy();
        } // for loop iterating test of IMEStateOnFocusMoveTester
      } // definition of runIMEStateOnFocusMoveTests

      // test designMode
      await SpecialPowers.spawn(browser, [], async () => {
        content.document.designMode = "on";
      });
      await runIMEStateOnFocusMoveTests('in designMode="on"');
      await SpecialPowers.spawn(browser, [], async () => {
        content.document.designMode = "off";
      });
      await runIMEStateOnFocusMoveTests('in designMode="off"');
    }
  );
});
