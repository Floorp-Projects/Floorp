/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that (real) files can be pasted into chrome/content.
// Pasting files should also hide all other data from content.

function setClipboard(path) {
  const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(path);

  const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(null);
  trans.addDataFlavor("application/x-moz-file");
  trans.setTransferData("application/x-moz-file", file);

  trans.addDataFlavor("text/plain");
  const str = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString
  );
  str.data = "Alternate";
  trans.setTransferData("text/plain", str);

  // Write to clipboard.
  Services.clipboard.setData(trans, null, Ci.nsIClipboard.kGlobalClipboard);
}

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = mockContentAnalysisService(mockCA);
});

const PAGE_URL =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/clipboard_paste_file.html";

function assertContentAnalysisRequest(
  request,
  expectedDisplayType,
  expectedFilePath,
  expectedText
) {
  is(request.url.spec, PAGE_URL, "request has correct URL");
  is(
    request.analysisType,
    Ci.nsIContentAnalysisRequest.eBulkDataEntry,
    "request has correct analysisType"
  );
  is(
    request.operationTypeForDisplay,
    expectedDisplayType,
    "request has correct operationTypeForDisplay"
  );
  is(request.filePath, expectedFilePath, "request filePath should match");
  is(request.textContent, expectedText, "request textContent should match");
  is(request.printDataHandle, 0, "request printDataHandle should not be 0");
  is(request.printDataSize, 0, "request printDataSize should not be 0");
  ok(!!request.requestToken.length, "request requestToken should not be empty");
}
function assertContentAnalysisRequestFile(request, expectedFilePath) {
  assertContentAnalysisRequest(
    request,
    Ci.nsIContentAnalysisRequest.eCustomDisplayString,
    expectedFilePath,
    ""
  );
}
function assertContentAnalysisRequestText(request, expectedText) {
  assertContentAnalysisRequest(
    request,
    Ci.nsIContentAnalysisRequest.eClipboard,
    "",
    expectedText
  );
}

async function testClipboardPasteFileWithContentAnalysis(allowPaste) {
  mockCA.setupForTest(allowPaste);
  await SpecialPowers.pushPrefEnv({
    set: [["dom.events.dataTransfer.mozFile.enabled", true]],
  });

  // Create a temporary file that will be pasted.
  const file = await IOUtils.createUniqueFile(
    PathUtils.tempDir,
    "test-file.txt",
    0o600
  );
  const FILE_TEXT = "Hello World!";
  await IOUtils.writeUTF8(file, FILE_TEXT);

  // Put the data directly onto the native clipboard to make sure
  // it isn't handled internally in Gecko somehow.
  setClipboard(file);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  let browser = tab.linkedBrowser;

  let resultPromise = SpecialPowers.spawn(browser, [allowPaste], allowPaste => {
    return new Promise(resolve => {
      content.document.addEventListener("testresult", event => {
        resolve(event.detail.result);
      });
      content.document.getElementById("pasteAllowed").checked = allowPaste;
    });
  });

  // Focus <input> in content
  await SpecialPowers.spawn(browser, [], async function () {
    content.document.getElementById("input").focus();
  });

  // Paste file into <input> in content
  await BrowserTestUtils.synthesizeKey("v", { accelKey: true }, browser);

  let result = await resultPromise;
  is(
    result,
    allowPaste ? PathUtils.filename(file) : "",
    "Correctly pasted file in content"
  );
  is(mockCA.calls.length, 2, "Correct number of calls to Content Analysis");
  assertContentAnalysisRequestFile(mockCA.calls[0], file);
  assertContentAnalysisRequestText(mockCA.calls[1], "Alternate");
  mockCA.clearCalls();

  // The following part of the test is done in-process (note the use of document here instead of
  // content.document) so none of this should go through content analysis.
  var input = document.createElement("input");
  document.documentElement.appendChild(input);
  input.focus();

  await new Promise((resolve, _reject) => {
    input.addEventListener(
      "paste",
      function (event) {
        let dt = event.clipboardData;
        is(dt.types.length, 3, "number of types");
        ok(dt.types.includes("text/plain"), "text/plain exists in types");
        ok(
          dt.types.includes("application/x-moz-file"),
          "application/x-moz-file exists in types"
        );
        is(dt.types[2], "Files", "Last type should be 'Files'");
        ok(
          dt.mozTypesAt(0).contains("text/plain"),
          "text/plain exists in mozTypesAt"
        );
        is(
          dt.getData("text/plain"),
          "Alternate",
          "text/plain returned in getData"
        );
        is(
          dt.mozGetDataAt("text/plain", 0),
          "Alternate",
          "text/plain returned in mozGetDataAt"
        );

        ok(
          dt.mozTypesAt(0).contains("application/x-moz-file"),
          "application/x-moz-file exists in mozTypesAt"
        );
        let mozFile = dt.mozGetDataAt("application/x-moz-file", 0);

        ok(
          mozFile instanceof Ci.nsIFile,
          "application/x-moz-file returned nsIFile with mozGetDataAt"
        );

        is(
          mozFile.leafName,
          PathUtils.filename(file),
          "nsIFile has correct leafName"
        );

        is(mozFile.fileSize, FILE_TEXT.length, "nsIFile has correct length");

        resolve();
      },
      { capture: true, once: true }
    );

    EventUtils.synthesizeKey("v", { accelKey: true });
  });
  is(mockCA.calls.length, 0, "Correct number of calls to Content Analysis");

  input.remove();

  BrowserTestUtils.removeTab(tab);

  await IOUtils.remove(file);
}

add_task(async function testClipboardPasteFileWithContentAnalysisAllow() {
  await testClipboardPasteFileWithContentAnalysis(true);
});

add_task(async function testClipboardPasteFileWithContentAnalysisBlock() {
  await testClipboardPasteFileWithContentAnalysis(false);
});
