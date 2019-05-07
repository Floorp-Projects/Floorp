/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const PREF_BLOCKLIST_URL              = "extensions.blocklist.url";
const PREF_BLOCKLIST_ENABLED          = "extensions.blocklist.enabled";
const PREF_APP_DISTRIBUTION           = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION   = "distribution.version";
const PREF_APP_UPDATE_CHANNEL         = "app.update.channel";

// Get the HTTP server.
var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});

async function updateBlocklist(file) {
  let blocklistUpdated = TestUtils.topicObserved("addon-blocklist-updated");
  Blocklist.notify();
  return blocklistUpdated;
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  await promiseStartupManager();
});

add_task(async function test_blocklist_disabled() {
  testserver.registerPathHandler("/1", () => {
    ok(false, "Should not have attempted to retrieve the blocklist when it is disabled");
  });

  // This should have no effect as the blocklist is disabled
  Services.prefs.setCharPref(PREF_BLOCKLIST_URL, "http://example.com/1");
  Services.prefs.setBoolPref(PREF_BLOCKLIST_ENABLED, false);

  // No await here. We're not expecting it to resolve until the
  // blocklist is re-enabled.
  updateBlocklist();
});

add_task(async function test_blocklist_disabled() {
  var ABI = "noarch-spidermonkey";
  let osVersion;
  try {
    osVersion = Services.sysinfo.getProperty("name") + " " + Services.sysinfo.getProperty("version");
    if (osVersion) {
      osVersion += " (" + Services.sysinfo.getProperty("secondaryLibrary") + ")";
    }
  } catch (e) {}

  const EXPECTED = {
    app_id: "xpcshell@tests.mozilla.org",
    app_version: 1,
    product: "XPCShell",
    version: 1,
    build_id: gAppInfo.appBuildID,
    build_target: "XPCShell_" + ABI,
    locale: "locale",
    channel: "updatechannel",
    os_version: osVersion,
    platform_version: "1.9",
    distribution: "distribution",
    distribution_version: "distribution-version",
  };

  const PARAMS = Object.keys(EXPECTED).map(key => `${key}=%${key.toUpperCase()}%`).join("&");

  let gotRequest = new Promise(resolve => {
    testserver.registerPathHandler("/2", (request, response) => {
      let params = new URLSearchParams(request.queryString);
      for (let [key, val] of Object.entries(EXPECTED)) {
        equal(String(params.get(key)), val, `Expected value for ${key} parameter`);
      }

      resolve();
    });
  });

  // Some values have to be on the default branch to work
  var defaults = Services.prefs.QueryInterface(Ci.nsIPrefService)
                       .getDefaultBranch(null);
  defaults.setCharPref(PREF_APP_UPDATE_CHANNEL, EXPECTED.channel);
  defaults.setCharPref(PREF_APP_DISTRIBUTION, EXPECTED.distribution);
  defaults.setCharPref(PREF_APP_DISTRIBUTION_VERSION, EXPECTED.distribution_version);
  Services.locale.requestedLocales = [EXPECTED.locale];

  // This should correctly escape everything
  Services.prefs.setCharPref(PREF_BLOCKLIST_URL, "http://example.com/2?" + PARAMS);
  Services.prefs.setBoolPref(PREF_BLOCKLIST_ENABLED, true);

  await updateBlocklist();
  await gotRequest;
});
