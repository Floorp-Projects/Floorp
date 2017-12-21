/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var subscriptLoaded = false;

function run_test() {
  var lf = do_get_file("file.txt");
  Assert.ok(lf.exists());
  Assert.ok(lf.isFile());
  // check that allowNonexistent works
  lf = do_get_file("file.txt.notfound", true);
  Assert.ok(!lf.exists());
  // check that we can get a file from a subdirectory
  lf = do_get_file("subdir/file.txt");
  Assert.ok(lf.exists());
  Assert.ok(lf.isFile());
  // and that we can get a handle to a directory itself
  lf = do_get_file("subdir/");
  Assert.ok(lf.exists());
  Assert.ok(lf.isDirectory());
  // check that we can go up a level
  lf = do_get_file("..");
  Assert.ok(lf.exists());
  lf.append("unit");
  lf.append("file.txt");
  Assert.ok(lf.exists());
  // check that do_get_cwd works
  lf = do_get_cwd();
  Assert.ok(lf.exists());
  Assert.ok(lf.isDirectory());
}
