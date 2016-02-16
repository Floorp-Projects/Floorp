/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cr = Components.results;
var CC = Components.Constructor;
var Ci = Components.interfaces;

const MAX_TIME_DIFFERENCE = 2500;
const MILLIS_PER_DAY      = 1000 * 60 * 60 * 24;

var LocalFile = CC("@mozilla.org/file/local;1", "nsILocalFile", "initWithPath");

function run_test()
{
  test_toplevel_parent_is_null();
  test_normalize_crash_if_media_missing();
  test_file_modification_time();
  test_directory_modification_time();
  test_diskSpaceAvailable();
}

function test_toplevel_parent_is_null()
{
  try
  {
    var lf = new LocalFile("C:\\");

    // not required by API, but a property on which the implementation of
    // parent == null relies for correctness
    do_check_true(lf.path.length == 2);

    do_check_true(lf.parent === null);
  }
  catch (e)
  {
    // not Windows
    do_check_eq(e.result, Cr.NS_ERROR_FILE_UNRECOGNIZED_PATH);
  }
}

function test_normalize_crash_if_media_missing()
{
  const a="a".charCodeAt(0);
  const z="z".charCodeAt(0);
  for (var i = a; i <= z; ++i)
  {
    try
    {
      LocalFile(String.fromCharCode(i)+":.\\test").normalize();
    }
    catch (e)
    {
    }
  }
}

// Tests that changing a file's modification time is possible   
function test_file_modification_time()
{
  var file = do_get_profile();
  file.append("testfile");

  // Should never happen but get rid of it anyway
  if (file.exists())
    file.remove(true);

  var now = Date.now();
  file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);
  do_check_true(file.exists());

  // Modification time may be out by up to 2 seconds on FAT filesystems. Test
  // with a bit of leeway, close enough probably means it is correct.
  var diff = Math.abs(file.lastModifiedTime - now);
  do_check_true(diff < MAX_TIME_DIFFERENCE);

  var yesterday = now - MILLIS_PER_DAY;
  file.lastModifiedTime = yesterday;

  diff = Math.abs(file.lastModifiedTime - yesterday);
  do_check_true(diff < MAX_TIME_DIFFERENCE);

  var tomorrow = now - MILLIS_PER_DAY;
  file.lastModifiedTime = tomorrow;

  diff = Math.abs(file.lastModifiedTime - tomorrow);
  do_check_true(diff < MAX_TIME_DIFFERENCE);

  var bug377307 = 1172950238000;
  file.lastModifiedTime = bug377307;

  diff = Math.abs(file.lastModifiedTime - bug377307);
  do_check_true(diff < MAX_TIME_DIFFERENCE);

  file.remove(true);
}

// Tests that changing a directory's modification time is possible   
function test_directory_modification_time()
{
  var dir = do_get_profile();
  dir.append("testdir");

  // Should never happen but get rid of it anyway
  if (dir.exists())
    dir.remove(true);

  var now = Date.now();
  dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  do_check_true(dir.exists());

  // Modification time may be out by up to 2 seconds on FAT filesystems. Test
  // with a bit of leeway, close enough probably means it is correct.
  var diff = Math.abs(dir.lastModifiedTime - now);
  do_check_true(diff < MAX_TIME_DIFFERENCE);

  var yesterday = now - MILLIS_PER_DAY;
  dir.lastModifiedTime = yesterday;

  diff = Math.abs(dir.lastModifiedTime - yesterday);
  do_check_true(diff < MAX_TIME_DIFFERENCE);

  var tomorrow = now - MILLIS_PER_DAY;
  dir.lastModifiedTime = tomorrow;

  diff = Math.abs(dir.lastModifiedTime - tomorrow);
  do_check_true(diff < MAX_TIME_DIFFERENCE);

  dir.remove(true);
}

function test_diskSpaceAvailable()
{
  let file = do_get_profile();
  file.QueryInterface(Ci.nsILocalFile);

  let bytes = file.diskSpaceAvailable;
  do_check_true(bytes > 0);

  file.append("testfile");
  if (file.exists())
    file.remove(true);
  file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);

  bytes = file.diskSpaceAvailable;
  do_check_true(bytes > 0);

  file.remove(true);
}
