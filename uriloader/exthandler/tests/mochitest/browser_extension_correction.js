/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

let gPathsToRemove = [];

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.useDownloadDir", true]],
  });
  registerCleanupFunction(async () => {
    for (let path of gPathsToRemove) {
      // IOUtils.remove ignores non-existing files out of the box.
      await IOUtils.remove(path);
    }
    let publicList = await Downloads.getList(Downloads.PUBLIC);
    await publicList.removeFinished();
  });
});

async function testLinkWithoutExtension(type, shouldHaveExtension) {
  info("Checking " + type);

  let task = function() {
    return SpecialPowers.spawn(gBrowser.selectedBrowser, [type], mimetype => {
      let link = content.document.createElement("a");
      link.textContent = "Click me";
      link.href = "data:" + mimetype + ",hello";
      link.download = "somefile";
      content.document.body.appendChild(link);
      link.click();
    });
  };

  await checkDownloadWithExtensionState(task, {
    type,
    shouldHaveExtension,
    alwaysViewPDFInline: false,
  });

  if (type == "application/pdf") {
    // For PDF, try again with the always open inline preference set
    await SpecialPowers.pushPrefEnv({
      set: [["browser.download.open_pdf_attachments_inline", true]],
    });

    await checkDownloadWithExtensionState(task, {
      type,
      shouldHaveExtension,
      alwaysViewPDFInline: true,
    });

    await SpecialPowers.popPrefEnv();
  }
}

async function checkDownloadWithExtensionState(
  task,
  { type, shouldHaveExtension, expectedName = null, alwaysViewPDFInline }
) {
  const shouldExpectDialog = Services.prefs.getBoolPref(
    "browser.download.always_ask_before_handling_new_types",
    false
  );

  let winPromise;
  if (shouldExpectDialog) {
    winPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
  }

  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let shouldCheckFilename = shouldHaveExtension || !shouldExpectDialog;

  let downloadFinishedPromise = shouldCheckFilename
    ? promiseDownloadFinished(publicList)
    : null;

  // PDF should load using the internal viewer without downloading it.
  let waitForLoad;
  if (
    (!shouldExpectDialog || alwaysViewPDFInline) &&
    type == "application/pdf"
  ) {
    waitForLoad = BrowserTestUtils.waitForNewTab(gBrowser);
  }

  await task();
  await waitForLoad;

  let win;
  if (shouldExpectDialog) {
    info("Waiting for dialog.");
    win = await winPromise;
  }

  expectedName ??= shouldHaveExtension
    ? "somefile." + getMIMEInfoForType(type).primaryExtension
    : "somefile";

  let closedPromise = true;
  if (shouldExpectDialog) {
    let actualName = win.document.getElementById("location").value;
    closedPromise = BrowserTestUtils.windowClosed(win);

    if (shouldHaveExtension) {
      is(actualName, expectedName, `${type} should get an extension`);
    } else {
      is(actualName, expectedName, `${type} should not get an extension`);
    }
  }

  if (shouldExpectDialog && shouldHaveExtension) {
    // Then pick "save" in the dialog, if we have a dialog.
    let dialog = win.document.getElementById("unknownContentType");
    win.document.getElementById("save").click();
    let button = dialog.getButton("accept");
    button.disabled = false;
    dialog.acceptDialog();
  }

  if (!shouldExpectDialog && type == "application/pdf") {
    if (alwaysViewPDFInline) {
      is(
        gURLBar.inputField.value,
        "data:application/pdf,hello",
        "url is correct for " + type
      );
    } else {
      ok(
        gURLBar.inputField.value.startsWith("file://") &&
          gURLBar.inputField.value.endsWith("somefile.pdf"),
        "url is correct for " + type
      );
    }

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }

  if (shouldExpectDialog || !alwaysViewPDFInline || type != "application/pdf") {
    // Wait for the download if it exists (may produce null).
    let download = await downloadFinishedPromise;
    if (download) {
      // Check the download's extension is correct.
      is(
        PathUtils.filename(download.target.path),
        expectedName,
        `Downloaded file should match ${expectedName}`
      );
      gPathsToRemove.push(download.target.path);
      let pathToRemove = download.target.path;
      // Avoid one file interfering with subsequent files.
      await publicList.removeFinished();
      await IOUtils.remove(pathToRemove);
    } else if (win) {
      // We just cancel out for files that would end up without a path, as we'd
      // prompt for a filename.
      win.close();
    }
  }

  return closedPromise;
}

/**
 * Check that for document types, images, videos and audio files,
 * we enforce a useful extension.
 */
add_task(async function test_enforce_useful_extension() {
  await BrowserTestUtils.withNewTab("data:text/html,", async browser => {
    await testLinkWithoutExtension("image/png", true);
    await testLinkWithoutExtension("audio/ogg", true);
    await testLinkWithoutExtension("video/webm", true);
    await testLinkWithoutExtension("application/pdf", true);

    await testLinkWithoutExtension("application/x-gobbledygook", false);
    await testLinkWithoutExtension("application/octet-stream", false);
    await testLinkWithoutExtension("binary/octet-stream", false);
    await testLinkWithoutExtension("application/x-msdownload", false);
  });
});

/**
 * Check that we still use URL extension info when we don't have anything else,
 * despite bogus local info.
 */
add_task(async function test_broken_saved_handlerinfo_and_useless_mimetypes() {
  let bogusType = getMIMEInfoForType("binary/octet-stream");
  registerCleanupFunction(() => {
    handlerSvc.remove(bogusType);
  });
  bogusType.setFileExtensions(["jpg"]);
  let handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
    Ci.nsIHandlerService
  );
  handlerSvc.store(bogusType);
  let tabToClean = null;
  let task = function() {
    return BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: TEST_PATH + "file_as.exe?foo=bar",
      waitForLoad: false,
      waitForStateStop: true,
    }).then(tab => {
      return (tabToClean = tab);
    });
  };
  await checkDownloadWithExtensionState(task, {
    type: "binary/octet-stream",
    shouldHaveExtension: true,
    expectedName: "file_as.exe",
  });
  // Downloads should really close their tabs...
  if (tabToClean?.isConnected) {
    BrowserTestUtils.removeTab(tabToClean);
  }
});
