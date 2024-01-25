/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const FILE_URI = Services.appinfo.OS == "WINNT" ? "file://C:/" : "file:///";

const RESOURCE_URI = "resource:///";

const ZIP_FILE = "res_empty.zip";
const ZIP_DIR = getChromeDir(getResolvedURI(gTestPath));
ZIP_DIR.append(ZIP_FILE);
ZIP_DIR.normalize();
const ZIP_URI = Services.io.newFileURI(ZIP_DIR).spec;
const JAR_URI = "jar:" + ZIP_URI + "!/";

const BASE_URI = "http://mochi.test:8888/browser/netwerk/test/browser/";
const INDEX_URI = BASE_URI + "res_http_index_format";

async function expectIndexToDisplay(aUrl, aExpectToDisplay) {
  info(`Opening ${aUrl} to check if index will be displayed`);
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, aUrl, true);
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [aExpectToDisplay],
    aExpectToDisplay => {
      is(
        !!content.document.getElementsByTagName("table").length,
        aExpectToDisplay,
        "The index should be displayed"
      );
    }
  );
  await BrowserTestUtils.removeTab(tab);
}

add_task(async function test_http_index_format() {
  await expectIndexToDisplay(FILE_URI, true);
  await expectIndexToDisplay(RESOURCE_URI, true);
  await expectIndexToDisplay(JAR_URI, true);
  await expectIndexToDisplay(INDEX_URI, false);

  await SpecialPowers.pushPrefEnv({
    set: [["network.http_index_format.allowed_schemes", "*"]],
  });

  await expectIndexToDisplay(INDEX_URI, true);
});
