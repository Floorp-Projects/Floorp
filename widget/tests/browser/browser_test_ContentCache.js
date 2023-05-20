/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  const TIP = Cc["@mozilla.org/text-input-processor;1"].createInstance(
    Ci.nsITextInputProcessor
  );
  let notifications = [];
  const observer = (aTIP, aNotification) => {
    switch (aNotification.type) {
      case "request-to-commit":
        aTIP.commitComposition();
        break;
      case "request-to-cancel":
        aTIP.cancelComposition();
        break;
      case "notify-end-input-transaction":
      case "notify-focus":
      case "notify-blur":
      case "notify-text-change":
      case "notify-selection-change":
        notifications.push(aNotification);
        break;
    }
    return true;
  };

  function checkNotifications(aExpectedNotifications, aDescription) {
    for (const expectedNotification of aExpectedNotifications) {
      const notification = notifications.find(
        element => element.type == expectedNotification.type
      );
      if (expectedNotification.expected) {
        isnot(
          notification,
          undefined,
          `"${expectedNotification.type}" should be notified ${aDescription}`
        );
      } else {
        is(
          notification,
          undefined,
          `"${expectedNotification.type}" should not be notified ${aDescription}`
        );
      }
    }
  }

  ok(
    TIP.beginInputTransaction(window, observer),
    "nsITextInputProcessor.beingInputTransaction should return true"
  );
  ok(
    TIP.beginInputTransactionForTests(window, observer),
    "nsITextInputProcessor.beginInputTransactionForTests should return true"
  );

  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
    async function (browser) {
      ok(browser.isRemoteBrowser, "This test passes only in e10s mode");

      // IMEContentObserver flushes pending IME notifications at next vsync
      // after something happens.  Therefore, after doing something in content
      // process, we need to guarantee that IMEContentObserver has a change to
      // send IME notifications to the main process with calling this function.
      function waitForSendingIMENotificationsInContent() {
        return SpecialPowers.spawn(browser, [], async () => {
          await new Promise(resolve =>
            content.requestAnimationFrame(() =>
              content.requestAnimationFrame(resolve)
            )
          );
        });
      }

      /**
       * Test when empty editor gets focus
       */
      notifications = [];
      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.innerHTML = "<div contenteditable><br></div>";
        const editor = content.document.querySelector("div[contenteditable]");
        editor.focus();
      });

      await waitForSendingIMENotificationsInContent();

      (function () {
        checkNotifications(
          [
            { type: "notify-focus", expected: true },
            { type: "notify-blur", expected: false },
            { type: "notify-end-input-transaction", expected: false },
            { type: "notify-text-change", expected: false },
            { type: "notify-selection-change", expected: false },
          ],
          "after empty editor gets focus"
        );
        const text = EventUtils.synthesizeQueryTextContent(0, 1000);
        ok(
          text?.succeeded,
          "query text content should succeed after empty editor gets focus"
        );
        if (text?.succeeded) {
          is(
            text.text.replace(/[\r\n]/g, ""),
            "",
            "text should be only line breaks after empty editor gets focus"
          );
        }
        const selection = EventUtils.synthesizeQuerySelectedText();
        ok(
          selection?.succeeded,
          "query selected text should succeed after empty editor gets focus"
        );
        if (selection?.succeeded) {
          ok(
            !selection.notFound,
            "query selected text should find a selection range after empty editor gets focus"
          );
          if (!selection.notFound) {
            is(
              selection.text,
              "",
              "selection should be collapsed after empty editor gets focus"
            );
          }
        }
      })();

      /**
       * Test when there is non-collapsed selection
       */
      notifications = [];
      await SpecialPowers.spawn(browser, [], () => {
        const editor = content.document.querySelector("div[contenteditable]");
        editor.innerHTML = "<p>abc</p><p>def</p>";
        content
          .getSelection()
          .setBaseAndExtent(
            editor.querySelector("p").firstChild,
            2,
            editor.querySelector("p + p").firstChild,
            1
          );
      });

      await waitForSendingIMENotificationsInContent();

      (function () {
        checkNotifications(
          [
            { type: "notify-focus", expected: false },
            { type: "notify-blur", expected: false },
            { type: "notify-end-input-transaction", expected: false },
            { type: "notify-text-change", expected: true },
            { type: "notify-selection-change", expected: true },
          ],
          "after modifying focused editor"
        );
        const text = EventUtils.synthesizeQueryTextContent(0, 1000);
        ok(
          text?.succeeded,
          "query text content should succeed after modifying focused editor"
        );
        if (text?.succeeded) {
          is(
            text.text.trim().replace(/\r\n/g, "\n").replace(/\n\n+/g, "\n"),
            "abc\ndef",
            "text should include the both paragraph's text after modifying focused editor"
          );
        }
        const selection = EventUtils.synthesizeQuerySelectedText();
        ok(
          selection?.succeeded,
          "query selected text should succeed after modifying focused editor"
        );
        if (selection?.succeeded) {
          ok(
            !selection.notFound,
            "query selected text should find a selection range after modifying focused editor"
          );
          if (!selection.notFound) {
            is(
              selection.text
                .trim()
                .replace(/\r\n/g, "\n")
                .replace(/\n\n+/g, "\n"),
              "c\nd",
              "selection should have the selected characters in the both paragraphs after modifying focused editor"
            );
          }
        }
      })();

      /**
       * Test when there is no selection ranges
       */
      notifications = [];
      await SpecialPowers.spawn(browser, [], () => {
        content.getSelection().removeAllRanges();
      });

      await waitForSendingIMENotificationsInContent();

      (function () {
        checkNotifications(
          [
            { type: "notify-focus", expected: false },
            { type: "notify-blur", expected: false },
            { type: "notify-end-input-transaction", expected: false },
            { type: "notify-text-change", expected: false },
            { type: "notify-selection-change", expected: true },
          ],
          "after removing all selection ranges from the focused editor"
        );
        const text = EventUtils.synthesizeQueryTextContent(0, 1000);
        ok(
          text?.succeeded,
          "query text content should succeed after removing all selection ranges from the focused editor"
        );
        if (text?.succeeded) {
          is(
            text.text.trim().replace(/\r\n/g, "\n").replace(/\n\n+/g, "\n"),
            "abc\ndef",
            "text should include the both paragraph's text after removing all selection ranges from the focused editor"
          );
        }
        const selection = EventUtils.synthesizeQuerySelectedText();
        ok(
          selection?.succeeded,
          "query selected text should succeed after removing all selection ranges from the focused editor"
        );
        if (selection?.succeeded) {
          ok(
            selection.notFound,
            "query selected text should find no selection range after removing all selection ranges from the focused editor"
          );
        }
      })();

      /**
       * Test when no editable element has focus.
       */
      notifications = [];
      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.innerHTML = "abcdef";
      });

      await waitForSendingIMENotificationsInContent();

      (function () {
        checkNotifications(
          [
            { type: "notify-focus", expected: false },
            { type: "notify-blur", expected: true },
          ],
          "removing editor should make ContentCacheInParent not have any data"
        );
        const text = EventUtils.synthesizeQueryTextContent(0, 1000);
        ok(
          !text?.succeeded,
          "query text content should fail because no editable element has focus"
        );
        const selection = EventUtils.synthesizeQuerySelectedText();
        ok(
          !selection?.succeeded,
          "query selected text should fail because no editable element has focus"
        );
        const caret = EventUtils.synthesizeQueryCaretRect(0);
        ok(
          !caret?.succeeded,
          "query caret rect should fail because no editable element has focus"
        );
        const textRect = EventUtils.synthesizeQueryTextRect(0, 5, false);
        ok(
          !textRect?.succeeded,
          "query text rect should fail because no editable element has focus"
        );
        const textRectArray = EventUtils.synthesizeQueryTextRectArray(0, 5);
        ok(
          !textRectArray?.succeeded,
          "query text rect array should fail because no editable element has focus"
        );
        const editorRect = EventUtils.synthesizeQueryEditorRect();
        todo(
          !editorRect?.succeeded,
          "query editor rect should fail because no editable element has focus"
        );
      })();
    }
  );
});
