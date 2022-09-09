/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  closeToolbox,
  openToolbox,
  COMPLICATED_URL,
  runTest,
  testSetup,
  testTeardown,
} = require("damp-test/tests/head");
const { Downloads } = require("resource://gre/modules/Downloads.jsm");

module.exports = async function() {
  await testSetup(COMPLICATED_URL);
  // Enable the screenshot button
  Services.prefs.setBoolPref(
    "devtools.command-button-screenshot.enabled",
    true
  );

  const toolbox = await openToolbox();

  let test = runTest(`screenshot.DAMP`);
  const onScreenshotDownloaded = waitUntilScreenshotDownloaded();
  toolbox.doc.querySelector("#command-button-screenshot").click();
  const filePath = await onScreenshotDownloaded;
  test.done();

  // Remove the downloaded screenshot file
  await IOUtils.remove(filePath);

  // ⚠️ Even after removing the file, the test could still manage to reuse files from the
  // previous test run if they have the same name. Since the screenshot file name is based
  // on a timestamp that has "second" precision, we might habe to wait for one second
  // to make sure screenshots taken in next iteration will have different names.

  Services.prefs.clearUserPref("devtools.command-button-screenshot.enabled");
  await resetDownloads();

  await closeToolbox();
  await testTeardown();
};

const allDownloads = new Set();
async function waitUntilScreenshotDownloaded() {
  const list = await Downloads.getList(Downloads.ALL);

  return new Promise(function(resolve) {
    const view = {
      onDownloadAdded: async download => {
        await download.whenSucceeded();
        if (allDownloads.has(download)) {
          return;
        }

        allDownloads.add(download);
        resolve(download.target.path);
        list.removeView(view);
      },
    };

    list.addView(view);
  });
}

async function resetDownloads() {
  const publicList = await Downloads.getList(Downloads.PUBLIC);
  const downloads = await publicList.getAll();
  for (const download of downloads) {
    publicList.remove(download);
    await download.finalize(true);
  }
  allDownloads.clear();
}
