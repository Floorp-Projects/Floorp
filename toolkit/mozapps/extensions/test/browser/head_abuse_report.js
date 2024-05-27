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
const EXT_SYSTEM_ADDON_ID = "test-system-addon@mochi.test";

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
        id: EXT_SYSTEM_ADDON_ID,
        name: "This is a system addon",
        version: "1.1",
        creator: { name: "creator", url: "http://example.com/creator" },
        type: "extension",
        isSystem: true,
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
