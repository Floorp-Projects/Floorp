/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var subscriptLoaded = false;

function run_test() {
  var lf = do_get_file("file.txt");
  do_check_true(lf.exists());
  do_check_true(lf.isFile());
  // check that allowNonexistent works
  lf = do_get_file("file.txt.notfound", true);
  do_check_false(lf.exists());
  // check that we can get a file from a subdirectory
  lf = do_get_file("subdir/file.txt");
  do_check_true(lf.exists());
  do_check_true(lf.isFile());
  // and that we can get a handle to a directory itself
  lf = do_get_file("subdir/");
  do_check_true(lf.exists());
  do_check_true(lf.isDirectory());
  // check that we can go up a level
  lf = do_get_file("..");
  do_check_true(lf.exists());
  lf.append("unit");
  lf.append("file.txt");
  do_check_true(lf.exists());
  // check that do_get_cwd works
  lf = do_get_cwd();
  do_check_true(lf.exists());
  do_check_true(lf.isDirectory());
}
