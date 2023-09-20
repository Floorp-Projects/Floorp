/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This test exercises functionality and also ensures the exit codes,
// which are a public API, do not change over time.
const { EXIT_CODE } = ChromeUtils.importESModule(
  "resource://gre/modules/BackgroundTasksManager.sys.mjs"
);

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

  let outputLines = [];
  let exitCode = await do_backgroundtask("removeDirectory", {
    extraArgs: [do_get_profile().path, LEAF_NAME, "10", ".abc"],
    onStdoutLine: line => outputLines.push(line),
  });
  equal(exitCode, EXIT_CODE.SUCCESS);
  equal(dir.exists(), false);
  equal(extraDir.exists(), false);

  if (AppConstants.platform !== "win") {
    // Check specific logs because there can still be some logs in certain conditions,
    // e.g. in code coverage (see bug 1831778 and bug 1804833)
    ok(
      outputLines.every(l => !l.includes("*** You are running in")),
      "Should not have logs by default"
    );
  }
});

add_task(async function test_no_extension() {
  // Test that not passing the extension is fine

  let dir = do_get_profile();
  dir.append(LEAF_NAME);
  dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);
  equal(dir.exists(), true);

  let exitCode = await do_backgroundtask("removeDirectory", {
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

  let task = do_backgroundtask("removeDirectory", {
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

  let task = do_backgroundtask("removeDirectory", {
    extraArgs: [do_get_profile().path, leafNameWithSpaces, "10", ""],
  });

  dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);

  let exitCode = await task;
  equal(exitCode, EXIT_CODE.SUCCESS);
  equal(dir.exists(), false);
});

add_task(async function test_missing_folder() {
  // We never create the directory.
  // We only wait for 1 second

  let dir = do_get_profile();
  dir.append(LEAF_NAME);
  let exitCode = await do_backgroundtask("removeDirectory", {
    extraArgs: [do_get_profile().path, LEAF_NAME, "1", ""],
  });

  equal(exitCode, EXIT_CODE.SUCCESS);
  equal(dir.exists(), false);
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
    let exitCode = await do_backgroundtask("removeDirectory", {
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

  let exitCode = await do_backgroundtask("removeDirectory", {
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

  exitCode = await do_backgroundtask("removeDirectory", {
    extraArgs: [do_get_profile().path, LEAF_NAME, "2", ".abc"],
  });

  equal(exitCode, EXIT_CODE.SUCCESS);
  equal(dir.exists(), false);
  equal(dir2.exists(), false);
  equal(file.exists(), true);
  file.remove(true);
});

add_task(async function test_two_tasks() {
  let dir1 = do_get_profile();
  dir1.append("leaf1.abc");

  let dir2 = do_get_profile();
  dir2.append("leaf2.abc");

  let tasks = [];
  tasks.push(
    do_backgroundtask("removeDirectory", {
      extraArgs: [do_get_profile().path, dir1.leafName, "5", ".abc"],
    })
  );
  dir1.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);
  tasks.push(
    do_backgroundtask("removeDirectory", {
      extraArgs: [do_get_profile().path, dir2.leafName, "5", ".abc"],
    })
  );
  dir2.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);

  let [r1, r2] = await Promise.all(tasks);

  equal(r1, EXIT_CODE.SUCCESS);
  equal(r2, EXIT_CODE.SUCCESS);
  equal(dir1.exists(), false);
  equal(dir2.exists(), false);
});

// This test creates a large number of tasks and directories.  Spawning a huge
// number of tasks concurrently (say, 50 or 100) appears to cause problems;
// since doing so is rather artificial, we haven't tracked this down.
const TASK_COUNT = 20;
add_task(async function test_aLotOfTasks() {
  let dirs = [];
  let tasks = [];

  for (let i = 0; i < TASK_COUNT; i++) {
    let dir = do_get_profile();
    dir.append(`leaf${i}.abc`);

    tasks.push(
      do_backgroundtask("removeDirectory", {
        extraArgs: [do_get_profile().path, dir.leafName, "5", ".abc"],
        extraEnv: { MOZ_LOG: "BackgroundTasks:5" },
      })
    );

    dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);
    dirs.push(dir);
  }

  let results = await Promise.all(tasks);
  for (let i in results) {
    equal(results[i], EXIT_CODE.SUCCESS, `Task ${i} should succeed`);
    equal(dirs[i].exists(), false, `leaf${i}.abc should not exist`);
  }
});

add_task(async function test_suffix() {
  // Make sure only the directories with the specified suffix will be removed

  let leaf = do_get_profile();
  leaf.append(LEAF_NAME);
  leaf.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);

  let abc = do_get_profile();
  abc.append("foo.abc");
  abc.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);
  equal(abc.exists(), true);

  let bcd = do_get_profile();
  bcd.append("foo.bcd");
  bcd.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);
  equal(bcd.exists(), true);

  // XXX: This should be able to skip passing LEAF_NAME (passing "" instead),
  // but bug 1853920 and bug 1853921 causes incorrect arguments in that case.
  let task = do_backgroundtask("removeDirectory", {
    extraArgs: [do_get_profile().path, LEAF_NAME, "10", ".abc"],
  });

  let exitCode = await task;
  equal(exitCode, EXIT_CODE.SUCCESS);
  equal(abc.exists(), false);
  equal(bcd.exists(), true);
});

add_task(async function test_suffix_wildcard() {
  // wildcard as a suffix should remove every subdirectories

  let leaf = do_get_profile();
  leaf.append(LEAF_NAME);
  leaf.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);

  let abc = do_get_profile();
  abc.append("foo.abc");
  abc.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);

  let cde = do_get_profile();
  cde.append("foo.cde");
  cde.create(Ci.nsIFile.DIRECTORY_TYPE, 0o744);

  // XXX: This should be able to skip passing LEAF_NAME (passing "" instead),
  // but bug 1853920 and bug 1853921 causes incorrect arguments in that case.
  let task = do_backgroundtask("removeDirectory", {
    extraArgs: [do_get_profile().path, LEAF_NAME, "10", "*"],
  });

  let exitCode = await task;
  equal(exitCode, EXIT_CODE.SUCCESS);
  equal(cde.exists(), false);
  equal(cde.exists(), false);
});
