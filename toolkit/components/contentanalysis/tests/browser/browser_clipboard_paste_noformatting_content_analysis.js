/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = mockContentAnalysisService(mockCA);
});

const PAGE_URL =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/clipboard_paste_noformatting.html";
async function testClipboardPasteNoFormatting(allowPaste) {
  mockCA.setupForTest(allowPaste);

  const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(null);
  const CLIPBOARD_TEXT_STRING = "Some text";
  const CLIPBOARD_HTML_STRING = "<b>Some HTML</b>";
  {
    trans.addDataFlavor("text/plain");
    const str = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    str.data = CLIPBOARD_TEXT_STRING;
    trans.setTransferData("text/plain", str);
  }
  {
    trans.addDataFlavor("text/html");
    const str = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    str.data = CLIPBOARD_HTML_STRING;
    trans.setTransferData("text/html", str);
  }

  // Write to clipboard.
  Services.clipboard.setData(trans, null, Ci.nsIClipboard.kGlobalClipboard);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  let browser = tab.linkedBrowser;
  let result = await SpecialPowers.spawn(browser, [allowPaste], allowPaste => {
    return new Promise(resolve => {
      content.document.addEventListener("testresult", event => {
        resolve(event.detail.result);
      });
      content.document.getElementById("pasteAllowed").checked = allowPaste;
      content.document.dispatchEvent(new content.CustomEvent("teststart", {}));
    });
  });
  is(result, true, "Got unexpected result from page");

  // Because we call event.clipboardData.getData in the test, this causes another call to
  // content analysis.
  is(mockCA.calls.length, 2, "Correct number of calls to Content Analysis");
  assertContentAnalysisRequest(mockCA.calls[0], CLIPBOARD_TEXT_STRING);
  assertContentAnalysisRequest(mockCA.calls[1], CLIPBOARD_TEXT_STRING);

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
  is(request.filePath, "", "request filePath should match");
  is(request.textContent, expectedText, "request textContent should match");
  is(request.printDataHandle, 0, "request printDataHandle should not be 0");
  is(request.printDataSize, 0, "request printDataSize should not be 0");
  ok(!!request.requestToken.length, "request requestToken should not be empty");
}

add_task(
  async function testClipboardPasteNoFormattingWithContentAnalysisAllow() {
    await testClipboardPasteNoFormatting(true);
  }
);

add_task(
  async function testClipboardPasteNoFormattingWithContentAnalysisBlock() {
    await testClipboardPasteNoFormatting(false);
  }
);
