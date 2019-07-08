/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
Services.prefs.setBoolPref("toolkit.osfile.test.syslib_necessary", false);
// We don't need libc/kernel32.dll for this test

var ImportWin = {};
var ImportUnix = {};
ChromeUtils.import("resource://gre/modules/osfile/ospath_win.jsm", ImportWin);
ChromeUtils.import("resource://gre/modules/osfile/ospath_unix.jsm", ImportUnix);

var Win = ImportWin;
var Unix = ImportUnix;

function do_check_fail(f) {
  try {
    let result = f();
    info("Failed do_check_fail: " + result);
    Assert.ok(false);
  } catch (ex) {
    Assert.ok(true);
  }
}

function run_test() {
  info("Testing Windows paths");

  info("Backslash-separated, no drive");
  Assert.equal(Win.basename("a\\b"), "b");
  Assert.equal(Win.basename("a\\b\\"), "");
  Assert.equal(Win.basename("abc"), "abc");
  Assert.equal(Win.dirname("a\\b"), "a");
  Assert.equal(Win.dirname("a\\b\\"), "a\\b");
  Assert.equal(Win.dirname("a\\\\\\\\b"), "a");
  Assert.equal(Win.dirname("abc"), ".");
  Assert.equal(Win.normalize("\\a\\b\\c"), "\\a\\b\\c");
  Assert.equal(Win.normalize("\\a\\b\\\\\\\\c"), "\\a\\b\\c");
  Assert.equal(Win.normalize("\\a\\b\\c\\\\\\"), "\\a\\b\\c");
  Assert.equal(Win.normalize("\\a\\b\\c\\..\\..\\..\\d\\e\\f"), "\\d\\e\\f");
  Assert.equal(Win.normalize("a\\b\\c\\..\\..\\..\\d\\e\\f"), "d\\e\\f");
  do_check_fail(() => Win.normalize("\\a\\b\\c\\..\\..\\..\\..\\d\\e\\f"));

  Assert.equal(
    Win.join("\\tmp", "foo", "bar"),
    "\\tmp\\foo\\bar",
    "join \\tmp,foo,bar"
  );
  Assert.equal(
    Win.join("\\tmp", "\\foo", "bar"),
    "\\foo\\bar",
    "join \\tmp,\\foo,bar"
  );
  Assert.equal(Win.winGetDrive("\\tmp"), null);
  Assert.equal(Win.winGetDrive("\\tmp\\a\\b\\c\\d\\e"), null);
  Assert.equal(Win.winGetDrive("\\"), null);

  info("Backslash-separated, with a drive");
  Assert.equal(Win.basename("c:a\\b"), "b");
  Assert.equal(Win.basename("c:a\\b\\"), "");
  Assert.equal(Win.basename("c:abc"), "abc");
  Assert.equal(Win.dirname("c:a\\b"), "c:a");
  Assert.equal(Win.dirname("c:a\\b\\"), "c:a\\b");
  Assert.equal(Win.dirname("c:a\\\\\\\\b"), "c:a");
  Assert.equal(Win.dirname("c:abc"), "c:");
  let options = {
    winNoDrive: true,
  };
  Assert.equal(Win.dirname("c:a\\b", options), "a");
  Assert.equal(Win.dirname("c:a\\b\\", options), "a\\b");
  Assert.equal(Win.dirname("c:a\\\\\\\\b", options), "a");
  Assert.equal(Win.dirname("c:abc", options), ".");
  Assert.equal(Win.join("c:", "abc"), "c:\\abc", "join c:,abc");

  Assert.equal(Win.normalize("c:"), "c:\\");
  Assert.equal(Win.normalize("c:\\"), "c:\\");
  Assert.equal(Win.normalize("c:\\a\\b\\c"), "c:\\a\\b\\c");
  Assert.equal(Win.normalize("c:\\a\\b\\\\\\\\c"), "c:\\a\\b\\c");
  Assert.equal(Win.normalize("c:\\\\\\\\a\\b\\c"), "c:\\a\\b\\c");
  Assert.equal(Win.normalize("c:\\a\\b\\c\\\\\\"), "c:\\a\\b\\c");
  Assert.equal(
    Win.normalize("c:\\a\\b\\c\\..\\..\\..\\d\\e\\f"),
    "c:\\d\\e\\f"
  );
  Assert.equal(Win.normalize("c:a\\b\\c\\..\\..\\..\\d\\e\\f"), "c:\\d\\e\\f");
  do_check_fail(() => Win.normalize("c:\\a\\b\\c\\..\\..\\..\\..\\d\\e\\f"));

  Assert.equal(Win.join("c:\\", "foo"), "c:\\foo", "join c:,foo");
  Assert.equal(
    Win.join("c:\\tmp", "foo", "bar"),
    "c:\\tmp\\foo\\bar",
    "join c:\\tmp,foo,bar"
  );
  Assert.equal(
    Win.join("c:\\tmp", "\\foo", "bar"),
    "c:\\foo\\bar",
    "join c:\\tmp,\\foo,bar"
  );
  Assert.equal(
    Win.join("c:\\tmp", "c:\\foo", "bar"),
    "c:\\foo\\bar",
    "join c:\\tmp,c:\\foo,bar"
  );
  Assert.equal(
    Win.join("c:\\tmp", "c:foo", "bar"),
    "c:\\foo\\bar",
    "join c:\\tmp,c:foo,bar"
  );
  Assert.equal(Win.winGetDrive("c:"), "c:");
  Assert.equal(Win.winGetDrive("c:\\"), "c:");
  Assert.equal(Win.winGetDrive("c:abc"), "c:");
  Assert.equal(Win.winGetDrive("c:abc\\d\\e\\f\\g"), "c:");
  Assert.equal(Win.winGetDrive("c:\\abc"), "c:");
  Assert.equal(Win.winGetDrive("c:\\abc\\d\\e\\f\\g"), "c:");

  info("Forwardslash-separated, no drive");
  Assert.equal(Win.normalize("/a/b/c"), "\\a\\b\\c");
  Assert.equal(Win.normalize("/a/b////c"), "\\a\\b\\c");
  Assert.equal(Win.normalize("/a/b/c///"), "\\a\\b\\c");
  Assert.equal(Win.normalize("/a/b/c/../../../d/e/f"), "\\d\\e\\f");
  Assert.equal(Win.normalize("a/b/c/../../../d/e/f"), "d\\e\\f");

  info("Forwardslash-separated, with a drive");
  Assert.equal(Win.normalize("c:/"), "c:\\");
  Assert.equal(Win.normalize("c:/a/b/c"), "c:\\a\\b\\c");
  Assert.equal(Win.normalize("c:/a/b////c"), "c:\\a\\b\\c");
  Assert.equal(Win.normalize("c:////a/b/c"), "c:\\a\\b\\c");
  Assert.equal(Win.normalize("c:/a/b/c///"), "c:\\a\\b\\c");
  Assert.equal(Win.normalize("c:/a/b/c/../../../d/e/f"), "c:\\d\\e\\f");
  Assert.equal(Win.normalize("c:a/b/c/../../../d/e/f"), "c:\\d\\e\\f");

  info("Backslash-separated, UNC-style");
  Assert.equal(Win.basename("\\\\a\\b"), "b");
  Assert.equal(Win.basename("\\\\a\\b\\"), "");
  Assert.equal(Win.basename("\\\\abc"), "");
  Assert.equal(Win.dirname("\\\\a\\b"), "\\\\a");
  Assert.equal(Win.dirname("\\\\a\\b\\"), "\\\\a\\b");
  Assert.equal(Win.dirname("\\\\a\\\\\\\\b"), "\\\\a");
  Assert.equal(Win.dirname("\\\\abc"), "\\\\abc");
  Assert.equal(Win.normalize("\\\\a\\b\\c"), "\\\\a\\b\\c");
  Assert.equal(Win.normalize("\\\\a\\b\\\\\\\\c"), "\\\\a\\b\\c");
  Assert.equal(Win.normalize("\\\\a\\b\\c\\\\\\"), "\\\\a\\b\\c");
  Assert.equal(Win.normalize("\\\\a\\b\\c\\..\\..\\d\\e\\f"), "\\\\a\\d\\e\\f");
  do_check_fail(() => Win.normalize("\\\\a\\b\\c\\..\\..\\..\\d\\e\\f"));

  Assert.equal(Win.join("\\\\a\\tmp", "foo", "bar"), "\\\\a\\tmp\\foo\\bar");
  Assert.equal(Win.join("\\\\a\\tmp", "\\foo", "bar"), "\\\\a\\foo\\bar");
  Assert.equal(Win.join("\\\\a\\tmp", "\\\\foo\\", "bar"), "\\\\foo\\bar");
  Assert.equal(Win.winGetDrive("\\\\"), null);
  Assert.equal(Win.winGetDrive("\\\\c"), "\\\\c");
  Assert.equal(Win.winGetDrive("\\\\c\\abc"), "\\\\c");

  info("Testing unix paths");
  Assert.equal(Unix.basename("a/b"), "b");
  Assert.equal(Unix.basename("a/b/"), "");
  Assert.equal(Unix.basename("abc"), "abc");
  Assert.equal(Unix.dirname("a/b"), "a");
  Assert.equal(Unix.dirname("a/b/"), "a/b");
  Assert.equal(Unix.dirname("a////b"), "a");
  Assert.equal(Unix.dirname("abc"), ".");
  Assert.equal(Unix.normalize("/a/b/c"), "/a/b/c");
  Assert.equal(Unix.normalize("/a/b////c"), "/a/b/c");
  Assert.equal(Unix.normalize("////a/b/c"), "/a/b/c");
  Assert.equal(Unix.normalize("/a/b/c///"), "/a/b/c");
  Assert.equal(Unix.normalize("/a/b/c/../../../d/e/f"), "/d/e/f");
  Assert.equal(Unix.normalize("a/b/c/../../../d/e/f"), "d/e/f");
  do_check_fail(() => Unix.normalize("/a/b/c/../../../../d/e/f"));

  Assert.equal(
    Unix.join("/tmp", "foo", "bar"),
    "/tmp/foo/bar",
    "join /tmp,foo,bar"
  );
  Assert.equal(
    Unix.join("/tmp", "/foo", "bar"),
    "/foo/bar",
    "join /tmp,/foo,bar"
  );

  info("Testing the presence of ospath.jsm");
  let Scope = {};
  try {
    ChromeUtils.import("resource://gre/modules/osfile/ospath.jsm", Scope);
  } catch (ex) {
    // Can't load ospath
  }
  Assert.ok(!!Scope.basename);
}
