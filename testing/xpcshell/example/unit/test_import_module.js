/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from import_module.jsm */
/* import-globals-from import_sub_module.jsm */

/**
 * Ensures that tests can import a module in the same folder through:
 * Components.utils.import("resource://test/module.jsm");
 */

function run_test() {
  do_check_true(typeof(this["MODULE_IMPORTED"]) == "undefined");
  do_check_true(typeof(this["MODULE_URI"]) == "undefined");
  let uri = "resource://test/import_module.jsm";
  Components.utils.import(uri);
  do_check_true(MODULE_URI == uri);
  do_check_true(MODULE_IMPORTED);
  do_check_true(SUBMODULE_IMPORTED);
  do_check_true(same_scope);
  do_check_true(SUBMODULE_IMPORTED_TO_SCOPE);
}
