/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  var commandLine = Cu.createCommandLine();
  var urlFile = do_get_file("../unit/data/test_bug410156.url");
  var uri = commandLine.resolveURI(urlFile.path);
  Assert.equal(uri.spec, "http://www.bug410156.com/");
}
