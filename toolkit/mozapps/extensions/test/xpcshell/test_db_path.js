
const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const DEFAULT_THEME_ID = "default-theme@mozilla.org";

let global = this;

// Test that paths in the extensions database are stored properly
// if they include non-ascii characters (see bug 1428234 for an example of
// a past bug with such paths)
add_task(async function test_non_ascii_path() {
  let env = Cc["@mozilla.org/process/environment;1"]
              .getService(Ci.nsIEnvironment);
  const PROFILE_VAR = "XPCSHELL_TEST_PROFILE_DIR";
  let profileDir = OS.Path.join(env.get(PROFILE_VAR),
                                "\u00ce \u00e5m \u00f1\u00f8t \u00e5s\u00e7ii");
  env.set(PROFILE_VAR, profileDir);

  AddonTestUtils.init(global);
  AddonTestUtils.overrideCertDB();
  AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  const ID1 = "profile1@tests.mozilla.org";
  let xpi1 = await AddonTestUtils.createTempWebExtensionFile({
    id: ID1,
    manifest: {
      applications: {gecko: {id: ID1}},
    },
  });

  const ID2 = "profile2@tests.mozilla.org";
  let xpi2 = await AddonTestUtils.createTempWebExtensionFile({
    id: ID2,
    manifest: {
      applications: {gecko: {id: ID2}},
    },
  });

  await AddonTestUtils.manuallyInstall(xpi1);
  await AddonTestUtils.promiseStartupManager();
  await AddonTestUtils.promiseInstallFile(xpi2);
  await AddonTestUtils.promiseShutdownManager();

  let dbfile = OS.Path.join(profileDir, "extensions.json");
  let raw = new TextDecoder().decode(await OS.File.read(dbfile));
  let data = JSON.parse(raw);

  let addons = data.addons.filter(a => a.id !== DEFAULT_THEME_ID);
  Assert.ok(Array.isArray(addons), "extensions.json has addons array");
  Assert.equal(2, addons.length, "extensions.json has 2 addons");
  Assert.ok(addons[0].path.startsWith(profileDir),
            "path property for sideloaded extension has the proper profile directory");
  Assert.ok(addons[1].path.startsWith(profileDir),
            "path property for extension installed at runtime has the proper profile directory");
});
