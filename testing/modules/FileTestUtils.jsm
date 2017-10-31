/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides testing functions dealing with local files and their contents.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "FileTestUtils",
];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/AsyncShutdown.jsm", this);
Cu.import("resource://gre/modules/DownloadPaths.jsm", this);
Cu.import("resource://gre/modules/FileUtils.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://testing-common/Assert.jsm", this);

let gFileCounter = 1;

this.FileTestUtils = {
  /**
   * Returns a reference to a temporary file that is guaranteed not to exist and
   * to have never been created before. If a file or a directory with this name
   * is created by the test, it will be deleted when all tests terminate.
   *
   * @param suggestedName [optional]
   *        Any extension on this template file name will be preserved. If this
   *        is unspecified, the returned file name will have the generic ".dat"
   *        extension, which may indicate either a binary or a text data file.
   *
   * @return nsIFile pointing to a non-existent file in a temporary directory.
   *
   * @note It is not enough to delete the file if it exists, or to delete the
   *       file after calling nsIFile.createUnique, because on Windows the
   *       delete operation in the file system may still be pending, preventing
   *       a new file with the same name to be created.
   */
  getTempFile(suggestedName = "test.dat") {
    // Prepend a serial number to the extension in the suggested leaf name.
    let [base, ext] = DownloadPaths.splitBaseNameAndExtension(suggestedName);
    let leafName = base + "-" + gFileCounter + ext;
    gFileCounter++;

    // Get a file reference under the temporary directory for this test file.
    let file = this._globalTemporaryDirectory.clone();
    file.append(leafName);
    Assert.ok(!file.exists(), "Sanity check the temporary file doesn't exist.");
    return file;
  },
};

/**
 * Returns a reference to a global temporary directory that will be deleted
 * when all tests terminate.
 */
XPCOMUtils.defineLazyGetter(FileTestUtils, "_globalTemporaryDirectory", () => {
  // While previous test runs should have deleted their temporary directories,
  // on Windows they might still be pending deletion on the physical file
  // system. This makes a simple nsIFile.createUnique call unreliable, and we
  // have to use a random number to make a collision unlikely.
  let randomNumber = Math.floor(Math.random() * 1000000);
  let dir = FileUtils.getFile("TmpD", ["testdir-" + randomNumber]);
  dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  AsyncShutdown.profileBeforeChange.addBlocker("Removing test files", () => {
    // Note that this may fail if any test leaves inaccessible files behind.
    dir.remove(true);
  });
  return dir;
});
