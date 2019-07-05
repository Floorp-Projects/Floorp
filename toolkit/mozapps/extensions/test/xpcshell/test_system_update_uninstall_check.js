// Tests that system add-on doesnt uninstall while update.

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2");

let distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "empty"], true);
registerDirectory("XREAppFeat", distroDir);

AddonTestUtils.usePrivilegedSignatures = "system";

add_task(() => initSystemAddonDirs());

const initialSetup = {
  async setup() {
    await buildPrefilledUpdatesDir();
    distroDir.leafName = "empty";
  },
  initialState: [
    { isUpgrade: false, version: null },
    { isUpgrade: true, version: "2.0" },
  ],
};

add_task(async function test_systems_update_uninstall_check() {
  Services.prefs.setBoolPref(PREF_SYSTEM_ADDON_UPDATE_ENABLED, true);

  await setupSystemAddonConditions(initialSetup, distroDir);

  let updateXML = buildSystemAddonUpdates([
    {
      id: "system2@tests.mozilla.org",
      version: "3.0",
      path: "system2_3.xpi",
      xpi: await getSystemAddonXPI(2, "3.0"),
    },
  ]);

  const listener = (msg, { method, params, reason }) => {
    if (params.id === "system2@tests.mozilla.org" && method === "uninstall") {
      Assert.ok(
        false,
        "Should not see uninstall call for system2@tests.mozilla.org"
      );
    }
  };

  AddonTestUtils.on("bootstrap-method", listener);

  await Promise.all([
    updateAllSystemAddons(updateXML),
    promiseWebExtensionStartup("system2@tests.mozilla.org"),
  ]);

  AddonTestUtils.off("bootstrap-method", listener);

  await promiseShutdownManager();
});
