/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { BackgroundTasksTestUtils } = ChromeUtils.import(
  "resource://testing-common/BackgroundTasksTestUtils.jsm"
);
BackgroundTasksTestUtils.init(this);
const do_backgroundtask = BackgroundTasksTestUtils.do_backgroundtask.bind(
  BackgroundTasksTestUtils
);

// This test exercises functionality and also ensures the exit codes,
// which are a public API, do not change over time.
const { EXIT_CODE } = ChromeUtils.import(
  "resource://gre/modules/BackgroundTasksManager.jsm"
).BackgroundTasksManager;

const LEAF_NAME = "newCacheFolder";

add_task(async function test_simple() {
  // This test creates a simple folder in the profile and passes it to
  // the background task to delete

  let dir = do_get_profile();
  dir.append(LEAF_NAME);
  dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);
  equal(dir.exists(), true);

  let extraDir = do_get_profile();
  extraDir.append("test.abc");
  extraDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);
  equal(extraDir.exists(), true);

  let exitCode = await do_backgroundtask("purgeHTTPCache", {
    extraArgs: [do_get_profile().path, LEAF_NAME, "10", ".abc"],
  });
  equal(exitCode, EXIT_CODE.SUCCESS);
  equal(dir.exists(), false);
  equal(extraDir.exists(), false);
});

add_task(async function test_no_extension() {
  // Test that not passing the extension is fine

  let dir = do_get_profile();
  dir.append(LEAF_NAME);
  dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);
  equal(dir.exists(), true);

  let exitCode = await do_backgroundtask("purgeHTTPCache", {
    extraArgs: [do_get_profile().path, LEAF_NAME, "10"],
  });
  equal(exitCode, EXIT_CODE.SUCCESS);
  equal(dir.exists(), false);
});

add_task(async function test_createAfter() {
  // First we create the task then we create the directory.
  // The task will only wait for 10 seconds.
  let dir = do_get_profile();
  dir.append(LEAF_NAME);

  let task = do_backgroundtask("purgeHTTPCache", {
    extraArgs: [do_get_profile().path, LEAF_NAME, "10", ""],
  });

  dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);

  let exitCode = await task;
  equal(exitCode, EXIT_CODE.SUCCESS);
  equal(dir.exists(), false);
});

add_task(async function test_folderNameWithSpaces() {
  // We pass in a leaf name with spaces just to make sure the command line
  // argument parsing works properly.
  let dir = do_get_profile();
  let leafNameWithSpaces = `${LEAF_NAME} space`;
  dir.append(leafNameWithSpaces);

  let task = do_backgroundtask("purgeHTTPCache", {
    extraArgs: [do_get_profile().path, leafNameWithSpaces, "10", ""],
  });

  dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);

  let exitCode = await task;
  equal(exitCode, EXIT_CODE.SUCCESS);
  equal(dir.exists(), false);
});

add_task(async function test_missing_folder() {
  // We never create the directory. The task should just fail.
  // We only wait for 1 second

  let exitCode = await do_backgroundtask("purgeHTTPCache", {
    extraArgs: [do_get_profile().path, LEAF_NAME, "1", ""],
  });

  equal(exitCode, EXIT_CODE.EXCEPTION);
});

add_task(
  { skip_if: () => mozinfo.os != "win" },
  async function test_ro_file_in_folder() {
    let dir = do_get_profile();
    dir.append(LEAF_NAME);
    dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);
    equal(dir.exists(), true);

    let file = dir.clone();
    file.append("ro_file");
    file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);
    file.QueryInterface(Ci.nsILocalFileWin).readOnly = true;

    // Make sure that we can move with a readonly file in the directory
    dir.moveTo(null, "newName");

    // This will fail because of the readonly file.
    let exitCode = await do_backgroundtask("purgeHTTPCache", {
      extraArgs: [do_get_profile().path, "newName", "1", ""],
    });

    equal(exitCode, EXIT_CODE.EXCEPTION);

    dir = do_get_profile();
    dir.append("newName");
    file = dir.clone();
    file.append("ro_file");
    equal(file.exists(), true);

    // Remove readonly attribute and try again.
    // As a followup we might want to force an old cache dir to be deleted, even if it has readonly files in it.
    file.QueryInterface(Ci.nsILocalFileWin).readOnly = false;
    dir.remove(true);
    equal(file.exists(), false);
    equal(dir.exists(), false);
  }
);

add_task(async function test_purgeFile() {
  // Test that only directories are purged by the background task
  let file = do_get_profile();
  file.append(LEAF_NAME);
  file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);
  equal(file.exists(), true);

  let exitCode = await do_backgroundtask("purgeHTTPCache", {
    extraArgs: [do_get_profile().path, LEAF_NAME, "2", ""],
  });
  equal(exitCode, EXIT_CODE.EXCEPTION);
  equal(file.exists(), true);

  file.remove(true);
  let dir = file.clone();
  dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);
  equal(dir.exists(), true);
  file = do_get_profile();
  file.append("test.abc");
  file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);
  equal(file.exists(), true);
  let dir2 = do_get_profile();
  dir2.append("dir.abc");
  dir2.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);
  equal(dir2.exists(), true);

  exitCode = await do_backgroundtask("purgeHTTPCache", {
    extraArgs: [do_get_profile().path, LEAF_NAME, "2", ".abc"],
  });

  equal(exitCode, EXIT_CODE.SUCCESS);
  equal(dir.exists(), false);
  equal(dir2.exists(), false);
  equal(file.exists(), true);
  file.remove(true);
});
