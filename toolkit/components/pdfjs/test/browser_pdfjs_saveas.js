/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

/* import-globals-from ../../../content/tests/browser/common/mockTransfer.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

function createTemporarySaveDirectory() {
  var saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  return saveDir;
}

function createPromiseForTransferComplete(expectedFileName, destFile) {
  return new Promise(resolve => {
    MockFilePicker.showCallback = fp => {
      info("Filepicker shown, checking filename");
      is(fp.defaultString, expectedFileName, "Filename should be correct.");
      let fileName = fp.defaultString;
      destFile.append(fileName);

      MockFilePicker.setFiles([destFile]);
      MockFilePicker.filterIndex = 0; // kSaveAsType_Complete

      MockFilePicker.showCallback = null;
      mockTransferCallback = function(downloadSuccess) {
        ok(downloadSuccess, "File should have been downloaded successfully");
        mockTransferCallback = () => {};
        resolve();
      };
    };
  });
}

let tempDir = createTemporarySaveDirectory();

add_task(async function setup() {
  mockTransferRegisterer.register();
  registerCleanupFunction(function() {
    mockTransferRegisterer.unregister();
    MockFilePicker.cleanup();
    tempDir.remove(true);
  });
});

/**
 * Check triggering "Save Page As" on a non-forms PDF opens the "Save As" dialog
 * and successfully saves the file.
 */
add_task(async function test_pdf_saveas() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      await waitForPdfJS(browser, TESTROOT + "file_pdfjs_test.pdf");
      let destFile = tempDir.clone();
      MockFilePicker.displayDirectory = tempDir;
      let fileSavedPromise = createPromiseForTransferComplete(
        "file_pdfjs_test.pdf",
        destFile
      );
      saveBrowser(browser);
      await fileSavedPromise;
    }
  );
});

/**
 * Check triggering "Save Page As" on a PDF with forms that has been modified
 * does the following:
 * 1) opens the "Save As" dialog
 * 2) successfully saves the file
 * 3) the new file contains the new form data
 */
add_task(async function test_pdf_saveas_forms() {
  await SpecialPowers.pushPrefEnv({
    set: [["pdfjs.renderInteractiveForms", true]],
  });
  let destFile = tempDir.clone();
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      await waitForPdfJSAnnotationLayer(
        browser,
        TESTROOT + "file_pdfjs_form.pdf"
      );
      // Fill in the form input field.
      await SpecialPowers.spawn(browser, [], async function() {
        let formInput = content.document.querySelector(
          "#viewerContainer input"
        );
        ok(formInput, "PDF contains text field.");
        is(formInput.value, "", "Text field is empty to start.");
        formInput.value = "test";
        formInput.dispatchEvent(new content.window.Event("input"));
      });

      MockFilePicker.displayDirectory = tempDir;
      let fileSavedPromise = createPromiseForTransferComplete(
        "file_pdfjs_form.pdf",
        destFile
      );
      saveBrowser(browser);
      await fileSavedPromise;
    }
  );

  // Now that the file has been modified and saved, load it to verify the form
  // data persisted.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      await waitForPdfJSAnnotationLayer(browser, NetUtil.newURI(destFile).spec);
      await SpecialPowers.spawn(browser, [], async function() {
        let formInput = content.document.querySelector(
          "#viewerContainer input"
        );
        ok(formInput, "PDF contains text field.");
        is(formInput.value, "test", "Text field is filled in.");
      });
    }
  );
});
