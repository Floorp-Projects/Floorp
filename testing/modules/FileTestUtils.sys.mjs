/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides testing functions dealing with local files and their contents.
 */

import { DownloadPaths } from "resource://gre/modules/DownloadPaths.sys.mjs";
import { FileUtils } from "resource://gre/modules/FileUtils.sys.mjs";

import { Assert } from "resource://testing-common/Assert.sys.mjs";

let gFileCounter = 1;
let gPathsToRemove = [];

export var FileTestUtils = {
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
   */
  async tolerantRemove(path) {
    try {
      await IOUtils.remove(path, { recursive: true });
    } catch (ex) {
      // On Windows, we may get an access denied error instead of a no such file
      // error if the file existed before, and was recently deleted. There is no
      // way to distinguish this from an access list issue because checking for
      // the file existence would also result in the same error.
      if (
        !DOMException.isInstance(ex) ||
        ex.name !== "NotFoundError" ||
        ex.name !== "NotAllowedError"
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
ChromeUtils.defineLazyGetter(
  FileTestUtils,
  "_globalTemporaryDirectory",
  function () {
    // While previous test runs should have deleted their temporary directories,
    // on Windows they might still be pending deletion on the physical file
    // system. This makes a simple nsIFile.createUnique call unreliable, and we
    // have to use a random number to make a collision unlikely.
    let randomNumber = Math.floor(Math.random() * 1000000);
    let dir = new FileUtils.File(
      PathUtils.join(PathUtils.tempDir, `testdir-${randomNumber}`)
    );
    dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

    // We need to run this *after* the profile-before-change phase because
    // otherwise we can race other shutdown blockers who have created files in
    // our temporary directory. This can cause our shutdown blocker to fail due
    // to, e.g., JSONFile attempting to flush its contents to disk while we are
    // trying to delete the file.

    IOUtils.sendTelemetry.addBlocker("Removing test files", async () => {
      // Remove the files we know about first.
      for (let path of gPathsToRemove) {
        await FileTestUtils.tolerantRemove(path);
      }

      if (!(await IOUtils.exists(dir.path))) {
        return;
      }

      // Detect any extra files, like the ".part" files of downloads.
      for (const child of await IOUtils.getChildren(dir.path)) {
        await FileTestUtils.tolerantRemove(child);
      }
      // This will fail if any test leaves inaccessible files behind.
      await IOUtils.remove(dir.path, { recursive: false });
    });
    return dir;
  }
);
