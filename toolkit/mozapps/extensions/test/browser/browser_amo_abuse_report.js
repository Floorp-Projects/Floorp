/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

loadTestSubscript("head_abuse_report.js");

add_task(async function test_opens_amo_form_in_a_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.abuseReport.amoFormEnabled", true],
      [
        "extensions.abuseReport.amoFormURL",
        "https://example.org/%LOCALE%/%APP%/feedback/addon/%addonID%/",
      ],
    ],
  });

  await openAboutAddons();

  Assert.equal(
    gManagerWindow.ABUSE_REPORT_AMO_FORM_ENABLED,
    true,
    "Expect AMO abuse report form to be enabled by default"
  );

  const ADDON_ID = "test-ext@mochitest";
  const expectedUrl = Services.urlFormatter
    .formatURLPref("extensions.abuseReport.amoFormURL")
    .replace("%addonID%", ADDON_ID);

  const promiseWaitForAMOFormTab = BrowserTestUtils.waitForNewTab(
    gBrowser,
    expectedUrl
  );
  info("Call about:addons openAbuseReport helper function");
  gManagerWindow.openAbuseReport({ addonId: ADDON_ID });
  info(`Wait for the AMO form url ${expectedUrl} to be opened in a new tab`);
  const tab = await promiseWaitForAMOFormTab;
  Assert.equal(
    tab.linkedBrowser.currentURI.spec,
    expectedUrl,
    "The newly opened tab has the expected url"
  );
  Assert.equal(gBrowser.selectedTab, tab, "The newly opened tab is selected");

  BrowserTestUtils.removeTab(tab);
  await closeAboutAddons();
  await SpecialPowers.popPrefEnv();
});
