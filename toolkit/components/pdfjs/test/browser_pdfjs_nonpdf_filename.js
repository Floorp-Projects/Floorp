/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TESTROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);

// The page we use to open the PDF.
const LINK_PAGE_URL = TESTROOT + "file_pdf_download_link.html";

add_task(async function test_filename_nonpdf_extension() {
  var MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);
  let filepickerNamePromise = new Promise(resolve => {
    MockFilePicker.showCallback = function(fp) {
      resolve(fp.defaultString);
      return MockFilePicker.returnCancel;
    };
  });
  registerCleanupFunction(() => MockFilePicker.cleanup());

  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.open_pdf_attachments_inline", true]],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: LINK_PAGE_URL },
    async function(browser) {
      await SpecialPowers.spawn(browser, [], async () => {
        let link = content.document.getElementById("custom_filename");
        let response = await content.fetch(link.href, {
          method: "GET",
          headers: {
            "Content-Type": "application/pdf",
          },
        });
        let blob = await response.blob();
        const url = content.URL.createObjectURL(blob);
        link.href = url;
        link.download = "Fido-2022-04-28";
        link.rel = "noopener";
      });

      let pdfLoaded = BrowserTestUtils.waitForNewTab(
        gBrowser,
        url => url.startsWith("blob:"),
        true
      );
      await BrowserTestUtils.synthesizeMouseAtCenter(
        "#custom_filename",
        {},
        browser
      );
      let newTab = await pdfLoaded;

      info("Clicking on the download button...");
      await SpecialPowers.spawn(newTab.linkedBrowser, [], () => {
        content.document.getElementById("download").click();
      });
      info("Waiting for a filename to be picked from the file picker");
      let defaultName = await filepickerNamePromise;
      is(
        defaultName,
        "Fido-2022-04-28.pdf",
        "Should have gotten the provided filename with pdf suffixed."
      );
      BrowserTestUtils.removeTab(newTab);
    }
  );
});
