/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  var clClass = Components.classes["@mozilla.org/toolkit/command-line;1"];
  var commandLine = clClass.createInstance();
  var urlFile = do_get_file("../unit/data/test_bug410156.url");
  var uri = commandLine.resolveURI(urlFile.path);
  do_check_eq(uri.spec, "http://www.bug410156.com/");
}
