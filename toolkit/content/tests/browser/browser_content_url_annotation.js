/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* global Services, requestLongerTimeout, TestUtils, BrowserTestUtils,
 ok, info, dump, is, Ci, Cu, Components, ctypes, privateNoteIntentionalCrash,
 gBrowser, add_task, addEventListener, removeEventListener, ContentTask */

"use strict";

// Running this test in ASAN is slow.
requestLongerTimeout(2);

/**
 * Removes a file from a directory. This is a no-op if the file does not
 * exist.
 *
 * @param directory
 *        The nsIFile representing the directory to remove from.
 * @param filename
 *        A string for the file to remove from the directory.
 */
function removeFile(directory, filename) {
  let file = directory.clone();
  file.append(filename);
  if (file.exists()) {
    file.remove(false);
  }
}

/**
 * Returns the directory where crash dumps are stored.
 *
 * @return nsIFile
 */
function getMinidumpDirectory() {
  let dir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  dir.append("minidumps");
  return dir;
}

/**
 * Checks that the URL is correctly annotated on a content process crash.
 */
add_task(async function test_content_url_annotation() {
  let url =
    "https://example.com/browser/toolkit/content/tests/browser/file_redirect.html";
  let redirect_url =
    "https://example.com/browser/toolkit/content/tests/browser/file_redirect_to.html";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
    },
    async function(browser) {
      ok(browser.isRemoteBrowser, "Should be a remote browser");

      // file_redirect.html should send us to file_redirect_to.html
      let promise = BrowserTestUtils.waitForContentEvent(
        browser,
        "RedirectDone",
        true,
        null,
        true
      );
      BrowserTestUtils.loadURI(browser, url);
      await promise;

      // Crash the tab
      let annotations = await BrowserTestUtils.crashFrame(browser);

      ok("URL" in annotations, "annotated a URL");
      is(
        annotations.URL,
        redirect_url,
        "Should have annotated the URL after redirect"
      );
    }
  );
});
