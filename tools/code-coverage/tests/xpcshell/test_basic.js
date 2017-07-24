/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  Assert.ok("@mozilla.org/tools/code-coverage;1" in Cc);

  let codeCoverageCc = Cc["@mozilla.org/tools/code-coverage;1"];
  Assert.ok(!!codeCoverageCc);

  let codeCoverage = codeCoverageCc.getService(Ci.nsICodeCoverage);
  Assert.ok(!!codeCoverage);

  codeCoverage.dumpCounters();

  codeCoverage.resetCounters();
}
