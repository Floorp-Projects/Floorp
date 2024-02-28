/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let url =
  "data:text/html,<a id='link' href='http://localhost:8000/thefile.js'>Link</a>";

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

let httpServer = null;

add_task(async function test() {
  const { HttpServer } = ChromeUtils.importESModule(
    "resource://testing-common/httpd.sys.mjs"
  );

  httpServer = new HttpServer();
  httpServer.start(8000);

  function handleRequest(aRequest, aResponse) {
    aResponse.setStatusLine(aRequest.httpVersion, 200);
    aResponse.setHeader("Content-Type", "text/plain");
    aResponse.write("Some Text");
  }
  httpServer.registerPathHandler("/thefile.js", handleRequest);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  let popupShownPromise = BrowserTestUtils.waitForEvent(document, "popupshown");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#link",
    { type: "contextmenu", button: 2 },
    gBrowser.selectedBrowser
  );
  await popupShownPromise;

  let tempDir = createTemporarySaveDirectory();
  let destFile;

  MockFilePicker.displayDirectory = tempDir;
  MockFilePicker.showCallback = fp => {
    let fileName = fp.defaultString;
    destFile = tempDir.clone();
    destFile.append(fileName);

    MockFilePicker.setFiles([destFile]);
    MockFilePicker.filterIndex = 1; // kSaveAsType_URL
  };

  let transferCompletePromise = new Promise(resolve => {
    mockTransferCallback = resolve;
    mockTransferRegisterer.register();
  });

  registerCleanupFunction(function () {
    mockTransferRegisterer.unregister();
    tempDir.remove(true);
  });

  document.getElementById("context-savelink").doCommand();

  let contextMenu = document.getElementById("contentAreaContextMenu");
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    document,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await popupHiddenPromise;

  await transferCompletePromise;

  is(destFile.leafName, "thefile.js", "filename extension is not modified");

  BrowserTestUtils.removeTab(tab);
});

add_task(async () => {
  MockFilePicker.cleanup();
  await new Promise(resolve => httpServer.stop(resolve));
});

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

function createTemporarySaveDirectory() {
  let saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  return saveDir;
}
