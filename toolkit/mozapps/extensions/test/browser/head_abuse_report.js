/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

/* exported AbuseReportTestUtils, openAboutAddons, closeAboutAddons,
 *          gManagerWindow
 */

/* global MockProvider, loadInitialView, closeView */

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const EXT_DICTIONARY_ADDON_ID = "fake-dictionary@mochi.test";
const EXT_LANGPACK_ADDON_ID = "fake-langpack@mochi.test";
const EXT_WITH_PRIVILEGED_URL_ID = "ext-with-privileged-url@mochi.test";
const EXT_SYSTEM_ADDON_ID = "test-system-addon@mochi.test";
const EXT_UNSUPPORTED_TYPE_ADDON_ID = "report-unsupported-type@mochi.test";
const THEME_NO_UNINSTALL_ID = "theme-without-perm-can-uninstall@mochi.test";

let gManagerWindow;

AddonTestUtils.initMochitest(this);

async function openAboutAddons(type = "extension") {
  gManagerWindow = await loadInitialView(type);
}

async function closeAboutAddons() {
  if (gManagerWindow) {
    await closeView(gManagerWindow);
    gManagerWindow = null;
  }
}

const AbuseReportTestUtils = {
  _mockProvider: null,
  _mockServer: null,
  _abuseRequestHandlers: [],

  // Mock addon details API endpoint.
  amoAddonDetailsMap: new Map(),

  // Setup the test environment by setting the expected prefs and initializing
  // MockProvider.
  async setup() {
    await SpecialPowers.pushPrefEnv({
      set: [["extensions.abuseReport.enabled", true]],
    });

    this._setupMockProvider();
  },

  // Assert that the report action visibility on the addon card
  // for the given about:addons windows and extension id.
  async assertReportActionVisibility(gManagerWindow, extId, expectShown) {
    let addonCard = gManagerWindow.document.querySelector(
      `addon-list addon-card[addon-id="${extId}"]`
    );
    ok(addonCard, `Got the addon-card for the ${extId} test extension`);

    let reportButton = addonCard.querySelector("[action=report]");
    ok(reportButton, `Got the report action for ${extId}`);
    Assert.equal(
      reportButton.hidden,
      !expectShown,
      `${extId} report action should be ${expectShown ? "shown" : "hidden"}`
    );
  },

  // Assert that the report action is hidden on the addon card
  // for the given about:addons windows and extension id.
  assertReportActionHidden(gManagerWindow, extId) {
    return this.assertReportActionVisibility(gManagerWindow, extId, false);
  },

  // Assert that the report action is shown on the addon card
  // for the given about:addons windows and extension id.
  assertReportActionShown(gManagerWindow, extId) {
    return this.assertReportActionVisibility(gManagerWindow, extId, true);
  },

  // Internal helper methods.

  _setupMockProvider() {
    this._mockProvider = new MockProvider();
    this._mockProvider.createAddons([
      {
        id: THEME_NO_UNINSTALL_ID,
        name: "This theme cannot be uninstalled",
        version: "1.1",
        creator: { name: "Theme creator", url: "http://example.com/creator" },
        type: "theme",
        permissions: 0,
      },
      {
        id: EXT_WITH_PRIVILEGED_URL_ID,
        name: "This extension has an unexpected privileged creator URL",
        version: "1.1",
        creator: { name: "creator", url: "about:config" },
        type: "extension",
      },
      {
        id: EXT_SYSTEM_ADDON_ID,
        name: "This is a system addon",
        version: "1.1",
        creator: { name: "creator", url: "http://example.com/creator" },
        type: "extension",
        isSystem: true,
      },
      {
        id: EXT_UNSUPPORTED_TYPE_ADDON_ID,
        name: "This is a fake unsupported addon type",
        version: "1.1",
        type: "unsupported_addon_type",
      },
      {
        id: EXT_LANGPACK_ADDON_ID,
        name: "This is a fake langpack",
        version: "1.1",
        type: "locale",
      },
      {
        id: EXT_DICTIONARY_ADDON_ID,
        name: "This is a fake dictionary",
        version: "1.1",
        type: "dictionary",
      },
    ]);
  },
};
