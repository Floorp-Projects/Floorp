/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../../../../browser/base/content/test/webextensions/head.js */
loadTestSubscript(
  "../../../../../browser/base/content/test/webextensions/head.js"
);

const { BuiltInThemes } = ChromeUtils.importESModule(
  "resource:///modules/BuiltInThemes.sys.mjs"
);

AddonTestUtils.initMochitest(this);

const server = AddonTestUtils.createHttpServer();

const SERVER_BASE_URL = `http://localhost:${server.identity.primaryPort}`;
const EXPIRED_COLORWAY_THEME_ID1 = "2022red-colorway@mozilla.org";
const EXPIRED_COLORWAY_THEME_ID2 = "2022orange-colorway@mozilla.org";
const ICON_SVG = `
  <svg width="63" height="62" viewBox="0 0 63 62" fill="none" xmlns="http://www.w3.org/2000/svg">
    <circle cx="31.5" cy="31" r="31" fill="url(#paint0_linear)"/>
    <defs>
      <linearGradient id="paint0_linear" x1="44.4829" y1="19" x2="10.4829" y2="53" gradientUnits="userSpaceOnUse">
        <stop stop-color="hsl(147, 94%, 25%)"/>
        <stop offset="1" stop-color="hsl(146, 38%, 49%)"/>
      </linearGradient>
    </defs>
  </svg>
`;

AddonTestUtils.registerJSON(server, "/updates.json", {
  addons: {
    [EXPIRED_COLORWAY_THEME_ID1]: {
      updates: [
        {
          version: "2.0.0",
          update_link: `${SERVER_BASE_URL}/${EXPIRED_COLORWAY_THEME_ID1}.xpi`,
        },
      ],
    },
    [EXPIRED_COLORWAY_THEME_ID2]: {
      updates: [
        {
          version: "3.0.0",
          update_link: `${SERVER_BASE_URL}/${EXPIRED_COLORWAY_THEME_ID2}.xpi`,
        },
      ],
    },
  },
});

const createMockThemeManifest = (id, version) => ({
  name: `Mock theme ${id} ${version}`,
  author: "Mozilla",
  version,
  icons: { 32: "icon.svg" },
  theme: {
    colors: {
      toolbar: "red",
    },
  },
  browser_specific_settings: {
    gecko: { id },
  },
});

function createWebExtensionFile(id, version) {
  return AddonTestUtils.createTempWebExtensionFile({
    files: { "icon.svg": ICON_SVG },
    manifest: createMockThemeManifest(id, version),
  });
}

let expiredThemeUpdate1 = createWebExtensionFile(
  EXPIRED_COLORWAY_THEME_ID1,
  "2.0.0"
);
let expiredThemeUpdate2 = createWebExtensionFile(
  EXPIRED_COLORWAY_THEME_ID2,
  "3.0.0"
);

server.registerFile(`/${EXPIRED_COLORWAY_THEME_ID1}.xpi`, expiredThemeUpdate1);
server.registerFile(`/${EXPIRED_COLORWAY_THEME_ID2}.xpi`, expiredThemeUpdate2);

const goBack = async win => {
  let loaded = waitForViewLoad(win);
  let backButton = win.document.querySelector(".back-button");
  ok(!backButton.disabled, "back button is enabled");
  backButton.click();
  await loaded;
};

const assertAddonCardFound = (win, { addonId, expectColorwayBuiltIn }) => {
  const msg = expectColorwayBuiltIn
    ? `Found addon card for colorway builtin ${addonId}`
    : `Found addon card for migrated colorway ${addonId}`;

  Assert.equal(
    getAddonCard(win, addonId)?.addon.isBuiltinColorwayTheme,
    expectColorwayBuiltIn,
    msg
  );
};

const assertDetailView = async (win, { addonId, expectThemeName }) => {
  let loadedDetailView = waitForViewLoad(win);
  await gBrowser.ownerGlobal.promiseDocumentFlushed(() => {});
  const themeCard = getAddonCard(win, addonId);
  // Ensure that we send a click on the control that is accessible (while a
  // mouse user could also activate a card by clicking on the entire container):
  const themeCardLink = themeCard.querySelector(".addon-name-link");
  themeCardLink.click();
  await loadedDetailView;
  Assert.equal(
    themeCard.querySelector(".addon-name")?.textContent,
    expectThemeName,
    `Got the expected addon name in the addon details for ${addonId}`
  );
};

async function test_update_expired_colorways_builtins() {
  // Set expired theme as a retained colorway theme
  const retainedThemePrefName = "browser.theme.retainedExpiredThemes";
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_UPDATEURL, `${SERVER_BASE_URL}/updates.json`],
      ["extensions.checkUpdateSecurity", false],
      ["browser.theme.colorway-migration", true],
      [
        retainedThemePrefName,
        JSON.stringify([
          EXPIRED_COLORWAY_THEME_ID1,
          EXPIRED_COLORWAY_THEME_ID2,
        ]),
      ],
    ],
  });

  await BuiltInThemes.ensureBuiltInThemes();
  async function uninstallTestAddons() {
    for (const addonId of [
      EXPIRED_COLORWAY_THEME_ID1,
      EXPIRED_COLORWAY_THEME_ID2,
    ]) {
      info(`Uninstalling test theme ${addonId}`);
      let addon = await AddonManager.getAddonByID(addonId);
      await addon?.uninstall();
    }
  }
  registerCleanupFunction(uninstallTestAddons);

  const expiredAddon1 = await AddonManager.getAddonByID(
    EXPIRED_COLORWAY_THEME_ID1
  );
  const expiredAddon2 = await AddonManager.getAddonByID(
    EXPIRED_COLORWAY_THEME_ID2
  );
  await expiredAddon2.disable();
  await expiredAddon1.enable();

  info("Open about:addons theme list view");
  let win = await loadInitialView("theme");

  assertAddonCardFound(win, {
    addonId: EXPIRED_COLORWAY_THEME_ID1,
    expectColorwayBuiltIn: true,
  });
  assertAddonCardFound(win, {
    addonId: EXPIRED_COLORWAY_THEME_ID2,
    expectColorwayBuiltIn: true,
  });

  info("Trigger addon update check");
  const promiseInstallsEnded = Promise.all([
    AddonTestUtils.promiseInstallEvent(
      "onInstallEnded",
      install => install.addon.id === EXPIRED_COLORWAY_THEME_ID1
    ),
    AddonTestUtils.promiseInstallEvent(
      "onInstallEnded",
      install => install.addon.id === EXPIRED_COLORWAY_THEME_ID1
    ),
  ]);
  // Wait for active theme to also execute the update bootstrap method.
  let promiseUpdatedAddon1 = waitForUpdate(expiredAddon1);
  triggerPageOptionsAction(win, "check-for-updates");

  info("Wait for addon update to be completed");
  await Promise.all([promiseUpdatedAddon1, promiseInstallsEnded]);

  info("Verify theme list view addon cards");
  assertAddonCardFound(win, {
    addonId: EXPIRED_COLORWAY_THEME_ID1,
    expectColorwayBuiltIn: false,
  });
  assertAddonCardFound(win, {
    addonId: EXPIRED_COLORWAY_THEME_ID2,
    expectColorwayBuiltIn: false,
  });

  info(`Switch to detail view for theme ${EXPIRED_COLORWAY_THEME_ID1}`);
  await assertDetailView(win, {
    addonId: EXPIRED_COLORWAY_THEME_ID1,
    expectThemeName: `Mock theme ${EXPIRED_COLORWAY_THEME_ID1} 2.0.0`,
  });

  info("Switch back to list view");
  await goBack(win);
  assertAddonCardFound(win, {
    addonId: EXPIRED_COLORWAY_THEME_ID1,
    expectColorwayBuiltIn: false,
  });
  assertAddonCardFound(win, {
    addonId: EXPIRED_COLORWAY_THEME_ID2,
    expectColorwayBuiltIn: false,
  });

  info(`Switch to detail view for theme ${EXPIRED_COLORWAY_THEME_ID2}`);
  await assertDetailView(win, {
    addonId: EXPIRED_COLORWAY_THEME_ID2,
    expectThemeName: `Mock theme ${EXPIRED_COLORWAY_THEME_ID2} 3.0.0`,
  });

  info("Switch back to list view");
  await goBack(win);
  assertAddonCardFound(win, {
    addonId: EXPIRED_COLORWAY_THEME_ID1,
    expectColorwayBuiltIn: false,
  });
  assertAddonCardFound(win, {
    addonId: EXPIRED_COLORWAY_THEME_ID2,
    expectColorwayBuiltIn: false,
  });

  Assert.deepEqual(
    JSON.parse(
      Services.prefs.getStringPref("browser.theme.retainedExpiredThemes")
    ),
    [],
    "Migrated colorways theme have been removed from the retainedExpiredThemes pref"
  );

  await closeView(win);
  await uninstallTestAddons();

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_colorways_builtin_theme_migration() {
  await test_update_expired_colorways_builtins();
});

add_task(
  async function test_colorways_builtin_theme_migration_on_disabledAutoUpdates() {
    await SpecialPowers.pushPrefEnv({
      set: [["extensions.update.autoUpdateDefault", false]],
    });

    await test_update_expired_colorways_builtins();

    await SpecialPowers.popPrefEnv();
  }
);
