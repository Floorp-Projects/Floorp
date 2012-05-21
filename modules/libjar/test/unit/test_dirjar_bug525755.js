/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check that we refuse to open weird files
function run_test() {
  const Cc = Components.classes;
  const Ci = Components.interfaces;
  // open a bogus file
  var file = do_get_file("/");

  var zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Ci.nsIZipReader);
  var failed = false;
  try {
    zipreader.open(file);
  } catch (e) {
    failed = true;
  }
  do_check_true(failed);
  zipreader = null;
}

