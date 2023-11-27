/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

loadTestSubscript("head_abuse_report.js");

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "extensions.abuseReport.amoFormURL",
        "https://example.org/%LOCALE%/%APP%/feedback/addon/%addonID%/",
      ],
    ],
  });

  // Explicitly flip the amoFormEnabled pref on builds where the pref is
  // expected to not be set to true by default.
  if (AppConstants.MOZ_APP_NAME != "firefox") {
    await SpecialPowers.pushPrefEnv({
      set: [["extensions.abuseReport.amoFormEnabled", true]],
    });
  }

  const { AbuseReporter } = ChromeUtils.importESModule(
    "resource://gre/modules/AbuseReporter.sys.mjs"
  );

  Assert.equal(
    AbuseReporter.amoFormEnabled,
    true,
    "Expect AMO abuse report form to be enabled"
  );

  // Setting up MockProvider to mock various addon types
  // as installed.
  await AbuseReportTestUtils.setup();
});

add_task(async function test_opens_amo_form_in_a_tab() {
  await openAboutAddons();

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

add_task(async function test_report_button_shown_on_dictionary_addons() {
  await openAboutAddons("dictionary");
  await AbuseReportTestUtils.assertReportActionShown(
    gManagerWindow,
    EXT_DICTIONARY_ADDON_ID
  );
  await closeAboutAddons();
});

add_task(async function test_report_action_hidden_on_langpack_addons() {
  await openAboutAddons("locale");
  await AbuseReportTestUtils.assertReportActionHidden(
    gManagerWindow,
    EXT_LANGPACK_ADDON_ID
  );
  await closeAboutAddons();
});
