"use strict";

const { BuiltInThemes } = ChromeUtils.importESModule(
  "resource:///modules/BuiltInThemes.sys.mjs"
);
const { BuiltInThemeConfig } = ChromeUtils.importESModule(
  "resource:///modules/BuiltInThemeConfig.sys.mjs"
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

// Enable SCOPE_APPLICATION for builtin testing.
let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION;
Services.prefs.setIntPref("extensions.enabledScopes", scopes);

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

const ADDON_ID = "mock-colorway@mozilla.org";
const ADDON_ID_RETAINED = "mock-disabled-retained-colorway@mozilla.org";
const ADDON_ID_NOT_RETAINED = "mock-disabled-not-retained-colorway@mozilla.org";
const DEFAULT_THEME_ID = "default-theme@mozilla.org";
const NOT_MIGRATED_THEME = "mock-not-migrated-theme@mozilla.org";

const RETAINED_THEMES_PREF = "browser.theme.retainedExpiredThemes";
const COLORWAY_MIGRATION_PREF = "browser.theme.colorway-migration";

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
const createMockThemeManifest = (id, version) => ({
  name: "A mock colorway theme",
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

let server = createHttpServer();

const SERVER_BASE_URL = `http://localhost:${server.identity.primaryPort}`;

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);
Services.prefs.setCharPref(
  "extensions.update.background.url",
  `${SERVER_BASE_URL}/upgrade.json`
);

AddonTestUtils.registerJSON(server, "/upgrade.json", {
  addons: {
    [ADDON_ID]: {
      updates: [
        {
          version: "2.0.0",
          update_link: `${SERVER_BASE_URL}/${ADDON_ID}.xpi`,
        },
      ],
    },
    [ADDON_ID_RETAINED]: {
      updates: [
        {
          version: "3.0.0",
          update_link: `${SERVER_BASE_URL}/${ADDON_ID_RETAINED}.xpi`,
        },
      ],
    },
    // We list the test extension with addon id ADDON_ID_NOT_RETAINED here,
    // but the xpi file doesn't exist because we expect that we wouldn't
    // be checking this extension for updates, and that expected behavior
    // regresses, the test would fail either for the explicit assertion
    // (checking that we don't find an update) or because we would be trying
    // to download a file from an url that isn't going to be handled.
    [ADDON_ID_NOT_RETAINED]: {
      updates: [
        {
          version: "4.0.0",
          update_link: `${SERVER_BASE_URL}/non-existing.xpi`,
        },
      ],
    },
  },
});

function createWebExtensionFile(id, version) {
  return AddonTestUtils.createTempWebExtensionFile({
    files: { "icon.svg": ICON_SVG },
    manifest: createMockThemeManifest(id, version),
  });
}

let xpiUpdate = createWebExtensionFile(ADDON_ID, "2.0.0");
let retainedThemeUpdate = createWebExtensionFile(ADDON_ID_RETAINED, "3.0.0");

server.registerFile(`/${ADDON_ID}.xpi`, xpiUpdate);
server.registerFile(`/${ADDON_ID_RETAINED}.xpi`, retainedThemeUpdate);

function assertAddonWrapperProperties(
  addonWrapper,
  { id, version, isBuiltin, type, isBuiltinColorwayTheme, scope }
) {
  Assert.deepEqual(
    {
      id: addonWrapper.id,
      version: addonWrapper.version,
      type: addonWrapper.type,
      scope: addonWrapper.scope,
      isBuiltin: addonWrapper.isBuiltin,
      isBuiltinColorwayTheme: addonWrapper.isBuiltinColorwayTheme,
    },
    {
      id,
      version,
      type,
      scope,
      isBuiltin,
      isBuiltinColorwayTheme,
    },
    `Got expected properties on addon wrapper for "${id}"`
  );
}

function assertAddonCanUpgrade(addonWrapper, canUpgrade) {
  equal(
    !!(addonWrapper.permissions & AddonManager.PERM_CAN_UPGRADE),
    canUpgrade,
    `Expected "${addonWrapper.id}" to ${
      canUpgrade ? "have" : "not have"
    } PERM_CAN_UPGRADE AOM permission`
  );
}

function assertIsActiveThemeID(addonId) {
  equal(
    Services.prefs.getCharPref("extensions.activeThemeID"),
    addonId,
    `Expect ${addonId} to be the currently active theme`
  );
}

function assertIsExpiredTheme(addonId, expectExpired) {
  equal(
    // themeIsExpired returns undefined for themes without an expiry date,
    // normalized here to always be a boolean.
    !!BuiltInThemes.themeIsExpired(addonId),
    expectExpired,
    `Expect ${addonId} to be recognized as an expired colorway theme`
  );
}

function assertIsRetainedExpiredTheme(addonId, expectRetainedExpired) {
  equal(
    BuiltInThemes.isRetainedExpiredTheme(addonId),
    expectRetainedExpired,
    `Expect ${addonId} to be recognized as a retained expired colorway theme`
  );
}

function waitForBootstrapUpdateMethod(addonId, newVersion) {
  return new Promise(resolve => {
    function listener(_evt, { method, params }) {
      if (
        method === "update" &&
        params.id === addonId &&
        params.newVersion === newVersion
      ) {
        AddonTestUtils.off("bootstrap-method", listener);
        info(`Update bootstrap method called for ${addonId} ${newVersion}`);
        resolve();
      }
    }
    AddonTestUtils.on("bootstrap-method", listener);
  });
}

let waitForTemporaryXPIFilesRemoved;

add_setup(async () => {
  info("Creating BuiltInThemes stubs");
  const sandbox = sinon.createSandbox();
  // Restoring the mocked BuiltInThemeConfig doesn't really matter for xpcshell
  // because each test file will run in its own separate xpcshell instance,
  // but cleaning it up doesn't harm neither.
  registerCleanupFunction(() => {
    info("Restoring BuiltInThemes sandbox for cleanup");
    sandbox.restore();
    BuiltInThemes.builtInThemeMap = BuiltInThemeConfig;
  });

  // Mock BuiltInThemes builtInThemeMap.
  BuiltInThemes.builtInThemeMap = new Map();
  sandbox.stub(BuiltInThemes.builtInThemeMap, "get").callsFake(id => {
    info(`Mock BuiltInthemes.builtInThemeMap.get result for ${id}`);
    // No theme info is expected to be returned for the default-theme.
    if (id === DEFAULT_THEME_ID) {
      return undefined;
    }
    if (!id.endsWith("colorway@mozilla.org")) {
      return BuiltInThemeConfig.get(id);
    }
    let mockThemeProperties = {
      collection: "Mock expired colorway collection",
      figureUrl: "about:blank",
      expiry: new Date("1970-01-01"),
    };
    return mockThemeProperties;
  });

  // Start AOM and make sure updates are enabled.
  await AddonTestUtils.promiseStartupManager();
  AddonManager.updateEnabled = true;

  // Enable the default theme explicitly (mainly because on DevEdition builds
  // the dark theme would be the one enabled by default).
  const defaultTheme = await AddonManager.getAddonByID(DEFAULT_THEME_ID);
  await defaultTheme.enable();
  assertIsActiveThemeID(defaultTheme.id);

  const tmpFiles = new Set();
  const addonInstallListener = {
    onInstallEnded: function collectTmpFiles(install) {
      tmpFiles.add(install.file);
    },
  };
  AddonManager.addInstallListener(addonInstallListener);
  registerCleanupFunction(() => {
    AddonManager.removeInstallListener(addonInstallListener);
  });

  // Make sure all the tempfile created for the background updates have
  // been removed (otherwise AddonTestUtils cleanup function will trigger
  // intermittent test failures due to unexpected xpi files that may still
  // be found in the temporary directory).
  waitForTemporaryXPIFilesRemoved = async () => {
    info(
      "Wait for temporary xpi files created by the background updates to have been removed"
    );
    const files = Array.from(tmpFiles);
    tmpFiles.clear();
    await TestUtils.waitForCondition(async () => {
      for (const file of files) {
        if (await file.exists()) {
          return false;
        }
      }
      return true;
    }, "Wait for the temporary files created for the background updates to have been removed");
  };
});

add_task(
  {
    pref_set: [[COLORWAY_MIGRATION_PREF, false]],
  },
  async function test_colorways_migration_disabled() {
    info("Install and activate a colorway built-in test theme");

    await installBuiltinExtension(
      {
        manifest: createMockThemeManifest(ADDON_ID, "1.0.0"),
      },
      false /* waitForStartup */
    );
    const activeTheme = await AddonManager.getAddonByID(ADDON_ID);
    assertAddonWrapperProperties(activeTheme, {
      id: ADDON_ID,
      version: "1.0.0",
      type: "theme",
      scope: AddonManager.SCOPE_APPLICATION,
      isBuiltin: true,
      isBuiltinColorwayTheme: true,
    });
    const promiseThemeEnabled = AddonTestUtils.promiseAddonEvent(
      "onEnabled",
      addon => addon.id === ADDON_ID
    );
    await activeTheme.enable();
    await promiseThemeEnabled;
    ok(activeTheme.isActive, "Expect the colorways theme to be active");
    assertIsActiveThemeID(activeTheme.id);

    info("Verify that built-in colorway migration is disabled as expected");

    assertAddonCanUpgrade(activeTheme, false);

    const promiseBackgroundUpdatesFound = TestUtils.topicObserved(
      "addons-background-updates-found"
    );
    await AddonManagerPrivate.backgroundUpdateCheck();
    const [, numUpdatesFound] = await promiseBackgroundUpdatesFound;
    equal(numUpdatesFound, 0, "Expect no add-on updates to be found");

    await activeTheme.uninstall();
  }
);

add_task(
  {
    pref_set: [
      [COLORWAY_MIGRATION_PREF, true],
      [
        RETAINED_THEMES_PREF,
        JSON.stringify([ADDON_ID_RETAINED, NOT_MIGRATED_THEME]),
      ],
    ],
  },
  async function test_colorways_builtin_upgrade() {
    info("Verify default theme initially enabled");
    const defaultTheme = await AddonManager.getAddonByID(DEFAULT_THEME_ID);
    assertAddonWrapperProperties(defaultTheme, {
      id: DEFAULT_THEME_ID,
      version: defaultTheme.version,
      type: "theme",
      scope: AddonManager.SCOPE_APPLICATION,
      isBuiltin: true,
      isBuiltinColorwayTheme: false,
    });
    ok(
      defaultTheme.isActive,
      "Expect the default theme to be initially active"
    );
    assertIsActiveThemeID(defaultTheme.id);

    info("Install the non retained expired colorway test theme");
    await installBuiltinExtension(
      {
        manifest: createMockThemeManifest(ADDON_ID_NOT_RETAINED, "1.0.0"),
      },
      false /* waitForStartup */
    );
    const notRetainedTheme = await AddonManager.getAddonByID(
      ADDON_ID_NOT_RETAINED
    );
    assertAddonWrapperProperties(notRetainedTheme, {
      id: ADDON_ID_NOT_RETAINED,
      version: "1.0.0",
      type: "theme",
      scope: AddonManager.SCOPE_APPLICATION,
      isBuiltin: true,
      isBuiltinColorwayTheme: true,
    });

    info("Install the retained expired colorway test theme");
    await installBuiltinExtension(
      {
        manifest: createMockThemeManifest(ADDON_ID_RETAINED, "1.0.0"),
      },
      false /* waitForStartup */
    );
    const retainedTheme = await AddonManager.getAddonByID(ADDON_ID_RETAINED);
    assertAddonWrapperProperties(retainedTheme, {
      id: ADDON_ID_RETAINED,
      version: "1.0.0",
      type: "theme",
      scope: AddonManager.SCOPE_APPLICATION,
      isBuiltin: true,
      isBuiltinColorwayTheme: true,
    });

    info("Install the active colorway test theme");
    await installBuiltinExtension(
      {
        manifest: createMockThemeManifest(ADDON_ID, "1.0.0"),
      },
      false /* waitForStartup */
    );
    const activeTheme = await AddonManager.getAddonByID(ADDON_ID);
    assertAddonWrapperProperties(activeTheme, {
      id: ADDON_ID,
      version: "1.0.0",
      type: "theme",
      scope: AddonManager.SCOPE_APPLICATION,
      isBuiltin: true,
      isBuiltinColorwayTheme: true,
    });
    const promiseThemeEnabled = AddonTestUtils.promiseAddonEvent(
      "onEnabled",
      addon => addon.id === ADDON_ID
    );
    await activeTheme.enable();
    await promiseThemeEnabled;
    ok(activeTheme.isActive, "Expect the colorways theme to be active");
    assertIsActiveThemeID(activeTheme.id);

    info("Verify only active and retained colorways themes can be upgraded");
    assertIsActiveThemeID(activeTheme.id);
    assertIsExpiredTheme(activeTheme.id, true);
    assertIsRetainedExpiredTheme(activeTheme.id, false);

    assertIsExpiredTheme(retainedTheme.id, true);
    assertIsRetainedExpiredTheme(retainedTheme.id, true);

    assertIsExpiredTheme(notRetainedTheme.id, true);
    assertIsRetainedExpiredTheme(notRetainedTheme.id, false);

    assertIsExpiredTheme(defaultTheme.id, false);
    assertIsRetainedExpiredTheme(defaultTheme.id, false);

    assertAddonCanUpgrade(retainedTheme, true);
    assertAddonCanUpgrade(notRetainedTheme, false);
    assertAddonCanUpgrade(activeTheme, true);
    // Make sure a non-colorways built-in theme cannot check for updates.
    assertAddonCanUpgrade(defaultTheme, false);

    Assert.deepEqual(
      Services.prefs.getStringPref(RETAINED_THEMES_PREF),
      JSON.stringify([retainedTheme.id, NOT_MIGRATED_THEME]),
      `Expect the retained theme id to be listed in the ${RETAINED_THEMES_PREF} pref`
    );

    const promiseUpdatesInstalled = Promise.all([
      waitForBootstrapUpdateMethod(ADDON_ID, "2.0.0"),
      waitForBootstrapUpdateMethod(ADDON_ID_RETAINED, "3.0.0"),
    ]);

    const promiseInstallsEnded = Promise.all([
      AddonTestUtils.promiseInstallEvent(
        "onInstallEnded",
        install => install.addon.id === ADDON_ID
      ),
      AddonTestUtils.promiseInstallEvent(
        "onInstallEnded",
        install => install.addon.id === ADDON_ID_RETAINED
      ),
    ]);

    const promiseActiveThemeStartupCompleted =
      AddonTestUtils.promiseWebExtensionStartup(ADDON_ID);

    const promiseBackgroundUpdatesFound = TestUtils.topicObserved(
      "addons-background-updates-found"
    );
    await AddonManagerPrivate.backgroundUpdateCheck();
    const [, numUpdatesFound] = await promiseBackgroundUpdatesFound;
    equal(numUpdatesFound, 2, "Expect 2 add-on updates to have been found");

    info("Wait for the 2 expected updates to be completed");
    await promiseUpdatesInstalled;

    const updatedActiveTheme = await AddonManager.getAddonByID(ADDON_ID);
    assertAddonWrapperProperties(updatedActiveTheme, {
      id: ADDON_ID,
      version: "2.0.0",
      type: "theme",
      scope: AddonManager.SCOPE_PROFILE,
      isBuiltin: false,
      isBuiltinColorwayTheme: false,
    });
    // Expect the updated active theme to stay set as the currently active theme.
    assertIsActiveThemeID(updatedActiveTheme.id);

    info("Verify addon update on disabled builtin colorway theme");

    const updatedRetainedTheme = await AddonManager.getAddonByID(
      ADDON_ID_RETAINED
    );
    assertAddonWrapperProperties(updatedRetainedTheme, {
      id: ADDON_ID_RETAINED,
      version: "3.0.0",
      type: "theme",
      scope: AddonManager.SCOPE_PROFILE,
      isBuiltin: false,
      isBuiltinColorwayTheme: false,
    });
    // Expect the updated active theme to stay set as the currently active theme.
    assertIsActiveThemeID(updatedActiveTheme.id);
    ok(updatedActiveTheme.isActive, "Expect the colorways theme to be active");

    // We need to wait for the active theme startup otherwise uninstall the active
    // theme will fail to remove the xpi file because it is stil active while the
    // test is running on windows builds.
    info("Wait for the active theme to have been fully loaded");
    await promiseActiveThemeStartupCompleted;

    await promiseInstallsEnded;

    Assert.deepEqual(
      Services.prefs.getStringPref(RETAINED_THEMES_PREF),
      JSON.stringify([NOT_MIGRATED_THEME]),
      `Expect migrated retained theme to not be listed anymore in the ${RETAINED_THEMES_PREF} pref`
    );

    info(
      "uninstall test colorways themes and expect default theme to become active"
    );

    const promiseUninstalled = Promise.all([
      AddonTestUtils.promiseAddonEvent(
        "onUninstalled",
        addon => addon.id === ADDON_ID
      ),
      AddonTestUtils.promiseAddonEvent(
        "onUninstalled",
        addon => addon.id === ADDON_ID_RETAINED
      ),
      AddonTestUtils.promiseAddonEvent(
        "onUninstalled",
        addon => addon.id === ADDON_ID_NOT_RETAINED
      ),
    ]);

    const promiseDefaultThemeEnabled =
      AddonTestUtils.promiseAddonEvent("onEnabled");

    await updatedActiveTheme.uninstall();
    await updatedRetainedTheme.uninstall();
    await notRetainedTheme.uninstall();

    await promiseUninstalled;

    info("Wait for the default theme to become active");
    // Waiting explicitly for the onEnabled addon event prevents a race between
    // the test task exiting (and the AddonManager being shutdown automatically
    // as a side effect of that) and the XPIProvider trying to call the addon event
    // listeners for the default theme being enabled), which would trigger a test
    // failure after the test is existing.
    await promiseDefaultThemeEnabled;

    ok(defaultTheme.isActive, "Expect the default theme to be active");
    assertIsActiveThemeID(defaultTheme.id);

    // Wait for the temporary file to be actually removed, otherwise the hack we use
    // to mock an AOM restart (which is unloading the related jsm modules) may
    // affect the successfull removal of the temporary file because some of the
    // global helpers defined and used inside the XPIProvider may have been gone
    // already and intermittently trigger unexpected errors.
    await waitForTemporaryXPIFilesRemoved();

    // Restart the addon manager to confirm that the migrated colorways themes
    // are still gone after an AOM restart and that the previously installed
    // builtin hasn't been made implicitly visible again.
    info(
      "Verify old builtin colorways are not visible and default-theme still active after AOM restart"
    );
    await AddonTestUtils.promiseRestartManager();

    const defaultThemeAfterRestart = await AddonManager.getAddonByID(
      DEFAULT_THEME_ID
    );
    ok(
      defaultThemeAfterRestart.isActive,
      "Expect the default theme to be active"
    );

    equal(
      (await AddonManager.getAddonByID(ADDON_ID))?.version,
      undefined,
      "Expect the active theme addon to not be available anymore after being uninstalled"
    );
    equal(
      (await AddonManager.getAddonByID(ADDON_ID_RETAINED))?.version,
      undefined,
      "Expect the retained theme addon to not be available anymore after being uninstalled"
    );
    equal(
      (await AddonManager.getAddonByID(ADDON_ID_NOT_RETAINED))?.version,
      undefined,
      "Expect the not retained expired theme addon to not be available anymore after being uninstalled"
    );
  }
);
