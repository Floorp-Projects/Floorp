/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { QuarantinedDomains } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.usePrivilegedSignatures = id => id.startsWith("privileged");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42", "42");

add_setup(async () => {
  await AddonTestUtils.promiseStartupManager();
});

function assertQuarantineIgnoredByUserPrefsRemoved() {
  const { PREF_ADDONS_BRANCH_NAME } = QuarantinedDomains;
  const prefBranch = Services.prefs.getBranch(PREF_ADDONS_BRANCH_NAME);
  for (const prefSuffix of prefBranch.getChildList("")) {
    Assert.equal(
      prefBranch.getPrefType(prefSuffix),
      prefBranch.PREF_INVALID,
      `${PREF_ADDONS_BRANCH_NAME}${prefSuffix} pref should have been removed`
    );
  }
}

async function testQuarantineDomainsAddonWrapperProperties() {
  // Make sure no extension is initially user exempted.
  const prefBranch = Services.prefs.getBranch(
    QuarantinedDomains.PREF_ADDONS_BRANCH_NAME
  );
  for (const leafName of prefBranch.getChildList("")) {
    Services.prefs.clearUserPref(
      QuarantinedDomains.PREF_ADDONS_BRANCH_NAME + leafName
    );
  }

  const REGULAR_EXT_ID = "regular@ext.id";
  const PRIVILEGE_EXT_ID = "privileged@ext.id";
  const RECOMMENDED_EXT_ID = "recommended@ext.id";

  const regularExt = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id: REGULAR_EXT_ID } },
    },
  });

  const privilegedExt = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id: PRIVILEGE_EXT_ID } },
    },
  });

  const testStartTime = Date.now();
  // Keep the recommendations validity range used here in sync with
  // the one used in test_recommendations.js.
  const not_before = new Date(testStartTime - 3600000).toISOString();
  const not_after = new Date(testStartTime + 3600000).toISOString();
  const RECOMMENDATION_FILE_NAME = "mozilla-recommendation.json";
  const recommendedExt = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id: RECOMMENDED_EXT_ID } },
    },
    files: {
      [RECOMMENDATION_FILE_NAME]: {
        addon_id: RECOMMENDED_EXT_ID,
        states: ["fake", "states"],
        validity: { not_before, not_after },
      },
    },
  });

  await regularExt.startup();
  await privilegedExt.startup();
  await recommendedExt.startup();

  function assertAddonWrapperProps(addon, expectedProps) {
    const expectedPropNames = Object.keys(expectedProps);
    if (!expectedPropNames.length) {
      throw new Error("expectedProps shouldn't be empty");
    }
    for (const propName of expectedPropNames) {
      Assert.deepEqual(
        addon[propName],
        expectedProps[propName],
        `Got the expected value on ${propName} property from ${addon.id}`
      );
    }
  }

  const EXPECTED_PROPS_REGULAR_EXT = {
    id: regularExt.id,
    isPrivileged: false,
    recommendationStates: [],
    quarantineIgnoredByApp: false,
    quarantineIgnoredByUser: false,
    canChangeQuarantineIgnored: true,
  };

  const EXPECTED_PROPS_PRIVILEGED_EXT = {
    id: privilegedExt.id,
    isPrivileged: true,
    recommendationStates: [],
    // Expected to be true due to privileged signature.
    quarantineIgnoredByApp: true,
    quarantineIgnoredByUser: false,
    // Expected to be false for app allowed.
    canChangeQuarantineIgnored: false,
  };

  const EXPECTED_PROPS_RECOMMENDED_EXT = {
    id: recommendedExt.id,
    isPrivileged: false,
    recommendationStates: ["fake", "states"],
    // Expected to be true due to recommendationStates.
    quarantineIgnoredByApp: true,
    quarantineIgnoredByUser: false,
    // Expected to be false for app allowed.
    canChangeQuarantineIgnored: false,
  };

  assertAddonWrapperProps(regularExt.addon, EXPECTED_PROPS_REGULAR_EXT);

  assertAddonWrapperProps(privilegedExt.addon, EXPECTED_PROPS_PRIVILEGED_EXT);

  assertAddonWrapperProps(recommendedExt.addon, EXPECTED_PROPS_RECOMMENDED_EXT);

  info("Verify quarantineIgnoredByUser property changed");
  let promisePropChanged =
    AddonTestUtils.promiseAddonEvent("onPropertyChanged");
  regularExt.addon.quarantineIgnoredByUser = true;
  assertAddonWrapperProps(regularExt.addon, {
    ...EXPECTED_PROPS_REGULAR_EXT,
    quarantineIgnoredByUser: true,
  });
  info("Wait for onPropertyChanged listener to be called");
  let [addon, props] = await promisePropChanged;
  Assert.deepEqual(
    {
      addonId: addon.id,
      props,
    },
    {
      addonId: regularExt.id,
      props: ["quarantineIgnoredByUser"],
    },
    "Got the expected params from onPropertyChanged listener call"
  );
  Services.prefs.clearUserPref(
    QuarantinedDomains.getUserAllowedAddonIdPrefName(regularExt.id)
  );

  info("Verify canChangeQuarantineIgnored on quarantineDomainsEnabled false");
  Services.prefs.setBoolPref("extensions.quarantinedDomains.enabled", false);
  assertAddonWrapperProps(regularExt.addon, {
    ...EXPECTED_PROPS_REGULAR_EXT,
    canChangeQuarantineIgnored: false,
  });
  Services.prefs.clearUserPref("extensions.quarantinedDomains.enabled");
  assertAddonWrapperProps(regularExt.addon, EXPECTED_PROPS_REGULAR_EXT);

  info("Verify canChangeQuarantineIgnored on uiDisabled true");
  Services.prefs.setBoolPref("extensions.quarantinedDomains.uiDisabled", true);
  assertAddonWrapperProps(regularExt.addon, {
    ...EXPECTED_PROPS_REGULAR_EXT,
    canChangeQuarantineIgnored: false,
  });
  Services.prefs.clearUserPref("extensions.quarantinedDomains.uiDisabled");
  assertAddonWrapperProps(regularExt.addon, EXPECTED_PROPS_REGULAR_EXT);

  info(
    "Verify that the per-addon quarantineIgnoredByUser pref is removed on addon uninstall"
  );

  promisePropChanged = AddonTestUtils.promiseAddonEvent("onPropertyChanged");
  regularExt.addon.quarantineIgnoredByUser = true;
  await promisePropChanged;
  Assert.equal(
    Services.prefs.getBoolPref(
      QuarantinedDomains.getUserAllowedAddonIdPrefName(regularExt.id)
    ),
    true,
    "Expect the per-addon quarantineIgnoredByUser to be set"
  );

  await recommendedExt.unload();
  await privilegedExt.unload();
  await regularExt.unload();

  assertQuarantineIgnoredByUserPrefsRemoved();
}

add_task(
  {
    pref_set: [
      ["extensions.quarantinedDomains.enabled", true],
      ["extensions.quarantinedDomains.uiDisabled", false],
    ],
  },
  testQuarantineDomainsAddonWrapperProperties
);
