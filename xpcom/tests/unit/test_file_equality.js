/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var LocalFile = CC("@mozilla.org/file/local;1", "nsIFile", "initWithPath");

Cu.import("resource://gre/modules/Services.jsm");

function run_test() {
  test_normalized_vs_non_normalized();
}

function test_normalized_vs_non_normalized() {
  // get a directory that exists on all platforms
  var tmp1 = Services.dirsvc.get("TmpD", Ci.nsIFile);
  var exists = tmp1.exists();
  Assert.ok(exists);
  if (!exists)
    return;

  // the test logic below assumes we're starting with a normalized path, but the
  // default location on macos is a symbolic link, so resolve it before starting
  tmp1.normalize();

  // this has the same exact path as tmp1, it should equal tmp1
  var tmp2 = new LocalFile(tmp1.path);
  Assert.ok(tmp1.equals(tmp2));

  // this is a non-normalized version of tmp1, it should not equal tmp1
  tmp2.appendRelativePath(".");
  Assert.ok(!tmp1.equals(tmp2));

  // normalize and make sure they are equivalent again
  tmp2.normalize();
  Assert.ok(tmp1.equals(tmp2));
}
