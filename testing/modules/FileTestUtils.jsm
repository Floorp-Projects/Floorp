/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides testing functions dealing with local files and their contents.
 */

"use strict";

var EXPORTED_SYMBOLS = ["FileTestUtils"];

const { DownloadPaths } = ChromeUtils.import(
  "resource://gre/modules/DownloadPaths.jsm"
);
const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { Assert } = ChromeUtils.import("resource://testing-common/Assert.jsm");

let gFileCounter = 1;
let gPathsToRemove = [];

var FileTestUtils = {
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

    // Since directory iteration on Windows may not see files that have just
    // been created, keep track of the known file names to be removed.
    gPathsToRemove.push(file.path);
    return file;
  },

  /**
   * Attemps to remove the given file or directory recursively, in a way that
   * works even on Windows, where race conditions may occur in the file system
   * when creating and removing files at the pace of the test suites.
   *
   * The function may fail silently if access is denied. This means that it
   * should only be used to clean up temporary files, rather than for cases
   * where the removal is part of a test and must be guaranteed.
   *
   * @param path
   *        String representing the path to remove.
   * @param isDir [optional]
   *        Indicates whether this is a directory. If not specified, this will
   *        be determined by querying the file system.
   */
  async tolerantRemove(path, isDir) {
    try {
      if (isDir === undefined) {
        isDir = (await OS.File.stat(path)).isDir;
      }
      if (isDir) {
        // The test created a directory, remove its contents recursively. The
        // deletion may still fail with an access denied error on Windows if any
        // of the files in the folder were recently deleted.
        await OS.File.removeDir(path);
      } else {
        // This is the usual case of a test file that has to be removed.
        await OS.File.remove(path);
      }
    } catch (ex) {
      // On Windows, we may get an access denied error instead of a no such file
      // error if the file existed before, and was recently deleted. There is no
      // way to distinguish this from an access list issue because checking for
      // the file existence would also result in the same error.
      if (
        !(ex instanceof OS.File.Error) ||
        !(ex.becauseNoSuchFile || ex.becauseAccessDenied)
      ) {
        throw ex;
      }
    }
  },
};

/**
 * Returns a reference to a global temporary directory that will be deleted
 * when all tests terminate.
 */
XPCOMUtils.defineLazyGetter(
  FileTestUtils,
  "_globalTemporaryDirectory",
  function() {
    // While previous test runs should have deleted their temporary directories,
    // on Windows they might still be pending deletion on the physical file
    // system. This makes a simple nsIFile.createUnique call unreliable, and we
    // have to use a random number to make a collision unlikely.
    let randomNumber = Math.floor(Math.random() * 1000000);
    let dir = FileUtils.getFile("TmpD", ["testdir-" + randomNumber]);
    dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

    // We need to run this *after* the profile-before-change phase because
    // otherwise we can race other shutdown blockers who have created files in
    // our temporary directory. This can cause our shutdown blocker to fail due
    // to, e.g., JSONFile attempting to flush its contents to disk while we are
    // trying to delete the file.
    OS.File.shutdown.addBlocker("Removing test files", async () => {
      // Remove the files we know about first.
      for (let path of gPathsToRemove) {
        await this.tolerantRemove(path);
      }

      if (!(await OS.File.exists(dir.path))) {
        return;
      }

      // Detect any extra files, like the ".part" files of downloads.
      let iterator = new OS.File.DirectoryIterator(dir.path);
      try {
        await iterator.forEach(entry =>
          this.tolerantRemove(entry.path, entry.isDir)
        );
      } finally {
        iterator.close();
      }
      // This will fail if any test leaves inaccessible files behind.
      await OS.File.removeEmptyDir(dir.path);
    });
    return dir;
  }
);
