/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.useDownloadDir", true]],
  });
  registerCleanupFunction(async () => {
    let publicList = await Downloads.getList(Downloads.PUBLIC);
    let downloads = await publicList.getAll();
    for (let download of downloads) {
      if (download.target.exists) {
        try {
          info("removing " + download.target.path);
          await IOUtils.remove(download.target.path);
        } catch (ex) {
          /* ignore */
        }
      }
    }
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
  await checkDownloadWithExtensionState(task, { type, shouldHaveExtension });
}

async function checkDownloadWithExtensionState(
  task,
  { type, shouldHaveExtension, expectedName = null }
) {
  let winPromise = BrowserTestUtils.domWindowOpenedAndLoaded();

  await task();

  info("Waiting for dialog.");
  let win = await winPromise;

  let actualName = win.document.getElementById("location").value;
  if (shouldHaveExtension) {
    expectedName ??= "somefile." + getMIMEInfoForType(type).primaryExtension;
    is(actualName, expectedName, `${type} should get an extension`);
  } else {
    expectedName ??= "somefile";
    is(actualName, expectedName, `${type} should not get an extension`);
  }

  let closedPromise = BrowserTestUtils.windowClosed(win);

  if (shouldHaveExtension) {
    // Wait for the download.
    let publicList = await Downloads.getList(Downloads.PUBLIC);
    let downloadFinishedPromise = promiseDownloadFinished(publicList);

    // Then pick "save" in the dialog.
    let dialog = win.document.getElementById("unknownContentType");
    win.document.getElementById("save").click();
    let button = dialog.getButton("accept");
    button.disabled = false;
    dialog.acceptDialog();

    // Wait for the download to finish and check the extension is correct.
    let download = await downloadFinishedPromise;
    is(
      PathUtils.filename(download.target.path),
      expectedName,
      `Downloaded file should also match ${expectedName}`
    );
    await IOUtils.remove(download.target.path);
  } else {
    // We just cancel out for files that would end up without a path, as we'd
    // prompt for a filename.
    win.close();
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
    await testLinkWithoutExtension("application/msword", true);
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
  bogusType.setFileExtensions(["jpg"]);
  let handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
    Ci.nsIHandlerService
  );
  handlerSvc.store(bogusType);
  let tabToClean = null;
  let task = function() {
    return BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_PATH + "file_as.exe?foo=bar"
    ).then(tab => {
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
