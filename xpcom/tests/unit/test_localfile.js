/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

const MAX_TIME_DIFFERENCE = 2500;
const MILLIS_PER_DAY = 1000 * 60 * 60 * 24;

var LocalFile = CC("@mozilla.org/file/local;1", "nsIFile", "initWithPath");

function sleep(ms) {
  // We are measuring timestamps, which are slightly fuzzed, and just need to
  // measure that they are increasing.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  return new Promise(resolve => setTimeout(() => resolve(), ms));
}

add_task(function test_toplevel_parent_is_null() {
  try {
    var lf = new LocalFile("C:\\");

    // not required by API, but a property on which the implementation of
    // parent == null relies for correctness
    Assert.ok(lf.path.length == 2);

    Assert.ok(lf.parent === null);
  } catch (e) {
    // not Windows
    Assert.equal(e.result, Cr.NS_ERROR_FILE_UNRECOGNIZED_PATH);
  }
});

add_task(function test_normalize_crash_if_media_missing() {
  const a = "a".charCodeAt(0);
  const z = "z".charCodeAt(0);
  for (var i = a; i <= z; ++i) {
    try {
      LocalFile(String.fromCharCode(i) + ":.\\test").normalize();
    } catch (e) {}
  }
});

// Tests that changing a file's modification time is possible
add_task(async function test_file_modification_time() {
  let file = do_get_profile();
  file.append("testfile");

  // Should never happen but get rid of it anyway
  if (file.exists()) {
    file.remove(true);
  }

  const now = Date.now();
  file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);
  Assert.ok(file.exists());

  const atime = file.lastAccessedTime;

  // Modification time may be out by up to 2 seconds on FAT filesystems. Test
  // with a bit of leeway, close enough probably means it is correct.
  let diff = Math.abs(file.lastModifiedTime - now);
  Assert.ok(diff < MAX_TIME_DIFFERENCE);

  const yesterday = now - MILLIS_PER_DAY;
  file.lastModifiedTime = yesterday;

  diff = Math.abs(file.lastModifiedTime - yesterday);
  Assert.ok(diff < MAX_TIME_DIFFERENCE);
  Assert.equal(
    file.lastAccessedTime,
    atime,
    "Setting lastModifiedTime should not set lastAccessedTime"
  );

  const tomorrow = now + MILLIS_PER_DAY;
  file.lastModifiedTime = tomorrow;

  diff = Math.abs(file.lastModifiedTime - tomorrow);
  Assert.ok(diff < MAX_TIME_DIFFERENCE);

  const bug377307 = 1172950238000;
  file.lastModifiedTime = bug377307;

  diff = Math.abs(file.lastModifiedTime - bug377307);
  Assert.ok(diff < MAX_TIME_DIFFERENCE);

  await sleep(1000);

  file.lastModifiedTime = 0;
  Assert.greater(
    file.lastModifiedTime,
    now,
    "Setting lastModifiedTime to 0 should set it to current date and time"
  );

  file.remove(true);
});

add_task(async function test_lastAccessedTime() {
  const file = do_get_profile();

  file.append("test-atime");
  if (file.exists()) {
    file.remove(true);
  }

  const now = Date.now();
  file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);
  Assert.ok(file.exists());

  const mtime = file.lastModifiedTime;

  // Modification time may be out by up to 2 seconds on FAT filesystems. Test
  // with a bit of leeway, close enough probably means it is correct.
  let diff = Math.abs(file.lastModifiedTime - now);
  Assert.ok(diff < MAX_TIME_DIFFERENCE);

  const yesterday = now - MILLIS_PER_DAY;
  file.lastAccessedTime = yesterday;

  diff = Math.abs(file.lastAccessedTime - yesterday);
  Assert.ok(diff < MAX_TIME_DIFFERENCE, `${diff} < ${MAX_TIME_DIFFERENCE}`);
  Assert.equal(
    file.lastModifiedTime,
    mtime,
    "Setting lastAccessedTime should not set lastModifiedTime"
  );

  const tomorrow = now + MILLIS_PER_DAY;
  file.lastAccessedTime = tomorrow;

  diff = Math.abs(file.lastAccessedTime - tomorrow);
  Assert.ok(diff < MAX_TIME_DIFFERENCE);

  const bug377307 = 1172950238000;
  file.lastAccessedTime = bug377307;

  diff = Math.abs(file.lastAccessedTime - bug377307);
  Assert.ok(diff < MAX_TIME_DIFFERENCE);

  await sleep(1000);

  file.lastAccessedTime = 0;
  Assert.greater(
    file.lastAccessedTime,
    now,
    "Setting lastAccessedTime to 0 should set it to the current date and time"
  );
});

// Tests that changing a directory's modification time is possible
add_task(function test_directory_modification_time() {
  var dir = do_get_profile();
  dir.append("testdir");

  // Should never happen but get rid of it anyway
  if (dir.exists()) {
    dir.remove(true);
  }

  var now = Date.now();
  dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  Assert.ok(dir.exists());

  // Modification time may be out by up to 2 seconds on FAT filesystems. Test
  // with a bit of leeway, close enough probably means it is correct.
  var diff = Math.abs(dir.lastModifiedTime - now);
  Assert.ok(diff < MAX_TIME_DIFFERENCE);

  var yesterday = now - MILLIS_PER_DAY;
  dir.lastModifiedTime = yesterday;

  diff = Math.abs(dir.lastModifiedTime - yesterday);
  Assert.ok(diff < MAX_TIME_DIFFERENCE);

  var tomorrow = now - MILLIS_PER_DAY;
  dir.lastModifiedTime = tomorrow;

  diff = Math.abs(dir.lastModifiedTime - tomorrow);
  Assert.ok(diff < MAX_TIME_DIFFERENCE);

  dir.remove(true);
});

add_task(function test_diskSpaceAvailable() {
  let file = do_get_profile();
  file.QueryInterface(Ci.nsIFile);

  let bytes = file.diskSpaceAvailable;
  Assert.ok(bytes > 0);

  file.append("testfile");
  if (file.exists()) {
    file.remove(true);
  }
  file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);

  bytes = file.diskSpaceAvailable;
  Assert.ok(bytes > 0);

  file.remove(true);
});

add_task(function test_diskCapacity() {
  let file = do_get_profile();
  file.QueryInterface(Ci.nsIFile);

  const startBytes = file.diskCapacity;
  Assert.ok(!!startBytes); // Not 0, undefined etc.

  file.append("testfile");
  if (file.exists()) {
    file.remove(true);
  }
  file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);

  const endBytes = file.diskCapacity;
  Assert.ok(!!endBytes); // Not 0, undefined etc.
  Assert.ok(startBytes === endBytes);

  file.remove(true);
});

add_task(
  {
    // nsIFile::CreationTime is only supported on macOS and Windows.
    skip_if: () => !["macosx", "win"].includes(AppConstants.platform),
  },
  function test_file_creation_time() {
    const file = do_get_profile();
    // If we re-use the same file name from the other tests, even if the
    // file.exists() check fails at 165, this test will likely fail due to the
    // creation time being copied over from the previous instance of the file on
    // Windows.
    file.append("testfile-creation-time");

    if (file.exists()) {
      file.remove(true);
    }

    const now = Date.now();

    file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);
    Assert.ok(file.exists());

    const creationTime = file.creationTime;
    Assert.ok(creationTime === file.lastModifiedTime);

    file.lastModifiedTime = now + MILLIS_PER_DAY;

    Assert.ok(creationTime !== file.lastModifiedTime);
    Assert.ok(creationTime === file.creationTime);

    file.remove(true);
  }
);

add_task(function test_file_append_parent() {
  const SEPARATOR = AppConstants.platform === "win" ? "\\" : "/";

  const file = do_get_profile();

  Assert.throws(
    () => file.append(".."),
    /NS_ERROR_FILE_UNRECOGNIZED_PATH/,
    `nsLocalFile::Append("..") throws`
  );

  Assert.throws(
    () => file.appendRelativePath(".."),
    /NS_ERROR_FILE_UNRECOGNIZED_PATH/,
    `nsLocalFile::AppendRelativePath("..") throws`
  );

  Assert.throws(
    () => file.appendRelativePath(`foo${SEPARATOR}..${SEPARATOR}baz`),
    /NS_ERROR_FILE_UNRECOGNIZED_PATH/,
    `nsLocalFile::AppendRelativePath(path) fails when path contains ".."`
  );
});
