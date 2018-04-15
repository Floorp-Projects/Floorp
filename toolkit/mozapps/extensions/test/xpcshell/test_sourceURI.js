/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ChromeUtils.import("resource://testing-common/httpd.js");
var gServer = new HttpServer();
gServer.start(-1);

const PREF_GETADDONS_CACHE_ENABLED       = "extensions.getAddons.cache.enabled";

const PORT          = gServer.identity.primaryPort;
const BASE_URL      = "http://localhost:" + PORT;

var addon = {
  id: "addon@tests.mozilla.org",
  version: "1.0",
  name: "Test",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

function backgroundUpdate(aCallback) {
  Services.obs.addObserver(function observer() {
    Services.obs.removeObserver(observer, "addons-background-update-complete");
    aCallback();
  }, "addons-background-update-complete");

  AddonManagerPrivate.backgroundUpdateCheck();
}

async function run_test() {
  do_test_pending();

  const GETADDONS_RESPONSE = {
    page_size: 25,
    next: null,
    previous: null,
    results: [
      {
        name: "Test",
        type: "extension",
        guid: "addon@tests.mozilla.org",
        current_version: {
          version: "1",
          files: [
            {
              platform: "all",
              url: "http://www.example.com/testaddon.xpi"
            },
          ],
        },
      }
    ],
  };
  gServer.registerPathHandler("/addons.json", (request, response) => {
    response.setHeader("content-type", "application/json");
    response.write(JSON.stringify(GETADDONS_RESPONSE));
  });

  const COMPAT_RESPONSE = {
    next: null,
    results: [],
  };
  gServer.registerPathHandler("/compat.json", (request, response) => {
    response.setHeader("content-type", "application/json");
    response.write(JSON.stringify(COMPAT_RESPONSE));
  });

  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, `${BASE_URL}/addons.json`);
  Services.prefs.setCharPref(PREF_COMPAT_OVERRIDES, `${BASE_URL}/compat.json`);
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  writeInstallRDFForExtension(addon, profileDir);
  startupManager();

  let a = await AddonManager.getAddonByID("addon@tests.mozilla.org");
  Assert.notEqual(a, null);
  Assert.equal(a.sourceURI, null);

  backgroundUpdate(async function() {
    restartManager();

    let a2 = await AddonManager.getAddonByID("addon@tests.mozilla.org");
    Assert.notEqual(a2, null);
    Assert.notEqual(a2.sourceURI, null);
    Assert.equal(a2.sourceURI.spec, "http://www.example.com/testaddon.xpi");

    do_test_finished();
  });
}
