/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  var clClass = Components.classes["@mozilla.org/toolkit/command-line;1"];
  var commandLine = clClass.createInstance();
  do_check_true("length" in commandLine);
}
