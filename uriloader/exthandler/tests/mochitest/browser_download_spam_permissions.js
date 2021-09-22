/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const TEST_URI = "https://example.com";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  TEST_URI
);

const AUTOMATIC_DOWNLOAD_TOPIC = "blocked-automatic-download";

add_task(async function setup() {
  // Create temp directory
  let time = new Date().getTime();
  let tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tempDir.append(time);
  Services.prefs.setComplexValue("browser.download.dir", Ci.nsIFile, tempDir);

  PermissionTestUtils.add(
    TEST_URI,
    "automatic-download",
    Services.perms.UNKNOWN
  );
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.improvements_to_download_panel", true]],
  });

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.download.dir");
    await IOUtils.remove(tempDir.path, { recursive: true });
  });
});

add_task(async function check_download_spam_permissions() {
  const INITIAL_TABS_COUNT = gBrowser.tabs.length;
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloadFinishedPromise = promiseDownloadFinished(
    publicList,
    true /* stop the download from openning */
  );
  let blockedDownloadsCount = 0;
  let blockedDownloadsURI = "";
  let automaticDownloadObserver = {
    observe: function automatic_download_observe(aSubject, aTopic, aData) {
      if (aTopic === AUTOMATIC_DOWNLOAD_TOPIC) {
        blockedDownloadsCount++;
        blockedDownloadsURI = aData;
      }
    },
  };
  Services.obs.addObserver(automaticDownloadObserver, AUTOMATIC_DOWNLOAD_TOPIC);

  let newTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "test_spammy_page.html"
  );
  registerCleanupFunction(async () => {
    await publicList.removeFinished();
    BrowserTestUtils.removeTab(newTab);
    Services.obs.removeObserver(
      automaticDownloadObserver,
      AUTOMATIC_DOWNLOAD_TOPIC
    );
  });

  let download = await downloadFinishedPromise;
  TestUtils.waitForCondition(
    () => gBrowser.tabs.length == INITIAL_TABS_COUNT + 1
  );
  is(
    PermissionTestUtils.testPermission(TEST_URI, "automatic-download"),
    Services.perms.PROMPT_ACTION,
    "The permission to prompt the user should be stored."
  );

  ok(
    await IOUtils.exists(download.target.path),
    "One file should be downloaded"
  );

  let aCopyFilePath = download.target.path.replace(".pdf", "(1).pdf");
  is(
    await IOUtils.exists(aCopyFilePath),
    false,
    "An other file should be blocked"
  );

  TestUtils.waitForCondition(() => blockedDownloadsCount >= 99);
  is(blockedDownloadsCount, 99, "Browser should block 99 downloads");
  is(
    blockedDownloadsURI,
    TEST_URI,
    "The test URI should have blocked automatic downloads"
  );
});
