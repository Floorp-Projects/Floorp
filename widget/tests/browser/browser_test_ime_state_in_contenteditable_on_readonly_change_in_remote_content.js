/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../file_ime_state_test_helper.js */
/* import-globals-from ../file_test_ime_state_in_contenteditable_on_readonly_change.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/widget/tests/browser/file_ime_state_test_helper.js",
  this
);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/widget/tests/browser/file_test_ime_state_in_contenteditable_on_readonly_change.js",
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

      await (async function test_ime_state_in_contenteditable_on_readonly_change() {
        const expectedDataOfInitialization = await SpecialPowers.spawn(
          browser,
          [],
          () => {
            content.document.body.innerHTML = "<div contenteditable><br></div>";
            content.wrappedJSObject.runner =
              content.wrappedJSObject.createIMEStateInContentEditableOnReadonlyChangeTester();
            const editingHost = content.document.querySelector(
              "div[contenteditable]"
            );
            return content.wrappedJSObject.runner.prepareToRun(
              editingHost,
              editingHost,
              content.window
            );
          }
        );
        const tester = new IMEStateInContentEditableOnReadonlyChangeTester();
        tester.checkResultOfPreparation(
          expectedDataOfInitialization,
          window,
          tipWrapper
        );
        const expectedDataOfReadonly = await SpecialPowers.spawn(
          browser,
          [],
          () => {
            return content.wrappedJSObject.runner.runToMakeHTMLEditorReadonly();
          }
        );
        tester.checkResultOfMakingHTMLEditorReadonly(expectedDataOfReadonly);
        const expectedDataOfEditable = await SpecialPowers.spawn(
          browser,
          [],
          () => {
            return content.wrappedJSObject.runner.runToMakeHTMLEditorEditable();
          }
        );
        tester.checkResultOfMakingHTMLEditorEditable(expectedDataOfEditable);
        const expectedDataOfFinalization = await SpecialPowers.spawn(
          browser,
          [],
          () => {
            return content.wrappedJSObject.runner.runToRemoveContentEditableAttribute();
          }
        );
        tester.checkResultOfRemovingContentEditableAttribute(
          expectedDataOfFinalization
        );
        tester.clear();
      })();

      await (async function test_ime_state_in_button_in_contenteditable_on_readonly_change() {
        const expectedDataOfInitialization = await SpecialPowers.spawn(
          browser,
          [],
          () => {
            content.document.body.innerHTML =
              "<div contenteditable><br><button>button</button></div>";
            content.wrappedJSObject.runner =
              content.wrappedJSObject.createIMEStateInContentEditableOnReadonlyChangeTester();
            const editingHost = content.document.querySelector(
              "div[contenteditable]"
            );
            return content.wrappedJSObject.runner.prepareToRun(
              editingHost,
              editingHost.querySelector("button"),
              content.window
            );
          }
        );
        const tester = new IMEStateInContentEditableOnReadonlyChangeTester();
        tester.checkResultOfPreparation(
          expectedDataOfInitialization,
          window,
          tipWrapper
        );
        const expectedDataOfReadonly = await SpecialPowers.spawn(
          browser,
          [],
          () => {
            return content.wrappedJSObject.runner.runToMakeHTMLEditorReadonly();
          }
        );
        tester.checkResultOfMakingHTMLEditorReadonly(expectedDataOfReadonly);
        const expectedDataOfEditable = await SpecialPowers.spawn(
          browser,
          [],
          () => {
            return content.wrappedJSObject.runner.runToMakeHTMLEditorEditable();
          }
        );
        tester.checkResultOfMakingHTMLEditorEditable(expectedDataOfEditable);
        const expectedDataOfFinalization = await SpecialPowers.spawn(
          browser,
          [],
          () => {
            return content.wrappedJSObject.runner.runToRemoveContentEditableAttribute();
          }
        );
        tester.checkResultOfRemovingContentEditableAttribute(
          expectedDataOfFinalization
        );
        tester.clear();
      })();

      await (async function test_ime_state_of_text_controls_in_contenteditable_on_readonly_change() {
        const tester =
          new IMEStateOfTextControlInContentEditableOnReadonlyChangeTester();
        await SpecialPowers.spawn(browser, [], () => {
          content.document.body.innerHTML = "<div contenteditable></div>";
          content.wrappedJSObject.runner =
            content.wrappedJSObject.createIMEStateOfTextControlInContentEditableOnReadonlyChangeTester();
        });
        for (
          let index = 0;
          index <
          IMEStateOfTextControlInContentEditableOnReadonlyChangeTester.numberOfTextControlTypes;
          index++
        ) {
          const expectedDataOfInitialization = await SpecialPowers.spawn(
            browser,
            [index],
            aIndex => {
              const editingHost = content.document.querySelector("div");
              return content.wrappedJSObject.runner.prepareToRun(
                aIndex,
                editingHost,
                content.window
              );
            }
          );
          tester.checkResultOfPreparation(
            expectedDataOfInitialization,
            window,
            tipWrapper
          );
          const expectedDataOfMakingParentEditingHost =
            await SpecialPowers.spawn(browser, [], () => {
              return content.wrappedJSObject.runner.runToMakeParentEditingHost();
            });
          tester.checkResultOfMakingParentEditingHost(
            expectedDataOfMakingParentEditingHost
          );
          const expectedDataOfMakingHTMLEditorReadonly =
            await SpecialPowers.spawn(browser, [], () => {
              return content.wrappedJSObject.runner.runToMakeHTMLEditorReadonly();
            });
          tester.checkResultOfMakingHTMLEditorReadonly(
            expectedDataOfMakingHTMLEditorReadonly
          );
          const expectedDataOfMakingHTMLEditorEditable =
            await SpecialPowers.spawn(browser, [], () => {
              return content.wrappedJSObject.runner.runToMakeHTMLEditorEditable();
            });
          tester.checkResultOfMakingHTMLEditorEditable(
            expectedDataOfMakingHTMLEditorEditable
          );
          const expectedDataOfMakingParentNonEditable =
            await SpecialPowers.spawn(browser, [], () => {
              return content.wrappedJSObject.runner.runToMakeParentNonEditingHost();
            });
          tester.checkResultOfMakingParentNonEditable(
            expectedDataOfMakingParentNonEditable
          );
          tester.clear();
        }
      })();

      await (async function test_ime_state_outside_contenteditable_on_readonly_change() {
        const tester =
          new IMEStateOutsideContentEditableOnReadonlyChangeTester();
        await SpecialPowers.spawn(browser, [], () => {
          content.document.body.innerHTML = "<div contenteditable></div>";
          content.wrappedJSObject.runner =
            content.wrappedJSObject.createIMEStateOutsideContentEditableOnReadonlyChangeTester();
        });
        for (
          let index = 0;
          index <
          IMEStateOutsideContentEditableOnReadonlyChangeTester.numberOfFocusTargets;
          index++
        ) {
          const expectedDataOfInitialization = await SpecialPowers.spawn(
            browser,
            [index],
            aIndex => {
              const editingHost = content.document.querySelector("div");
              return content.wrappedJSObject.runner.prepareToRun(
                aIndex,
                editingHost,
                content.window
              );
            }
          );
          tester.checkResultOfPreparation(
            expectedDataOfInitialization,
            window,
            tipWrapper
          );
          const expectedDataOfMakingParentEditingHost =
            await SpecialPowers.spawn(browser, [], () => {
              return content.wrappedJSObject.runner.runToMakeParentEditingHost();
            });
          tester.checkResultOfMakingParentEditingHost(
            expectedDataOfMakingParentEditingHost
          );
          const expectedDataOfMakingHTMLEditorReadonly =
            await SpecialPowers.spawn(browser, [], () => {
              return content.wrappedJSObject.runner.runToMakeHTMLEditorReadonly();
            });
          tester.checkResultOfMakingHTMLEditorReadonly(
            expectedDataOfMakingHTMLEditorReadonly
          );
          const expectedDataOfMakingHTMLEditorEditable =
            await SpecialPowers.spawn(browser, [], () => {
              return content.wrappedJSObject.runner.runToMakeHTMLEditorEditable();
            });
          tester.checkResultOfMakingHTMLEditorEditable(
            expectedDataOfMakingHTMLEditorEditable
          );
          const expectedDataOfMakingParentNonEditable =
            await SpecialPowers.spawn(browser, [], () => {
              return content.wrappedJSObject.runner.runToMakeParentNonEditingHost();
            });
          tester.checkResultOfMakingParentNonEditable(
            expectedDataOfMakingParentNonEditable
          );
          tester.clear();
        }
      })();
    }
  );
});
