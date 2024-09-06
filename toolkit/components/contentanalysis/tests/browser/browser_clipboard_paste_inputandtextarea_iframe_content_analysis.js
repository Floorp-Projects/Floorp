/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = mockContentAnalysisService(mockCA);
});

const PAGE_URL_OUTER_SAME_ORIGIN =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/clipboard_paste_inputandtextarea_containing_frame.html";
const PAGE_URL_OUTER_DIFFERENT_ORIGIN =
  "https://example.org/browser/toolkit/components/contentanalysis/tests/browser/clipboard_paste_inputandtextarea_containing_frame.html";
const PAGE_URL_INNER =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/clipboard_paste_inputandtextarea.html";
const CLIPBOARD_TEXT_STRING = "Just some text";
async function testClipboardPaste(allowPaste, sameOrigin) {
  mockCA.setupForTest(allowPaste);

  setClipboardData(CLIPBOARD_TEXT_STRING);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    sameOrigin ? PAGE_URL_OUTER_SAME_ORIGIN : PAGE_URL_OUTER_DIFFERENT_ORIGIN
  );
  let browser = tab.linkedBrowser;

  await SpecialPowers.spawn(browser, [allowPaste], async allowPaste => {
    let frame = content.document.querySelector("iframe");
    await SpecialPowers.spawn(frame, [allowPaste], async allowPaste => {
      let doc = content.document;
      if (doc.readyState !== "complete") {
        await new Promise(r => {
          doc.addEventListener("DOMContentLoaded", () => {
            r();
          });
        });
      }
      let elem = doc.getElementById("pasteAllowed");
      elem.checked = allowPaste;
    });
  });
  await testPasteWithElementId("testInput", browser, allowPaste, sameOrigin);
  await testPasteWithElementId("testTextArea", browser, allowPaste, sameOrigin);

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

async function testPasteWithElementId(
  elementId,
  browser,
  allowPaste,
  sameOrigin
) {
  let resultPromise = SpecialPowers.spawn(browser, [], () => {
    let frame = content.document.querySelector("iframe");
    return SpecialPowers.spawn(frame, [], () => {
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
  });

  // Paste into content
  await SpecialPowers.spawn(browser, [elementId], async elementId => {
    let frame = content.document.querySelector("iframe");
    await SpecialPowers.spawn(frame, [elementId], elementId => {
      content.document.getElementById(elementId).focus();
    });
  });
  const iframeBC = browser.browsingContext.children[0];
  await BrowserTestUtils.synthesizeKey("v", { accelKey: true }, iframeBC);
  let result = await resultPromise;
  is(result, undefined, "Got unexpected result from page");

  // Because we call event.clipboardData.getData in the test, this causes another call to
  // content analysis.
  is(mockCA.calls.length, 2, "Correct number of calls to Content Analysis");
  assertContentAnalysisRequest(
    mockCA.calls[0],
    CLIPBOARD_TEXT_STRING,
    sameOrigin
  );
  assertContentAnalysisRequest(
    mockCA.calls[1],
    CLIPBOARD_TEXT_STRING,
    sameOrigin
  );
  mockCA.clearCalls();
  let value = await getElementValue(browser, elementId);
  is(
    value,
    allowPaste ? CLIPBOARD_TEXT_STRING : "",
    "element has correct value"
  );
}

function assertContentAnalysisRequest(request, expectedText, sameOrigin) {
  // If the outer page is same-origin to the iframe, the outer page URL should be passed to Content Analysis.
  // Otherwise the inner page URL should be passed.
  is(
    request.url.spec,
    sameOrigin ? PAGE_URL_OUTER_SAME_ORIGIN : PAGE_URL_INNER,
    "request has correct URL"
  );
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
    let frame = content.document.querySelector("iframe");
    return await SpecialPowers.spawn(frame, [elementId], elementId => {
      return content.document.getElementById(elementId).value;
    });
  });
}

add_task(async function testClipboardPasteSameOriginWithContentAnalysisAllow() {
  await testClipboardPaste(true, true);
});

add_task(async function testClipboardPasteSameOriginWithContentAnalysisBlock() {
  await testClipboardPaste(false, true);
});

add_task(
  async function testClipboardPasteDifferentOriginWithContentAnalysisAllow() {
    await testClipboardPaste(true, false);
  }
);

add_task(
  async function testClipboardPasteDifferentOriginWithContentAnalysisBlock() {
    await testClipboardPaste(false, false);
  }
);
