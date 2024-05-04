/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = mockContentAnalysisService(mockCA);
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.events.asyncClipboard.readText", true],
      // This pref turns off the "Paste" popup
      ["dom.events.testing.asyncClipboard", true],
    ],
  });
});

const PAGE_URL =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/clipboard_read_async.html";
const CLIPBOARD_TEXT_STRING = "Some plain text";
const CLIPBOARD_HTML_STRING = "<b>Some HTML</b>";
async function testClipboardReadAsync(allowPaste) {
  mockCA.setupForTest(allowPaste);

  setClipboardData();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  let browser = tab.linkedBrowser;
  {
    let result = await setDataAndStartTest(browser, allowPaste, "read");
    is(result, true, "Got unexpected result from page for read()");

    is(
      mockCA.calls.length,
      2,
      "Correct number of calls to Content Analysis for read()"
    );
    // On Windows, widget adds extra data into HTML clipboard.
    let expectedHtml = navigator.platform.includes("Win")
      ? `<html><body>\n<!--StartFragment-->${CLIPBOARD_HTML_STRING}<!--EndFragment-->\n</body>\n</html>`
      : CLIPBOARD_HTML_STRING;

    assertContentAnalysisRequest(mockCA.calls[0], expectedHtml);
    assertContentAnalysisRequest(mockCA.calls[1], CLIPBOARD_TEXT_STRING);
    mockCA.clearCalls();
  }

  {
    let result = await setDataAndStartTest(browser, allowPaste, "readText");
    is(result, true, "Got unexpected result from page for readText()");

    is(
      mockCA.calls.length,
      1,
      "Correct number of calls to Content Analysis for read()"
    );
    assertContentAnalysisRequest(mockCA.calls[0], CLIPBOARD_TEXT_STRING);
    mockCA.clearCalls();
  }

  BrowserTestUtils.removeTab(tab);
}

async function testClipboardReadAsyncWithErrorHelper() {
  mockCA.setupForTestWithError(Cr.NS_ERROR_NOT_AVAILABLE);

  setClipboardData();

  // This test throws a number of exceptions, so tell the framework this is OK.
  // If an exception is thrown we won't get the right response from setDataAndStartTest()
  // so this should be safe to do.
  ignoreAllUncaughtExceptions();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  let browser = tab.linkedBrowser;
  {
    let result = await setDataAndStartTest(browser, false, "read", true);
    is(result, true, "Got unexpected result from page for read()");

    is(
      mockCA.calls.length,
      2,
      "Correct number of calls to Content Analysis for read()"
    );
    // On Windows, widget adds extra data into HTML clipboard.
    let expectedHtml = navigator.platform.includes("Win")
      ? `<html><body>\n<!--StartFragment-->${CLIPBOARD_HTML_STRING}<!--EndFragment-->\n</body>\n</html>`
      : CLIPBOARD_HTML_STRING;

    assertContentAnalysisRequest(mockCA.calls[0], expectedHtml);
    assertContentAnalysisRequest(mockCA.calls[1], CLIPBOARD_TEXT_STRING);
    mockCA.clearCalls();
  }

  {
    let result = await setDataAndStartTest(browser, false, "readText", true);
    is(result, true, "Got unexpected result from page for readText()");

    is(
      mockCA.calls.length,
      1,
      "Correct number of calls to Content Analysis for read()"
    );
    assertContentAnalysisRequest(mockCA.calls[0], CLIPBOARD_TEXT_STRING);
    mockCA.clearCalls();
  }

  BrowserTestUtils.removeTab(tab);
}

function setDataAndStartTest(
  browser,
  allowPaste,
  testType,
  shouldError = false
) {
  return SpecialPowers.spawn(
    browser,
    [allowPaste, testType, shouldError],
    (allowPaste, testType, shouldError) => {
      return new Promise(resolve => {
        content.document.addEventListener(
          "testresult",
          event => {
            resolve(event.detail.result);
          },
          { once: true }
        );
        content.document.getElementById("pasteAllowed").checked = allowPaste;
        content.document.getElementById("contentAnalysisReturnsError").checked =
          shouldError;
        content.document.dispatchEvent(
          new content.CustomEvent("teststart", {
            detail: Cu.cloneInto({ testType }, content),
          })
        );
      });
    }
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

function setClipboardData() {
  const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(null);
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
}

add_task(async function testClipboardReadAsyncWithContentAnalysisAllow() {
  await testClipboardReadAsync(true);
});

add_task(async function testClipboardReadAsyncWithContentAnalysisBlock() {
  await testClipboardReadAsync(false);
});

add_task(async function testClipboardReadAsyncWithError() {
  await testClipboardReadAsyncWithErrorHelper();
});
