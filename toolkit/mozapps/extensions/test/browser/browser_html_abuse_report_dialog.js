/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

loadTestSubscript("head_abuse_report.js");

add_task(async function setup() {
  await AbuseReportTestUtils.setup();

  // Run abuse reports tests in the "dialog window" mode.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.abuseReport.openDialog", true]],
  });
});

// Add all the test tasks shared with browser_html_abuse_report.js.
addCommonAbuseReportTestTasks();
