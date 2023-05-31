/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { FileTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/FileTestUtils.sys.mjs"
);

const ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const TEST_FILE = ROOT + "download.bin";

add_task(async function test_check_download_dir() {
  // Force XDG dir to somewhere that has no config files, causing lookups of the
  // system download dir to fail:
  let newXDGRoot = FileTestUtils.getTempFile("xdgstuff");
  newXDGRoot.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  let oldXDG = Services.env.exists("XDG_CONFIG_HOME")
    ? Services.env.get("XDG_CONFIG_HOME")
    : "";
  registerCleanupFunction(() => Services.env.set("XDG_CONFIG_HOME", oldXDG));
  Services.env.set("XDG_CONFIG_HOME", newXDGRoot.path + "/");

  let propBundle = Services.strings.createBundle(
    "chrome://mozapps/locale/downloads/downloads.properties"
  );
  let dlRoot = PathUtils.join(
    Services.dirsvc.get("Home", Ci.nsIFile).path,
    propBundle.GetStringFromName("downloadsFolder")
  );

  // Check lookups fail:
  Assert.throws(
    () => Services.dirsvc.get("DfltDwnld", Ci.nsIFile),
    /NS_ERROR_FAILURE/,
    "Should throw when asking for downloads dir."
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      // Avoid opening dialogs
      ["browser.download.always_ask_before_handling_new_types", false],
      // Switch back to default OS downloads dir (changed in head.js):
      ["browser.download.folderList", 1],
    ],
  });

  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloadFinished = promiseDownloadFinished(publicList);
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_FILE);
  let dl = await downloadFinished;
  ok(dl.succeeded, "Download should succeed.");
  Assert.stringContains(
    dl.target.path,
    dlRoot,
    "Should store download under DL folder root."
  );
  let dlKids = await IOUtils.getChildren(dlRoot);
  ok(
    dlKids.includes(dl.target.path),
    "Download should be a direct child of the DL folder."
  );
  await IOUtils.remove(dl.target.path);

  BrowserTestUtils.removeTab(tab);

  // Download a second file to make sure we're not continuously adding filenames
  // onto the download folder path.
  downloadFinished = promiseDownloadFinished(publicList);
  tab = BrowserTestUtils.addTab(gBrowser, TEST_FILE);
  dl = await downloadFinished;
  Assert.stringContains(
    dl.target.path,
    dlRoot,
    "Second download should store download under new DL folder root."
  );
  dlKids = await IOUtils.getChildren(dlRoot);
  ok(
    dlKids.includes(dl.target.path),
    "Second download should be a direct child of the new DL folder."
  );
  BrowserTestUtils.removeTab(tab);
  await IOUtils.remove(dl.target.path);

  await publicList.removeFinished();
  await IOUtils.remove(newXDGRoot.path);
});
