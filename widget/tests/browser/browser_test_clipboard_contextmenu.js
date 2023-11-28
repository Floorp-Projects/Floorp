/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const clipboard = SpecialPowers.Services.clipboard;
const clipboardTypes = [
  clipboard.kGlobalClipboard,
  clipboard.kSelectionClipboard,
  clipboard.kFindClipboard,
  clipboard.kSelectionCache,
];
const supportedClipboardTypes = clipboardTypes.filter(type =>
  clipboard.isClipboardTypeSupported(type)
);

const kPasteMenuPopupId = "clipboardReadPasteMenuPopup";
const kPasteMenuItemId = "clipboardReadPasteMenuItem";

function waitForPasteMenuPopupEvent(aEventSuffix) {
  // The element with id `kPasteMenuPopupId` is inserted dynamically, hence
  // calling `BrowserTestUtils.waitForEvent` instead of
  // `BrowserTestUtils.waitForPopupEvent`.
  return BrowserTestUtils.waitForEvent(
    document,
    "popup" + aEventSuffix,
    false /* capture */,
    e => {
      return e.target.getAttribute("id") == kPasteMenuPopupId;
    }
  );
}

async function waitForPasteContextMenu() {
  await waitForPasteMenuPopupEvent("shown");
  const pasteButton = document.getElementById(kPasteMenuItemId);
  info("Wait for paste button enabled");
  await BrowserTestUtils.waitForMutationCondition(
    pasteButton,
    { attributeFilter: ["disabled"] },
    () => !pasteButton.disabled,
    "Wait for paste button enabled"
  );
}

function promiseClickPasteButton() {
  info("Wait for clicking paste contextmenu");
  const pasteButton = document.getElementById(kPasteMenuItemId);
  let promise = BrowserTestUtils.waitForEvent(pasteButton, "click");
  EventUtils.synthesizeMouseAtCenter(pasteButton, {});
  return promise;
}

async function clipboardAsyncGetData(aBrowser, aClipboardType) {
  await SpecialPowers.spawn(aBrowser, [aClipboardType], async type => {
    return new Promise((resolve, reject) => {
      SpecialPowers.Services.clipboard.asyncGetData(
        ["text/plain"],
        type,
        content.browsingContext.currentWindowContext,
        content.document.nodePrincipal,
        {
          QueryInterface: SpecialPowers.ChromeUtils.generateQI([
            "nsIAsyncClipboardGetCallback",
          ]),
          // nsIAsyncClipboardGetCallback
          onSuccess: aAsyncGetClipboardData => {
            resolve();
          },
          onError: aResult => {
            reject(aResult);
          },
        }
      );
    });
  });
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Avoid paste button delay enabling making test too long.
      ["security.dialog_enable_delay", 0],
    ],
  });
});

supportedClipboardTypes.forEach(type => {
  add_task(async function test_clipboard_contextmenu() {
    await BrowserTestUtils.withNewTab(
      "https://example.com",
      async function (browser) {
        info(`Test clipboard contextmenu for clipboard type ${type}`);
        let pasteContextMenuPromise = waitForPasteContextMenu();
        const asyncRead = clipboardAsyncGetData(browser, type);
        await pasteContextMenuPromise;

        // We don't allow requests for different clipboard type to be
        // consolidated, so when the context menu is shown for a specific
        // clipboard type and has not yet got user response, any new requests
        // for a different type should be rejected.
        info(
          "Test other clipboard types before interact with paste contextmenu"
        );
        for (let otherType of supportedClipboardTypes) {
          if (type == otherType) {
            continue;
          }

          info(`Test other clipboard type ${otherType}`);
          await Assert.rejects(
            clipboardAsyncGetData(browser, otherType),
            ex => ex == Cr.NS_ERROR_DOM_NOT_ALLOWED_ERR,
            `Request for clipboard type ${otherType} should be rejected`
          );
        }

        await promiseClickPasteButton();
        try {
          await asyncRead;
          ok(true, `nsIClipboard.asyncGetData() should success`);
        } catch (e) {
          ok(false, `nsIClipboard.asyncGetData() should not fail with ${e}`);
        }
      }
    );
  });
});
