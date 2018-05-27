/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that side-loaded extensions with invalid install.rdf files are
// not initialized at startup.

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

XPCOMUtils.defineLazyServiceGetters(this, {
  ChromeRegistry: ["@mozilla.org/chrome/chrome-registry;1", "nsIChromeRegistry"],
});

function hasChromeEntry(package) {
  try {
    void ChromeRegistry.convertChromeURL(Services.io.newURI(`chrome://${package}/content/`));
    return true;
  } catch (e) {
    return false;
  }
}

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
      content foo-langpack ./
    `,
  });

  await promiseWriteInstallRDFToXPI({
    id: "foo@addons.mozilla.org",
    version: "1.0",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Invalid install.rdf extension",
  }, userExtensions, undefined, {
    "chrome.manifest": `
      content foo ./
    `,
  });

  await promiseWriteInstallRDFToXPI({
    id: "foo-legacy-legacy@addons.mozilla.org",
    version: "1.0",
    bootstrap: false,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Invalid install.rdf extension",
  }, userExtensions, undefined, {
    "chrome.manifest": `
      content foo-legacy-legacy ./
    `,
  });

  equal(hasChromeEntry("foo-langpack"), false,
        "Should not have registered foo-langpack resource before AOM startup");
  equal(hasChromeEntry("foo-legacy-legacy"), false,
        "Should not have registered foo-legacy-legacy resource before AOM startup");
  equal(hasChromeEntry("foo"), false,
        "Should not have registered foo resource before AOM startup");

  await promiseStartupManager();

  equal(hasChromeEntry("foo-langpack"), false,
        "Should not have registered chrome manifest for invalid extension");
  equal(hasChromeEntry("foo-legacy-legacy"), false,
        "Should not have registered chrome manifest for non-restartless extension");
  equal(hasChromeEntry("foo"), true,
        "Should have registered chrome manifest for valid extension");

  await promiseRestartManager();

  equal(hasChromeEntry("foo-langpack"), false,
        "Should still not have registered chrome manifest for invalid extension after restart");
  equal(hasChromeEntry("foo-legacy-legacy"), false,
        "Should still not have registered chrome manifest for non-restartless extension");
  equal(hasChromeEntry("foo"), true,
        "Should still have registered chrome manifest for valid extension after restart");

  await promiseShutdownManager();

  userAppDir.remove(true);
});
