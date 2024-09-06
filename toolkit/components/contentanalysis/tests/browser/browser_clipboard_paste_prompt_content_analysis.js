/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = mockContentAnalysisService(mockCA);
});

// Using an external page so the test can checks that the URL matches in the nsIContentAnalysisRequest
const PAGE_URL =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/clipboard_paste_prompt.html";
const CLIPBOARD_TEXT_STRING = "Just some text";
async function testClipboardPaste(allowPaste) {
  mockCA.setupForTest(allowPaste);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  let browser = tab.linkedBrowser;

  let promptPromise = SpecialPowers.spawn(browser, [], async () => {
    return content.prompt();
  });

  let prompt = await PromptTestUtils.waitForPrompt(browser, {
    modalType: Services.prompt.MODAL_TYPE_CONTENT,
  });
  // Paste text into prompt() in content
  let pastePromise = new Promise(resolve => {
    prompt.ui.loginTextbox.addEventListener(
      "paste",
      () => {
        // Since mockCA uses setTimeout before invoking the callback,
        // do it here too
        setTimeout(() => {
          resolve();
        }, 0);
      },
      { once: true }
    );
  });
  let ev = new ClipboardEvent("paste", {
    dataType: "text/plain",
    data: CLIPBOARD_TEXT_STRING,
  });
  prompt.ui.loginTextbox.dispatchEvent(ev);
  await pastePromise;

  // Close the prompt
  await PromptTestUtils.handlePrompt(prompt);

  let result = await promptPromise;
  is(
    result,
    allowPaste ? CLIPBOARD_TEXT_STRING : "",
    "prompt has correct value"
  );
  is(mockCA.calls.length, 1, "Correct number of calls to Content Analysis");
  assertContentAnalysisRequest(mockCA.calls[0], CLIPBOARD_TEXT_STRING);
  is(
    mockCA.browsingContextsForURIs.length,
    1,
    "Correct number of calls to getURIForBrowsingContext()"
  );

  BrowserTestUtils.removeTab(tab);
}

function assertContentAnalysisRequest(request, expectedText) {
  is(request.url.spec, PAGE_URL, "request has correct URL");
  is(
    request.analysisType,
    Ci.nsIContentAnalysisRequest.eBulkDataEntry,
    "request has correct analysisType"
  );
  is(
    request.operationTypeForDisplay,
    Ci.nsIContentAnalysisRequest.eClipboard,
    "request has correct operationTypeForDisplay"
  );
  is(request.filePath, null, "request filePath should match");
  is(request.textContent, expectedText, "request textContent should match");
  is(request.printDataHandle, 0, "request printDataHandle should not be 0");
  is(request.printDataSize, 0, "request printDataSize should not be 0");
  ok(!!request.requestToken.length, "request requestToken should not be empty");
}

add_task(async function testClipboardPasteWithContentAnalysisAllow() {
  await testClipboardPaste(true);
});

add_task(async function testClipboardPasteWithContentAnalysisBlock() {
  await testClipboardPaste(false);
});
