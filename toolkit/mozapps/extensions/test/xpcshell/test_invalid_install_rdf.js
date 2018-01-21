/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that side-loaded extensions with invalid install.rdf files are
// not initialized at startup.

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const APP_ID = "xpcshell@tests.mozilla.org";

Services.prefs.setIntPref("extensions.enabledScopes", AddonManager.SCOPE_USER);

createAppInfo(APP_ID, "XPCShell", "1", "1.9.2");

const userAppDir = AddonTestUtils.profileDir.clone();
userAppDir.append("app-extensions");
userAppDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
AddonTestUtils.registerDirectory("XREUSysExt", userAppDir);

const userExtensions = userAppDir.clone();
userExtensions.append(APP_ID);
userExtensions.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

add_task(async function() {
  await promiseWriteInstallRDFToXPI({
    id: "langpack-foo@addons.mozilla.org",
    version: "1.0",
    type: 8,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Invalid install.rdf extension",
  }, userExtensions, undefined, {
    "chrome.manifest": `
      resource foo-langpack ./
    `,
  });

  await promiseWriteInstallRDFToXPI({
    id: "foo@addons.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Invalid install.rdf extension",
  }, userExtensions, undefined, {
    "chrome.manifest": `
      resource foo ./
    `,
  });

  const resProto = Services.io.getProtocolHandler("resource");
  resProto.QueryInterface(Ci.nsIResProtocolHandler);

  equal(resProto.hasSubstitution("foo-langpack"), false,
        "Should not foo-langpack resource before AOM startup");
  equal(resProto.hasSubstitution("foo"), false,
        "Should not foo resource before AOM startup");

  await promiseStartupManager();

  equal(resProto.hasSubstitution("foo-langpack"), false,
        "Should not have registered chrome manifest for invalid extension");
  equal(resProto.hasSubstitution("foo"), true,
        "Should have registered chrome manifest for valid extension");

  await promiseRestartManager();

  equal(resProto.hasSubstitution("foo-langpack"), false,
        "Should still not have registered chrome manifest for invalid extension after restart");
  equal(resProto.hasSubstitution("foo"), true,
        "Should still have registered chrome manifest for valid extension after restart");

  await promiseShutdownManager();

  userAppDir.remove(true);
});
