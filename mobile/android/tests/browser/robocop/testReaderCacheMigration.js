/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/ReaderMode.jsm");

var java = new JavaBridge(this);

do_register_cleanup(() => {
  java.disconnect();
});
do_test_pending();

function check_hashed_path_matches(url, hashedPath) {
  var jsHashedPath = ReaderMode._toHashedPath(url);
  do_check_eq(hashedPath, jsHashedPath);
}

function finish_test() {
  do_test_finished();
}