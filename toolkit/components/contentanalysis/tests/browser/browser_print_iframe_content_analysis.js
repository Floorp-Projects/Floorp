/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/printing/tests/head.js",
  this
);

const PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
  Ci.nsIPrintSettingsService
);

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = mockContentAnalysisService(mockCA);
});

const TEST_PAGE_URL =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/clipboard_print_iframe.html";
const TEST_PAGE_URL_INNER =
  "https://example.com/browser/toolkit/components/printing/tests/simplifyArticleSample.html";

function addUniqueSuffix(prefix) {
  return `${prefix}-${Services.uuid
    .generateUUID()
    .toString()
    .slice(1, -1)}.pdf`;
}

async function printToDestination(aBrowsingContext, aDestination) {
  let tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  let fileName = addUniqueSuffix(`printDestinationTest-${aDestination}`);
  let filePath = PathUtils.join(tmpDir.path, fileName);

  info(`Printing to ${filePath}`);

  let settings = PSSVC.createNewPrintSettings();
  settings.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;
  settings.outputDestination = aDestination;

  settings.headerStrCenter = "";
  settings.headerStrLeft = "";
  settings.headerStrRight = "";
  settings.footerStrCenter = "";
  settings.footerStrLeft = "";
  settings.footerStrRight = "";

  settings.unwriteableMarginTop = 1; /* Just to ensure settings are respected on both */
  let outStream = null;
  if (aDestination == Ci.nsIPrintSettings.kOutputDestinationFile) {
    settings.toFileName = PathUtils.join(tmpDir.path, fileName);
  } else {
    is(aDestination, Ci.nsIPrintSettings.kOutputDestinationStream);
    outStream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
      Ci.nsIFileOutputStream
    );
    let tmpFile = tmpDir.clone();
    tmpFile.append(fileName);
    outStream.init(tmpFile, -1, 0o666, 0);
    settings.outputStream = outStream;
  }

  await aBrowsingContext.print(settings);

  return filePath;
}

function assertContentAnalysisRequest(request, expectedUrl) {
  is(
    request.url.spec,
    expectedUrl ?? TEST_PAGE_URL,
    "request has correct outer URL"
  );
  is(
    request.analysisType,
    Ci.nsIContentAnalysisRequest.ePrint,
    "request has print analysisType"
  );
  is(
    request.operationTypeForDisplay,
    Ci.nsIContentAnalysisRequest.eOperationPrint,
    "request has print operationTypeForDisplay"
  );
  is(request.textContent, "", "request textContent should be empty");
  is(request.filePath, "", "request filePath should be empty");
  isnot(request.printDataHandle, 0, "request printDataHandle should not be 0");
  isnot(request.printDataSize, 0, "request printDataSize should not be 0");
  ok(!!request.requestToken.length, "request requestToken should not be empty");
}

// Printing to a stream is different than going through the print preview dialog because it
// doesn't make a static clone of the document before the print, which causes the
// Content Analysis code to go through a different code path. This is similar to what
// happens when various preferences are set to skip the print preview dialog, for example
// print.prefer_system_dialog.
add_task(
  async function testPrintIframeToStreamWithContentAnalysisActiveAndAllowing() {
    await PrintHelper.withTestPage(
      async helper => {
        mockCA.setupForTest(true);

        await SpecialPowers.spawn(helper.sourceBrowser, [], async () => {
          let innerDoc =
            content.document.querySelector("iframe").contentDocument;
          if (innerDoc.readyState !== "complete") {
            await new Promise(r => {
              innerDoc.addEventListener("DOMContentLoaded", () => {
                r();
              });
            });
          }
        });
        let frameBrowsingContext =
          helper.sourceBrowser.browsingContext.children[0];

        let filePath = await printToDestination(
          frameBrowsingContext,
          Ci.nsIPrintSettings.kOutputDestinationFile
        );

        is(
          mockCA.calls.length,
          1,
          "Correct number of calls to Content Analysis"
        );
        assertContentAnalysisRequest(mockCA.calls[0]);

        await waitForFileToAlmostMatchSize(
          filePath,
          mockCA.calls[0].printDataSize
        );

        await IOUtils.remove(filePath);
      },
      TEST_PAGE_URL,
      true
    );
  }
);
