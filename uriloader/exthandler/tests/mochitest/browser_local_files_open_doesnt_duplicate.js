/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const mimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
const handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);
let mimeInfo = mimeSvc.getFromTypeAndExtension("application/pdf", "pdf");

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({ set: [["pdfjs.disabled", true]] });

  let oldAsk = mimeInfo.alwaysAskBeforeHandling;
  let oldPreferredAction = mimeInfo.preferredAction;
  let oldPreferredApp = mimeInfo.preferredApplicationHandler;
  registerCleanupFunction(() => {
    mimeInfo.preferredApplicationHandler = oldPreferredApp;
    mimeInfo.preferredAction = oldPreferredAction;
    mimeInfo.alwaysAskBeforeHandling = oldAsk;
    handlerSvc.store(mimeInfo);
  });

  if (!mimeInfo.preferredApplicationHandler) {
    let handlerApp = Cc[
      "@mozilla.org/uriloader/local-handler-app;1"
    ].createInstance(Ci.nsILocalHandlerApp);
    handlerApp.executable = Services.dirsvc.get("TmpD", Ci.nsIFile);
    handlerApp.executable.append("foopydoo.exe");
    mimeInfo.possibleApplicationHandlers.appendElement(handlerApp);
    mimeInfo.preferredApplicationHandler = handlerApp;
  }
});

add_task(async function open_from_dialog() {
  // Force PDFs to prompt:
  mimeInfo.preferredAction = mimeInfo.useHelperApp;
  mimeInfo.alwaysAskBeforeHandling = true;
  handlerSvc.store(mimeInfo);

  let openingPromise = TestUtils.topicObserved(
    "test-only-opening-downloaded-file",
    (subject, data) => {
      subject.QueryInterface(Ci.nsISupportsPRBool);
      // Block opening the file:
      subject.data = false;
      return true;
    }
  );

  let dialogWindowPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
  let openedFile = getChromeDir(getResolvedURI(gTestPath));
  openedFile.append("file_pdf_binary_octet_stream.pdf");
  let expectedPath = openedFile.isSymlink()
    ? openedFile.target
    : openedFile.path;
  let loadingTab = BrowserTestUtils.addTab(gBrowser, expectedPath);

  let dialogWindow = await dialogWindowPromise;
  is(
    dialogWindow.location.href,
    "chrome://mozapps/content/downloads/unknownContentType.xhtml",
    "Should have seen the unknown content dialogWindow."
  );

  let doc = dialogWindow.document;

  // Select the 'open' entry.
  doc.querySelector("#open").click();
  let dialog = doc.querySelector("#unknownContentType");
  dialog.getButton("accept").removeAttribute("disabled");
  dialog.acceptDialog();
  let [, openedPath] = await openingPromise;
  is(
    openedPath,
    expectedPath,
    "Should have opened file directly (not created a copy)."
  );
  if (openedPath != expectedPath) {
    await IOUtils.setPermissions(openedPath, 0o666);
    await IOUtils.remove(openedPath);
  }
  BrowserTestUtils.removeTab(loadingTab);
});

add_task(async function open_directly() {
  // Force PDFs to open immediately:
  mimeInfo.preferredAction = mimeInfo.useHelperApp;
  mimeInfo.alwaysAskBeforeHandling = false;
  handlerSvc.store(mimeInfo);

  let openingPromise = TestUtils.topicObserved(
    "test-only-opening-downloaded-file",
    (subject, data) => {
      subject.QueryInterface(Ci.nsISupportsPRBool);
      // Block opening the file:
      subject.data = false;
      return true;
    }
  );

  let openedFile = getChromeDir(getResolvedURI(gTestPath));
  openedFile.append("file_pdf_binary_octet_stream.pdf");
  let expectedPath = openedFile.isSymlink()
    ? openedFile.target
    : openedFile.path;
  let loadingTab = BrowserTestUtils.addTab(gBrowser, expectedPath);

  let [, openedPath] = await openingPromise;
  is(
    openedPath,
    expectedPath,
    "Should have opened file directly (not created a copy)."
  );
  if (openedPath != expectedPath) {
    await IOUtils.setPermissions(openedPath, 0o666);
    await IOUtils.remove(openedPath);
  }
  BrowserTestUtils.removeTab(loadingTab);
});
