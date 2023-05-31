/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_HOST = "example.org";
const TEST_FILE = "\u3002.bin";
const TEST_URL = `http://${TEST_HOST}/${TEST_FILE}`;

const { XPCShellContentUtils } = ChromeUtils.importESModule(
  "resource://testing-common/XPCShellContentUtils.sys.mjs"
);
XPCShellContentUtils.initMochitest(this);
const server = XPCShellContentUtils.createHttpServer({
  hosts: [TEST_HOST],
});
let file = getChromeDir(getResolvedURI(gTestPath));
file.append("download.bin");
server.registerFile(`/${encodeURIComponent(TEST_FILE)}`, file);

/**
 * Check that IDN blocklisted characters are not escaped in
 * download file names.
 */
add_task(async function test_idn_blocklisted_char_not_escaped() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.always_ask_before_handling_new_types", false]],
  });

  info("Testing with " + TEST_URL);
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  registerCleanupFunction(async () => {
    await publicList.removeFinished();
  });
  let downloadFinished = promiseDownloadFinished(publicList);
  var tab = BrowserTestUtils.addTab(gBrowser, TEST_URL);
  let dl = await downloadFinished;
  ok(dl.succeeded, "Download should succeed.");
  Assert.equal(
    PathUtils.filename(dl.target.path),
    TEST_FILE,
    "Should not escape a download file name."
  );
  await IOUtils.remove(dl.target.path);

  BrowserTestUtils.removeTab(tab);
});
