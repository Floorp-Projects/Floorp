// Tests that system add-on doesnt request update while normal backgroundUpdateCheck

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
  await setupSystemAddonConditions(initialSetup, distroDir);

  const testserver = createHttpServer({ hosts: ["example.com"] });
  testserver.registerPathHandler("/update.json", (request, response) => {
    Assert.ok(
      !request._queryString.includes("system2@tests.mozilla.org"),
      "System addon should not request update from normal update process"
    );
  });

  Services.prefs.setCharPref(
    "extensions.update.background.url",
    "http://example.com/update.json?id=%ITEM_ID%"
  );

  await AddonManagerInternal.backgroundUpdateCheck();

  await promiseShutdownManager();
});
