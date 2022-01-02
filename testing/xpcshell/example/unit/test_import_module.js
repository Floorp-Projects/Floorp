/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from import_module.jsm */

/**
 * Ensures that tests can import a module in the same folder through:
 * Components.utils.import("resource://test/module.jsm");
 */

function run_test() {
  Assert.ok(typeof this.MODULE_IMPORTED == "undefined");
  Assert.ok(typeof this.MODULE_URI == "undefined");
  let uri = "resource://test/import_module.jsm";
  let exports = ChromeUtils.import(uri);
  Assert.ok(exports.MODULE_URI == uri);
  Assert.ok(exports.MODULE_IMPORTED);
}
