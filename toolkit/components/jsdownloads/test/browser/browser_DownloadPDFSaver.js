/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the PDF download saver, and tests using a window as a
 * source for the copy download saver.
 */

"use strict";

/**
 * Helper function to make sure a window reference exists on the download source.
 */
function test_download_windowRef(aTab, aDownload) {
  ok(aDownload.source.windowRef, "Download source had a window reference");
  ok(aDownload.source.windowRef instanceof Ci.xpcIJSWeakReference, "Download window reference is a weak ref");
  is(aDownload.source.windowRef.get(), aTab.linkedBrowser.contentWindow, "Download window exists during test");
}

/**
 * Helper function to check the state of a completed download.
 */
function test_download_state_complete(aTab, aDownload, aPrivate, aCanceled) {
  ok(aDownload.source, "Download has a source");
  is(aDownload.source.url, aTab.linkedBrowser.contentWindow.location, "Download source has correct url");
  is(aDownload.source.isPrivate, aPrivate, "Download source has correct private state");
  ok(aDownload.stopped, "Download is stopped");
  is(aCanceled, aDownload.canceled, "Download has correct canceled state");
  is(!aCanceled, aDownload.succeeded, "Download has correct succeeded state");
  is(aDownload.error, null, "Download error is not defined");
}

async function test_createDownload_common(aPrivate, aType) {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: aPrivate});

  let tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, getRootDirectory(gTestPath) + "testFile.html");
  let download = await Downloads.createDownload({
    source: tab.linkedBrowser.contentWindow,
    target: { path: getTempFile(TEST_TARGET_FILE_NAME_PDF).path },
    saver: { type: aType }
  });

  await test_download_windowRef(tab, download);
  await download.start();

  await test_download_state_complete(tab, download, aPrivate, false);
  if (aType == "pdf") {
    let signature = await OS.File.read(download.target.path,
                                       { bytes: 4, encoding: "us-ascii" });
    is(signature, "%PDF", "File exists and signature matches");
  } else {
    ok((await OS.File.exists(download.target.path)), "File exists");
  }

  win.gBrowser.removeTab(tab);
  win.close()
}

add_task(async function test_createDownload_pdf_private() {
  await test_createDownload_common(true, "pdf");
});
add_task(async function test_createDownload_pdf_not_private() {
  await test_createDownload_common(false, "pdf");
});

// Even for the copy saver, using a window should produce valid results
add_task(async function test_createDownload_copy_private() {
  await test_createDownload_common(true, "copy");
});
add_task(async function test_createDownload_copy_not_private() {
  await test_createDownload_common(false, "copy");
});

add_task(async function test_cancel_pdf_download() {
  let tab = BrowserTestUtils.addTab(gBrowser, getRootDirectory(gTestPath) + "testFile.html");
  await promiseBrowserLoaded(tab.linkedBrowser);

  let download = await Downloads.createDownload({
    source: tab.linkedBrowser.contentWindow,
    target: { path: getTempFile(TEST_TARGET_FILE_NAME_PDF).path },
    saver: "pdf",
  });

  await test_download_windowRef(tab, download);
  download.start().catch(() => {});

  // Immediately cancel the download to test that it is erased correctly.
  await download.cancel();
  await test_download_state_complete(tab, download, false, true);

  let exists = await OS.File.exists(download.target.path)
  ok(!exists, "Target file does not exist");

  gBrowser.removeTab(tab);
});
