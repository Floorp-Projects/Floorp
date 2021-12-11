/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env mozilla/chrome-worker, node */
/* global finish, log */

importScripts("chrome://mochikit/content/tests/SimpleTest/WorkerSimpleTest.js");
importScripts("resource://gre/modules/workers/require.js");

var SharedAll = require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
SharedAll.Config.DEBUG = true;

function should_throw(f) {
  try {
    f();
  } catch (x) {
    return x;
  }
  return null;
}

self.onmessage = function onmessage_start(msg) {
  self.onmessage = function onmessage_ignored(msg) {
    log("ignored message " + JSON.stringify(msg.data));
  };
  try {
    test_init();
    test_open_existing_file();
    test_open_non_existing_file();
    test_flush_open_file();
    test_copy_existing_file();
    test_position();
    test_move_file();
    test_iter_dir();
    test_info();
    test_path();
    test_exists_file();
    test_remove_file();
  } catch (x) {
    log("Catching error: " + x);
    log("Stack: " + x.stack);
    log("Source: " + x.toSource());
    ok(false, x.toString() + "\n" + x.stack);
  }
  finish();
};

function test_init() {
  info("Starting test_init");
  importScripts("resource://gre/modules/osfile.jsm");
}

/**
 * Test that we can open an existing file.
 */
function test_open_existing_file() {
  info("Starting test_open_existing");
  let file = OS.File.open(
    "chrome/toolkit/components/osfile/tests/mochi/worker_test_osfile_unix.js"
  );
  file.close();
}

/**
 * Test that opening a file that does not exist fails with the right error.
 */
function test_open_non_existing_file() {
  info("Starting test_open_non_existing");
  let exn;
  try {
    OS.File.open("/I do not exist");
  } catch (x) {
    exn = x;
    info("test_open_non_existing_file: Exception detail " + exn);
  }
  ok(!!exn, "test_open_non_existing_file: Exception was raised ");
  ok(
    exn instanceof OS.File.Error,
    "test_open_non_existing_file: Exception was a OS.File.Error"
  );
  ok(
    exn.becauseNoSuchFile,
    "test_open_non_existing_file: Exception confirms that the file does not exist"
  );
}

/**
 * Test that to ensure that |foo.flush()| does not
 * cause an error, where |foo| is an open file.
 */
function test_flush_open_file() {
  info("Starting test_flush_open_file");
  let tmp = "test_flush.tmp";
  let file = OS.File.open(tmp, { create: true, write: true });
  file.flush();
  file.close();
  OS.File.remove(tmp);
}

/**
 * Utility function for comparing two files (or a prefix of two files).
 *
 * This function returns nothing but fails of both files (or prefixes)
 * are not identical.
 *
 * @param {string} test The name of the test (used for logging).
 * @param {string} sourcePath The name of the first file.
 * @param {string} destPath The name of the second file.
 * @param {number=} prefix If specified, only compare the |prefix|
 * first bytes of |sourcePath| and |destPath|.
 */
function compare_files(test, sourcePath, destPath, prefix) {
  info(test + ": Comparing " + sourcePath + " and " + destPath);
  let source = OS.File.open(sourcePath);
  let dest = OS.File.open(destPath);
  info("Files are open");
  let sourceResult, destResult;
  try {
    if (prefix != undefined) {
      sourceResult = source.read(prefix);
      destResult = dest.read(prefix);
    } else {
      sourceResult = source.read();
      destResult = dest.read();
    }
    is(
      sourceResult.length,
      destResult.length,
      test + ": Both files have the same size"
    );
    for (let i = 0; i < sourceResult.length; ++i) {
      if (sourceResult[i] != destResult[i]) {
        is(sourceResult[i] != destResult[i], test + ": Comparing char " + i);
        break;
      }
    }
  } finally {
    source.close();
    dest.close();
  }
  info(test + ": Comparison complete");
}

/**
 * Test that copying a file using |copy| works.
 */
function test_copy_existing_file() {
  let src_file_name = OS.Path.join(
    "chrome",
    "toolkit",
    "components",
    "osfile",
    "tests",
    "mochi",
    "worker_test_osfile_front.js"
  );
  let tmp_file_name = "test_osfile_front.tmp";
  info("Starting test_copy_existing");
  OS.File.copy(src_file_name, tmp_file_name);

  info("test_copy_existing: Copy complete");
  compare_files("test_copy_existing", src_file_name, tmp_file_name);

  // Create a bogus file with arbitrary content, then attempt to overwrite
  // it with |copy|.
  let dest = OS.File.open(tmp_file_name, { trunc: true });
  let buf = new Uint8Array(50);
  dest.write(buf);
  dest.close();

  OS.File.copy(src_file_name, tmp_file_name);

  compare_files("test_copy_existing 2", src_file_name, tmp_file_name);

  // Attempt to overwrite with noOverwrite
  let exn;
  try {
    OS.File.copy(src_file_name, tmp_file_name, { noOverwrite: true });
  } catch (x) {
    exn = x;
  }
  ok(
    !!exn,
    "test_copy_existing: noOverwrite prevents overwriting existing files"
  );

  info("test_copy_existing: Cleaning up");
  OS.File.remove(tmp_file_name);
}

/**
 * Test that moving a file works.
 */
function test_move_file() {
  info("test_move_file: Starting");
  // 1. Copy file into a temporary file
  let src_file_name = OS.Path.join(
    "chrome",
    "toolkit",
    "components",
    "osfile",
    "tests",
    "mochi",
    "worker_test_osfile_front.js"
  );
  let tmp_file_name = "test_osfile_front.tmp";
  let tmp2_file_name = "test_osfile_front.tmp2";
  OS.File.copy(src_file_name, tmp_file_name);

  info("test_move_file: Copy complete");

  // 2. Move
  OS.File.move(tmp_file_name, tmp2_file_name);

  info("test_move_file: Move complete");

  // 3. Check that destination exists
  compare_files("test_move_file", src_file_name, tmp2_file_name);

  // 4. Check that original file does not exist anymore
  let exn;
  try {
    OS.File.open(tmp_file_name);
  } catch (x) {
    exn = x;
  }
  ok(!!exn, "test_move_file: Original file has been removed");

  info("test_move_file: Cleaning up");
  OS.File.remove(tmp2_file_name);
}

function test_iter_dir() {
  info("test_iter_dir: Starting");

  // Create a file, to be sure that it exists
  let tmp_file_name = "test_osfile_front.tmp";
  let tmp_file = OS.File.open(tmp_file_name, { write: true, trunc: true });
  tmp_file.close();

  let parent = OS.File.getCurrentDirectory();
  info("test_iter_dir: directory " + parent);
  let iterator = new OS.File.DirectoryIterator(parent);
  info("test_iter_dir: iterator created");
  let encountered_tmp_file = false;
  for (let entry of iterator) {
    // Checking that |name| can be decoded properly
    info("test_iter_dir: encountering entry " + entry.name);

    if (entry.name == tmp_file_name) {
      encountered_tmp_file = true;
      isnot(
        entry.isDir,
        "test_iter_dir: The temporary file is not a directory"
      );
      isnot(entry.isSymLink, "test_iter_dir: The temporary file is not a link");
    }

    let file;
    let success = true;
    try {
      file = OS.File.open(entry.path);
    } catch (x) {
      if (x.becauseNoSuchFile) {
        success = false;
      }
    }
    if (file) {
      file.close();
    }
    ok(success, "test_iter_dir: Entry " + entry.path + " exists");

    if (OS.Win) {
      // We assume that the files are at least as recent as 2009.
      // Since this test was written in 2011 and some of our packaging
      // sets dates arbitrarily to 2010, this should be safe.
      let year = new Date().getFullYear();
      let creation = entry.winCreationDate;
      ok(creation, "test_iter_dir: Windows creation date exists: " + creation);
      ok(
        creation.getFullYear() >= 2009 && creation.getFullYear() <= year,
        "test_iter_dir: consistent creation date"
      );

      let lastWrite = entry.winLastWriteDate;
      ok(
        lastWrite,
        "test_iter_dir: Windows lastWrite date exists: " + lastWrite
      );
      ok(
        lastWrite.getFullYear() >= 2009 && lastWrite.getFullYear() <= year,
        "test_iter_dir: consistent lastWrite date"
      );

      let lastAccess = entry.winLastAccessDate;
      ok(
        lastAccess,
        "test_iter_dir: Windows lastAccess date exists: " + lastAccess
      );
      ok(
        lastAccess.getFullYear() >= 2009 && lastAccess.getFullYear() <= year,
        "test_iter_dir: consistent lastAccess date"
      );
    }
  }
  ok(encountered_tmp_file, "test_iter_dir: We have found the temporary file");

  info("test_iter_dir: Cleaning up");
  iterator.close();

  // Testing nextBatch()
  iterator = new OS.File.DirectoryIterator(parent);
  let allentries = [];
  for (let x of iterator) {
    allentries.push(x);
  }
  iterator.close();

  ok(
    allentries.length >= 14,
    "test_iter_dir: Meta-check: the test directory should contain at least 14 items"
  );

  iterator = new OS.File.DirectoryIterator(parent);
  let firstten = iterator.nextBatch(10);
  is(firstten.length, 10, "test_iter_dir: nextBatch(10) returns 10 items");
  for (let i = 0; i < firstten.length; ++i) {
    is(
      allentries[i].path,
      firstten[i].path,
      "test_iter_dir: Checking that batch returns the correct entries"
    );
  }
  let nextthree = iterator.nextBatch(3);
  is(nextthree.length, 3, "test_iter_dir: nextBatch(3) returns 3 items");
  for (let i = 0; i < nextthree.length; ++i) {
    is(
      allentries[i + firstten.length].path,
      nextthree[i].path,
      "test_iter_dir: Checking that batch 2 returns the correct entries"
    );
  }
  let everythingelse = iterator.nextBatch();
  ok(
    everythingelse.length >= 1,
    "test_iter_dir: nextBatch() returns at least one item"
  );
  for (let i = 0; i < everythingelse.length; ++i) {
    is(
      allentries[i + firstten.length + nextthree.length].path,
      everythingelse[i].path,
      "test_iter_dir: Checking that batch 3 returns the correct entries"
    );
  }
  is(
    iterator.nextBatch().length,
    0,
    "test_iter_dir: Once there is nothing left, nextBatch returns an empty array"
  );
  iterator.close();

  iterator = new OS.File.DirectoryIterator(parent);
  iterator.close();
  is(
    iterator.nextBatch().length,
    0,
    "test_iter_dir: nextBatch on closed iterator returns an empty array"
  );

  iterator = new OS.File.DirectoryIterator(parent);
  let allentries2 = iterator.nextBatch();
  is(
    allentries.length,
    allentries2.length,
    "test_iter_dir: Checking that getBatch(null) returns the right number of entries"
  );
  for (let i = 0; i < allentries.length; ++i) {
    is(
      allentries[i].path,
      allentries2[i].path,
      "test_iter_dir: Checking that getBatch(null) returns everything in the right order"
    );
  }
  iterator.close();

  // Test forEach
  iterator = new OS.File.DirectoryIterator(parent);
  let index = 0;
  iterator.forEach(function cb(entry, aIndex, aIterator) {
    is(index, aIndex, "test_iter_dir: Checking that forEach index is correct");
    ok(
      iterator == aIterator,
      "test_iter_dir: Checking that right iterator is passed"
    );
    if (index < 10) {
      is(
        allentries[index].path,
        entry.path,
        "test_iter_dir: Checking that forEach entry is correct"
      );
    } else if (index == 10) {
      iterator.close();
    } else {
      ok(false, "test_iter_dir: Checking that forEach can be stopped early");
    }
    ++index;
  });
  iterator.close();

  // test for prototype |OS.File.DirectoryIterator.unixAsFile|
  if ("unixAsFile" in OS.File.DirectoryIterator.prototype) {
    info("testing property unixAsFile");
    let path = OS.Path.join(
      "chrome",
      "toolkit",
      "components",
      "osfile",
      "tests",
      "mochi"
    );
    iterator = new OS.File.DirectoryIterator(path);

    let dir_file = iterator.unixAsFile(); // return |File|
    let stat0 = dir_file.stat();
    let stat1 = OS.File.stat(path);

    let unix_info_to_string = function unix_info_to_string(info) {
      return (
        "| " +
        info.unixMode +
        " | " +
        info.unixOwner +
        " | " +
        info.unixGroup +
        " | " +
        info.creationDate +
        " | " +
        info.lastModificationDate +
        " | " +
        info.lastAccessDate +
        " | " +
        info.size +
        " |"
      );
    };

    let s0_string = unix_info_to_string(stat0);
    let s1_string = unix_info_to_string(stat1);

    ok(stat0.isDir, "unixAsFile returned a directory");
    is(s0_string, s1_string, "unixAsFile returned the correct file");
    dir_file.close();
    iterator.close();
  }
  info("test_iter_dir: Complete");
}

function test_position() {
  info("test_position: Starting");

  ok("POS_START" in OS.File, "test_position: POS_START exists");
  ok("POS_CURRENT" in OS.File, "test_position: POS_CURRENT exists");
  ok("POS_END" in OS.File, "test_position: POS_END exists");

  let ARBITRARY_POSITION = 321;
  let src_file_name = OS.Path.join(
    "chrome",
    "toolkit",
    "components",
    "osfile",
    "tests",
    "mochi",
    "worker_test_osfile_front.js"
  );

  let file = OS.File.open(src_file_name);
  is(file.getPosition(), 0, "test_position: Initial position is 0");

  let size = 0 + file.stat().size; // Hack: We can remove this 0 + once 776259 has landed

  file.setPosition(ARBITRARY_POSITION, OS.File.POS_START);
  is(
    file.getPosition(),
    ARBITRARY_POSITION,
    "test_position: Setting position from start"
  );

  file.setPosition(0, OS.File.POS_START);
  is(
    file.getPosition(),
    0,
    "test_position: Setting position from start back to 0"
  );

  file.setPosition(ARBITRARY_POSITION);
  is(
    file.getPosition(),
    ARBITRARY_POSITION,
    "test_position: Setting position without argument"
  );

  file.setPosition(-ARBITRARY_POSITION, OS.File.POS_END);
  is(
    file.getPosition(),
    size - ARBITRARY_POSITION,
    "test_position: Setting position from end"
  );

  file.setPosition(ARBITRARY_POSITION, OS.File.POS_CURRENT);
  is(file.getPosition(), size, "test_position: Setting position from current");

  file.close();
  info("test_position: Complete");
}

function test_info() {
  info("test_info: Starting");

  let filename = "test_info.tmp";
  let size = 261; // An arbitrary file length
  let start = new Date();

  // Cleanup any leftover from previous tests
  try {
    OS.File.remove(filename);
    info("test_info: Cleaned up previous garbage");
  } catch (x) {
    if (!x.becauseNoSuchFile) {
      throw x;
    }
    info("test_info: No previous garbage");
  }

  let file = OS.File.open(filename, { trunc: true });
  let buf = new ArrayBuffer(size);
  file._write(buf, size);
  file.close();

  // Test OS.File.stat on new file
  let stat = OS.File.stat(filename);
  ok(!!stat, "test_info: info acquired");
  ok(!stat.isDir, "test_info: file is not a directory");
  is(stat.isSymLink, false, "test_info: file is not a link");
  is(stat.size.toString(), size, "test_info: correct size");

  let stop = new Date();

  // We round down/up by 1s as file system precision is lower than
  // Date precision (no clear specifications about that, but it seems
  // that this can be a little over 1 second under ext3 and 2 seconds
  // under FAT).
  let SLOPPY_FILE_SYSTEM_ADJUSTMENT = 3000;
  let startMs = start.getTime() - SLOPPY_FILE_SYSTEM_ADJUSTMENT;
  let stopMs = stop.getTime() + SLOPPY_FILE_SYSTEM_ADJUSTMENT;
  info("Testing stat with bounds [ " + startMs + ", " + stopMs + " ]");

  (function() {
    let birth;
    if ("winBirthDate" in stat) {
      birth = stat.winBirthDate;
    } else if ("macBirthDate" in stat) {
      birth = stat.macBirthDate;
    } else {
      ok(true, "Skipping birthdate test");
      return;
    }
    ok(birth.getTime() <= stopMs, "test_info: platformBirthDate is consistent");
    // Note: Previous versions of this test checked whether the file
    // has been created after the start of the test. Unfortunately,
    // this sometimes failed under Windows, in specific circumstances:
    // if the file has been removed at the start of the test and
    // recreated immediately, the Windows file system detects this and
    // decides that the file was actually truncated rather than
    // recreated, hence that it should keep its previous creation
    // date.  Debugging hilarity ensues.
  })();

  let change = stat.lastModificationDate;
  info("Testing lastModificationDate: " + change);
  ok(
    change.getTime() >= startMs && change.getTime() <= stopMs,
    "test_info: lastModificationDate is consistent"
  );

  // Test OS.File.prototype.stat on new file
  file = OS.File.open(filename);
  try {
    stat = file.stat();
  } finally {
    file.close();
  }

  ok(!!stat, "test_info: info acquired 2");
  ok(!stat.isDir, "test_info: file is not a directory 2");
  ok(!stat.isSymLink, "test_info: file is not a link 2");
  is(stat.size.toString(), size, "test_info: correct size 2");

  stop = new Date();

  // Round up/down as above
  startMs = start.getTime() - SLOPPY_FILE_SYSTEM_ADJUSTMENT;
  stopMs = stop.getTime() + SLOPPY_FILE_SYSTEM_ADJUSTMENT;
  info("Testing stat 2 with bounds [ " + startMs + ", " + stopMs + " ]");

  let access = stat.lastAccessDate;
  info("Testing lastAccessDate: " + access);
  ok(
    access.getTime() >= startMs && access.getTime() <= stopMs,
    "test_info: lastAccessDate is consistent"
  );

  change = stat.lastModificationDate;
  info("Testing lastModificationDate 2: " + change);
  ok(
    change.getTime() >= startMs && change.getTime() <= stopMs,
    "test_info: lastModificationDate 2 is consistent"
  );

  // Test OS.File.stat on directory
  stat = OS.File.stat(OS.File.getCurrentDirectory());
  ok(!!stat, "test_info: info on directory acquired");
  ok(stat.isDir, "test_info: directory is a directory");

  info("test_info: Complete");
}

// Note that most of the features of path are tested in
// worker_test_osfile_{unix, win}.js
function test_path() {
  info("test_path: starting");
  let abcd = OS.Path.join("a", "b", "c", "d");
  is(OS.Path.basename(abcd), "d", "basename of a/b/c/d");

  let abc = OS.Path.join("a", "b", "c");
  is(OS.Path.dirname(abcd), abc, "dirname of a/b/c/d");

  let abdotsc = OS.Path.join("a", "b", "..", "c");
  is(OS.Path.normalize(abdotsc), OS.Path.join("a", "c"), "normalize a/b/../c");

  let adotsdotsdots = OS.Path.join("a", "..", "..", "..");
  is(
    OS.Path.normalize(adotsdotsdots),
    OS.Path.join("..", ".."),
    "normalize a/../../.."
  );

  info("test_path: Complete");
}

/**
 * Test the file |exists| method.
 */
function test_exists_file() {
  let file_name = OS.Path.join(
    "chrome",
    "toolkit",
    "components",
    "osfile",
    "tests",
    "mochi",
    "test_osfile_front.xhtml"
  );
  info("test_exists_file: starting");
  ok(
    OS.File.exists(file_name),
    "test_exists_file: file exists (OS.File.exists)"
  );
  ok(
    !OS.File.exists(file_name + ".tmp"),
    "test_exists_file: file does not exists (OS.File.exists)"
  );

  let dir_name = OS.Path.join(
    "chrome",
    "toolkit",
    "components",
    "osfile",
    "tests",
    "mochi"
  );
  ok(OS.File.exists(dir_name), "test_exists_file: directory exists");
  ok(
    !OS.File.exists(dir_name) + ".tmp",
    "test_exists_file: directory does not exist"
  );

  info("test_exists_file: complete");
}

/**
 * Test the file |remove| method.
 */
function test_remove_file() {
  let absent_file_name = "test_osfile_front_absent.tmp";

  // Check that removing absent files is handled correctly
  let exn = should_throw(function() {
    OS.File.remove(absent_file_name, { ignoreAbsent: false });
  });
  ok(!!exn, "test_remove_file: throws if there is no such file");

  exn = should_throw(function() {
    OS.File.remove(absent_file_name, { ignoreAbsent: true });
    OS.File.remove(absent_file_name);
  });
  ok(!exn, "test_remove_file: ignoreAbsent works");

  if (OS.Win) {
    let file_name = "test_osfile_front_file_to_remove.tmp";
    let file = OS.File.open(file_name, { write: true });
    file.close();
    ok(OS.File.exists(file_name), "test_remove_file: test file exists");
    OS.Win.File.SetFileAttributes(
      file_name,
      OS.Constants.Win.FILE_ATTRIBUTE_READONLY
    );
    OS.File.remove(file_name);
    ok(
      !OS.File.exists(file_name),
      "test_remove_file: test file has been removed"
    );
  }
}
