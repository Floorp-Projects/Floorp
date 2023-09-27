/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

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
      mockTransferCallback = function (downloadSuccess) {
        ok(downloadSuccess, "File should have been downloaded successfully");
        mockTransferCallback = () => {};
        resolve();
      };
    };
  });
}

let tempDir = createTemporarySaveDirectory();

add_setup(async function () {
  mockTransferRegisterer.register();
  registerCleanupFunction(function () {
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
    async function (browser) {
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
    async function (browser) {
      await waitForPdfJSAnnotationLayer(
        browser,
        TESTROOT + "file_pdfjs_form.pdf"
      );
      // Fill in the form input field.
      await SpecialPowers.spawn(browser, [], async function () {
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
    async function (browser) {
      await waitForPdfJSAnnotationLayer(browser, NetUtil.newURI(destFile).spec);
      await SpecialPowers.spawn(browser, [], async function () {
        let formInput = content.document.querySelector(
          "#viewerContainer input"
        );
        ok(formInput, "PDF contains text field.");
        is(formInput.value, "test", "Text field is filled in.");
      });
    }
  );
});

/**
 * Check triggering "Save Page As" on a PDF which was loaded with a custom
 * content disposition filename defaults to using the provided filename.
 */
add_task(async function test_pdf_saveas_customname() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.helperApps.showOpenOptionForPdfJS", true],
      ["browser.helperApps.showOpenOptionForViewableInternally", true],
      ["browser.download.always_ask_before_handling_new_types", false],
      ["browser.download.open_pdf_attachments_inline", true],
    ],
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: TESTROOT + "file_pdf_download_link.html" },
    async function (browser) {
      // Click on the download link in the loaded file. This will create a new
      // tab with the PDF loaded in it.
      BrowserTestUtils.synthesizeMouseAtCenter("#custom_filename", {}, browser);
      let tab = await BrowserTestUtils.waitForNewTab(gBrowser);
      info("tab created");

      // Wait for the PDF's metadata to be fully loaded before downloading, as
      // otherwise it won't be aware of the content disposition filename yet.
      await BrowserTestUtils.waitForContentEvent(
        tab.linkedBrowser,
        "metadataloaded",
        false,
        null,
        true
      );
      info("metadata loaded");

      let destFile = tempDir.clone();
      MockFilePicker.displayDirectory = tempDir;
      let fileSavedPromise = createPromiseForTransferComplete(
        "custom_filename.pdf",
        destFile
      );
      saveBrowser(tab.linkedBrowser);
      await fileSavedPromise;
      BrowserTestUtils.removeTab(tab);
    }
  );
  await SpecialPowers.popPrefEnv();
});

/**
 * Check if the directory where the pdfs are saved is based on the original
 * domain (see bug 1768383).
 */
add_task(async function () {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      const downloadLastDir = new DownloadLastDir(null);
      const destDirs = [];
      for (let i = 1; i <= 2; i++) {
        const destDir = createTemporarySaveDirectory(i);
        destDirs.push(destDir);
        const url = `http://test${i}.example.com/browser/${RELATIVE_DIR}file_pdfjs_test.pdf`;
        downloadLastDir.setFile(url, destDir);
        await TestUtils.waitForTick();
      }

      const url = `http://test1.example.com/browser/${RELATIVE_DIR}file_pdfjs_hcm.pdf`;
      await waitForPdfJS(browser, url);

      const fileSavedPromise = new Promise(resolve => {
        MockFilePicker.showCallback = fp => {
          MockFilePicker.setFiles([]);
          MockFilePicker.showCallback = null;
          resolve(fp.displayDirectory.path);
        };
      });
      registerCleanupFunction(() => {
        for (const destDir of destDirs) {
          destDir.remove(true);
        }
      });
      saveBrowser(browser);
      const dirPath = await fileSavedPromise;
      is(
        dirPath,
        destDirs[0].path,
        "Proposed directory must be based on the domain"
      );
    }
  );
});
