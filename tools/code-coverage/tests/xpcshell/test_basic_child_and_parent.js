/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  do_load_child_test_harness();
  do_test_pending();

  sendCommand("let v = 'test';", async function() {
      let codeCoverage = Cc["@mozilla.org/tools/code-coverage;1"].getService(Ci.nsICodeCoverage);

      await codeCoverage.flushCounters();

      do_test_finished();
  });
}
