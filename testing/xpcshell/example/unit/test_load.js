/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var subscriptLoaded = false;

function run_test() {
  load("load_subscript.js");
  do_check_true(subscriptLoaded);
  subscriptLoaded = false;
  try {
    load("file_that_does_not_exist.js");
    subscriptLoaded = true;
  }
  catch (ex) {
    do_check_eq(ex.message.substring(0,16), "cannot open file");
  }
  do_check_false(subscriptLoaded, "load() should throw an error");
}
