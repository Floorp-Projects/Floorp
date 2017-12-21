/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* This tests that do_get_tempdir returns a directory that we can write to. */

var Ci = Components.interfaces;

function run_test() {
  let tmpd = do_get_tempdir();
  Assert.ok(tmpd.exists());
  tmpd.append("testfile");
  tmpd.create(Ci.nsIFile.NORMAL_FILE_TYPE, 600);
  Assert.ok(tmpd.exists());
}
