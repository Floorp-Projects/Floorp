/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/Services.jsm", this);
Services.prefs.setBoolPref("toolkit.osfile.test.syslib_necessary", false);
  // We don't need libc/kernel32.dll for this test

let ImportWin = {};
let ImportUnix = {};
Components.utils.import("resource://gre/modules/osfile/ospath_win_back.jsm", ImportWin);
Components.utils.import("resource://gre/modules/osfile/ospath_unix_back.jsm", ImportUnix);

let Win = ImportWin.OS;
let Unix = ImportUnix.OS;

function do_check_fail(f)
{
  try {
    let result = f();
    do_print("Failed do_check_fail: " + result);
    do_check_true(false);
  } catch (ex) {
    do_check_true(true);
  }
};

function run_test()
{
  do_print("Testing Windows paths");

  do_print("Backslash-separated, no drive");
  do_check_eq(Win.Path.basename("a\\b"), "b");
  do_check_eq(Win.Path.basename("a\\b\\"), "");
  do_check_eq(Win.Path.basename("abc"), "abc");
  do_check_eq(Win.Path.dirname("a\\b"), "a");
  do_check_eq(Win.Path.dirname("a\\b\\"), "a\\b");
  do_check_eq(Win.Path.dirname("a\\\\\\\\b"), "a");
  do_check_eq(Win.Path.dirname("abc"), ".");
  do_check_eq(Win.Path.normalize("\\a\\b\\c"), "\\a\\b\\c");
  do_check_eq(Win.Path.normalize("\\a\\b\\\\\\\\c"), "\\a\\b\\c");
  do_check_eq(Win.Path.normalize("\\a\\b\\c\\\\\\"), "\\a\\b\\c");
  do_check_eq(Win.Path.normalize("\\a\\b\\c\\..\\..\\..\\d\\e\\f"), "\\d\\e\\f");
  do_check_eq(Win.Path.normalize("a\\b\\c\\..\\..\\..\\d\\e\\f"), "d\\e\\f");
  do_check_fail(function() Win.Path.normalize("\\a\\b\\c\\..\\..\\..\\..\\d\\e\\f"));

  do_check_eq(Win.Path.join("\\tmp", "foo", "bar"), "\\tmp\\foo\\bar", "join \\tmp,foo,bar");
  do_check_eq(Win.Path.join("\\tmp", "\\foo", "bar"), "\\foo\\bar", "join \\tmp,\\foo,bar");
  do_check_eq(Win.Path.winGetDrive("\\tmp"), null);
  do_check_eq(Win.Path.winGetDrive("\\tmp\\a\\b\\c\\d\\e"), null);
  do_check_eq(Win.Path.winGetDrive("\\"), null);


  do_print("Backslash-separated, with a drive");
  do_check_eq(Win.Path.basename("c:a\\b"), "b");
  do_check_eq(Win.Path.basename("c:a\\b\\"), "");
  do_check_eq(Win.Path.basename("c:abc"), "abc");
  do_check_eq(Win.Path.dirname("c:a\\b"), "c:a");
  do_check_eq(Win.Path.dirname("c:a\\b\\"), "c:a\\b");
  do_check_eq(Win.Path.dirname("c:a\\\\\\\\b"), "c:a");
  do_check_eq(Win.Path.dirname("c:abc"), "c:");
  let options = {
    winNoDrive: true
  };
  do_check_eq(Win.Path.dirname("c:a\\b", options), "a");
  do_check_eq(Win.Path.dirname("c:a\\b\\", options), "a\\b");
  do_check_eq(Win.Path.dirname("c:a\\\\\\\\b", options), "a");
  do_check_eq(Win.Path.dirname("c:abc", options), ".");

  do_check_eq(Win.Path.normalize("c:\\a\\b\\c"), "c:\\a\\b\\c");
  do_check_eq(Win.Path.normalize("c:\\a\\b\\\\\\\\c"), "c:\\a\\b\\c");
  do_check_eq(Win.Path.normalize("c:\\\\\\\\a\\b\\c"), "c:\\a\\b\\c");
  do_check_eq(Win.Path.normalize("c:\\a\\b\\c\\\\\\"), "c:\\a\\b\\c");
  do_check_eq(Win.Path.normalize("c:\\a\\b\\c\\..\\..\\..\\d\\e\\f"), "c:\\d\\e\\f");
  do_check_eq(Win.Path.normalize("c:a\\b\\c\\..\\..\\..\\d\\e\\f"), "c:d\\e\\f");
  do_check_fail(function() Win.Path.normalize("c:\\a\\b\\c\\..\\..\\..\\..\\d\\e\\f"));

  do_check_eq(Win.Path.join("c:\\tmp", "foo", "bar"), "c:\\tmp\\foo\\bar", "join c:\\tmp,foo,bar");
  do_check_eq(Win.Path.join("c:\\tmp", "\\foo", "bar"), "c:\\foo\\bar", "join c:\\tmp,\\foo,bar");
  do_check_eq(Win.Path.join("c:\\tmp", "c:\\foo", "bar"), "c:\\foo\\bar", "join c:\\tmp,c:\\foo,bar");
  do_check_eq(Win.Path.join("c:\\tmp", "c:foo", "bar"), "c:foo\\bar", "join c:\\tmp,c:foo,bar");
  do_check_eq(Win.Path.winGetDrive("c:"), "c:");
  do_check_eq(Win.Path.winGetDrive("c:\\"), "c:");
  do_check_eq(Win.Path.winGetDrive("c:abc"), "c:");
  do_check_eq(Win.Path.winGetDrive("c:abc\\d\\e\\f\\g"), "c:");
  do_check_eq(Win.Path.winGetDrive("c:\\abc"), "c:");
  do_check_eq(Win.Path.winGetDrive("c:\\abc\\d\\e\\f\\g"), "c:");

  do_print("Backslash-separated, UNC-style");
  do_check_eq(Win.Path.basename("\\\\a\\b"), "b");
  do_check_eq(Win.Path.basename("\\\\a\\b\\"), "");
  do_check_eq(Win.Path.basename("\\\\abc"), "");
  do_check_eq(Win.Path.dirname("\\\\a\\b"), "\\\\a");
  do_check_eq(Win.Path.dirname("\\\\a\\b\\"), "\\\\a\\b");
  do_check_eq(Win.Path.dirname("\\\\a\\\\\\\\b"), "\\\\a");
  do_check_eq(Win.Path.dirname("\\\\abc"), "\\\\abc");
  do_check_eq(Win.Path.normalize("\\\\a\\b\\c"), "\\\\a\\b\\c");
  do_check_eq(Win.Path.normalize("\\\\a\\b\\\\\\\\c"), "\\\\a\\b\\c");
  do_check_eq(Win.Path.normalize("\\\\a\\b\\c\\\\\\"), "\\\\a\\b\\c");
  do_check_eq(Win.Path.normalize("\\\\a\\b\\c\\..\\..\\d\\e\\f"), "\\\\a\\d\\e\\f");
  do_check_fail(function() Win.Path.normalize("\\\\a\\b\\c\\..\\..\\..\\d\\e\\f"));

  do_check_eq(Win.Path.join("\\\\a\\tmp", "foo", "bar"), "\\\\a\\tmp\\foo\\bar");
  do_check_eq(Win.Path.join("\\\\a\\tmp", "\\foo", "bar"), "\\\\a\\foo\\bar");
  do_check_eq(Win.Path.join("\\\\a\\tmp", "\\\\foo\\", "bar"), "\\\\foo\\bar");
  do_check_eq(Win.Path.winGetDrive("\\\\"), null);
  do_check_eq(Win.Path.winGetDrive("\\\\c"), "\\\\c");
  do_check_eq(Win.Path.winGetDrive("\\\\c\\abc"), "\\\\c");

  do_print("Testing unix paths");
  do_check_eq(Unix.Path.basename("a/b"), "b");
  do_check_eq(Unix.Path.basename("a/b/"), "");
  do_check_eq(Unix.Path.basename("abc"), "abc");
  do_check_eq(Unix.Path.dirname("a/b"), "a");
  do_check_eq(Unix.Path.dirname("a/b/"), "a/b");
  do_check_eq(Unix.Path.dirname("a////b"), "a");
  do_check_eq(Unix.Path.dirname("abc"), ".");
  do_check_eq(Unix.Path.normalize("/a/b/c"), "/a/b/c");
  do_check_eq(Unix.Path.normalize("/a/b////c"), "/a/b/c");
  do_check_eq(Unix.Path.normalize("////a/b/c"), "/a/b/c");
  do_check_eq(Unix.Path.normalize("/a/b/c///"), "/a/b/c");
  do_check_eq(Unix.Path.normalize("/a/b/c/../../../d/e/f"), "/d/e/f");
  do_check_eq(Unix.Path.normalize("a/b/c/../../../d/e/f"), "d/e/f");
  do_check_fail(function() Unix.Path.normalize("/a/b/c/../../../../d/e/f"));

  do_check_eq(Unix.Path.join("/tmp", "foo", "bar"), "/tmp/foo/bar", "join /tmp,foo,bar");
  do_check_eq(Unix.Path.join("/tmp", "/foo", "bar"), "/foo/bar", "join /tmp,/foo,bar");
}
