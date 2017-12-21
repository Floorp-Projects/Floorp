/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ARCHIVE = "zips/zen.zip";
const SUBDIR = "zen";
const ENTRIES = ["beyond.txt", "waterwood.txt"];

Components.utils.import("resource://gre/modules/ZipUtils.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const archive = do_get_file(ARCHIVE, false);
const dir = do_get_profile().clone();
dir.append("test_ZipUtils");

function ensureExtracted(target) {
  target.append(SUBDIR);
  Assert.ok(target.exists());

  for (let i = 0; i < ENTRIES.length; i++) {
    let entry = target.clone();
    entry.append(ENTRIES[i]);
    do_print("ENTRY " + entry.path);
    Assert.ok(entry.exists());
  }
}

add_task(function test_extractFiles() {
  let target = dir.clone();
  target.append("test_extractFiles");

  try {
    ZipUtils.extractFiles(archive, target);
  } catch (e) {
    do_throw("Failed to extract synchronously!");
  }

  ensureExtracted(target);
});

add_task(async function test_extractFilesAsync() {
  let target = dir.clone();
  target.append("test_extractFilesAsync");
  target.create(Components.interfaces.nsIFile.DIRECTORY_TYPE,
    FileUtils.PERMS_DIRECTORY);

  await ZipUtils.extractFilesAsync(archive, target).then(
    function success() {
      do_print("SUCCESS");
      ensureExtracted(target);
    },
    function failure() {
      do_print("FAILURE");
      do_throw("Failed to extract asynchronously!");
    }
  );
});
