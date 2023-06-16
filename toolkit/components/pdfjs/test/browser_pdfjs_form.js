/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

async function checkFormState(formsEnabled) {
  ok(
    content.document.querySelector("div#viewer"),
    "document content has viewer UI"
  );

  let formInput = content.document.querySelector("#viewerContainer input");

  if (formsEnabled) {
    ok(formInput, "Form input available");
  } else {
    ok(!formInput, "Form input not available");
  }
  let viewer = content.wrappedJSObject.PDFViewerApplication;
  await viewer.close();
}

add_task(async function test_defaults() {
  // Ensure the default preference is set to the expected value.
  let defaultBranch = Services.prefs.getDefaultBranch("pdfjs.");
  let prefType = defaultBranch.getPrefType("annotationMode");
  let renderForms = Services.prefs.getIntPref("pdfjs.annotationMode") === 2;

  is(
    prefType,
    Ci.nsIPrefBranch.PREF_INT,
    "The form pref is defined by default"
  );

  ok(renderForms, "Forms are enabled");

  // Test that the forms state matches the pref.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      await waitForPdfJSAnnotationLayer(
        browser,
        TESTROOT + "file_pdfjs_form.pdf"
      );

      await SpecialPowers.spawn(browser, [true], checkFormState);
    }
  );
});

// Test disabling forms with pref.
add_task(async function test_disabling() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      // First, make sure they are enabled.
      await SpecialPowers.pushPrefEnv({
        set: [["pdfjs.annotationMode", 2]],
      });
      await waitForPdfJSAnnotationLayer(
        browser,
        TESTROOT + "file_pdfjs_form.pdf"
      );
      await SpecialPowers.spawn(
        browser,
        [true /* formsEnabled */],
        checkFormState
      );
      // Now disable the forms.
      await SpecialPowers.pushPrefEnv({
        set: [["pdfjs.annotationMode", 1]],
      });
      await waitForPdfJSAnnotationLayer(
        browser,
        TESTROOT + "file_pdfjs_form.pdf"
      );
      await SpecialPowers.spawn(
        browser,
        [false /* formsEnabled */],
        checkFormState
      );
    }
  );
});
