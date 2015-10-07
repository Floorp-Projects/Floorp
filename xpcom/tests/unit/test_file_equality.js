/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cr = Components.results;
const Ci = Components.interfaces;

const CC = Components.Constructor;
var LocalFile = CC("@mozilla.org/file/local;1", "nsILocalFile", "initWithPath");

function run_test()
{
  test_normalized_vs_non_normalized();
}

function test_normalized_vs_non_normalized()
{
  // get a directory that exists on all platforms
  var dirProvider = Components.classes["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
  var tmp1 = dirProvider.get("TmpD", Ci.nsILocalFile);
  var exists = tmp1.exists();
  do_check_true(exists);
  if (!exists)
    return;

  // this has the same exact path as tmp1, it should equal tmp1
  var tmp2 = new LocalFile(tmp1.path);
  do_check_true(tmp1.equals(tmp2));

  // this is a non-normalized version of tmp1, it should not equal tmp1
  tmp2.appendRelativePath(".");
  do_check_false(tmp1.equals(tmp2));

  // normalize and make sure they are equivalent again
  tmp2.normalize();
  do_check_true(tmp1.equals(tmp2));
}
