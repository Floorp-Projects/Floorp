/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = mockContentAnalysisService(mockCA);
});

const PAGE_URL =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/clipboard_paste_inputandtextarea.html";
const CLIPBOARD_TEXT_STRING = "Just some text";
async function testClipboardPaste(allowPaste) {
  mockCA.setupForTest(allowPaste);

  setClipboardData(CLIPBOARD_TEXT_STRING);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  let browser = tab.linkedBrowser;

  await SpecialPowers.spawn(browser, [allowPaste], async allowPaste => {
    content.document.getElementById("pasteAllowed").checked = allowPaste;
  });
  await testPasteWithElementId("testInput", browser, allowPaste);
  await testPasteWithElementId("testTextArea", browser, allowPaste);

  BrowserTestUtils.removeTab(tab);
}

async function testEmptyClipboardPaste() {
  mockCA.setupForTest(true);

  setClipboardData("");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  let browser = tab.linkedBrowser;

  let resultPromise = SpecialPowers.spawn(browser, [], () => {
    return new Promise(resolve => {
      content.document.addEventListener(
        "testresult",
        event => {
          resolve(event.detail.result);
        },
        { once: true }
      );
    });
  });

  // Paste into content
  await SpecialPowers.spawn(browser, [], async () => {
    content.document.getElementById("testInput").focus();
  });
  await BrowserTestUtils.synthesizeKey("v", { accelKey: true }, browser);
  let result = await resultPromise;
  is(result, undefined, "Got unexpected result from page");

  is(
    mockCA.calls.length,
    0,
    "Expect no calls to Content Analysis since it's an empty string"
  );
  let value = await getElementValue(browser, "testInput");
  is(value, "", "element has correct empty value");

  BrowserTestUtils.removeTab(tab);
}

function setClipboardData(clipboardString) {
  const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(null);
  trans.addDataFlavor("text/plain");
  const str = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString
  );
  str.data = clipboardString;
  trans.setTransferData("text/plain", str);

  // Write to clipboard.
  Services.clipboard.setData(trans, null, Ci.nsIClipboard.kGlobalClipboard);
}

async function testPasteWithElementId(elementId, browser, allowPaste) {
  let resultPromise = SpecialPowers.spawn(browser, [], () => {
    return new Promise(resolve => {
      content.document.addEventListener(
        "testresult",
        event => {
          resolve(event.detail.result);
        },
        { once: true }
      );
    });
  });

  // Paste into content
  await SpecialPowers.spawn(browser, [elementId], async elementId => {
    content.document.getElementById(elementId).focus();
  });
  await BrowserTestUtils.synthesizeKey("v", { accelKey: true }, browser);
  let result = await resultPromise;
  is(result, undefined, "Got unexpected result from page");

  // Because we call event.clipboardData.getData in the test, this causes another call to
  // content analysis.
  is(mockCA.calls.length, 2, "Correct number of calls to Content Analysis");
  assertContentAnalysisRequest(mockCA.calls[0], CLIPBOARD_TEXT_STRING);
  assertContentAnalysisRequest(mockCA.calls[1], CLIPBOARD_TEXT_STRING);
  mockCA.clearCalls();
  let value = await getElementValue(browser, elementId);
  is(
    value,
    allowPaste ? CLIPBOARD_TEXT_STRING : "",
    "element has correct value"
  );
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

async function getElementValue(browser, elementId) {
  return await SpecialPowers.spawn(browser, [elementId], async elementId => {
    return content.document.getElementById(elementId).value;
  });
}

add_task(async function testClipboardPasteWithContentAnalysisAllow() {
  await testClipboardPaste(true);
});

add_task(async function testClipboardPasteWithContentAnalysisBlock() {
  await testClipboardPaste(false);
});

add_task(async function testClipboardEmptyPasteWithContentAnalysis() {
  await testEmptyClipboardPaste();
});
