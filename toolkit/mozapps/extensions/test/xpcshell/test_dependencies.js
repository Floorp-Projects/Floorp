/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const profileDir = gProfD.clone();
profileDir.append("extensions");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

const BOOTSTRAP = String.raw`
  Components.utils.import("resource://gre/modules/Services.jsm");

  function startup(data) {
    Services.obs.notifyObservers(null, "test-addon-bootstrap-startup", data.id);
  }
  function shutdown(data) {
    Services.obs.notifyObservers(null, "test-addon-bootstrap-shutdown", data.id);
  }
  function install() {}
  function uninstall() {}
`;

const ADDONS = [
  {
    id: "addon1@dependency-test.mozilla.org",
    dependencies: ["addon2@dependency-test.mozilla.org"],
  },
  {
    id: "addon2@dependency-test.mozilla.org",
    dependencies: ["addon3@dependency-test.mozilla.org"],
  },
  {
    id: "addon3@dependency-test.mozilla.org",
  },
  {
    id: "addon4@dependency-test.mozilla.org",
  },
  {
    id: "addon5@dependency-test.mozilla.org",
    dependencies: ["addon2@dependency-test.mozilla.org"],
  },
];

let addonFiles = [];

let events = [];
add_task(async function setup() {
  await promiseStartupManager();

  let startupObserver = (subject, topic, data) => {
    events.push(["startup", data]);
  };
  let shutdownObserver = (subject, topic, data) => {
    events.push(["shutdown", data]);
  };

  Services.obs.addObserver(startupObserver, "test-addon-bootstrap-startup");
  Services.obs.addObserver(shutdownObserver, "test-addon-bootstrap-shutdown");
  registerCleanupFunction(() => {
    Services.obs.removeObserver(startupObserver, "test-addon-bootstrap-startup");
    Services.obs.removeObserver(shutdownObserver, "test-addon-bootstrap-shutdown");
  });

  for (let addon of ADDONS) {
    Object.assign(addon, {
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1",
      }],
      version: "1.0",
      name: addon.id,
      bootstrap: true,
    });

    addonFiles.push(createTempXPIFile(addon, {"bootstrap.js": BOOTSTRAP}));
  }
});

add_task(async function() {
  deepEqual(events, [], "Should have no events");

  await promiseInstallAllFiles([addonFiles[3]]);

  deepEqual(events, [
    ["startup", ADDONS[3].id],
  ]);

  events.length = 0;

  await promiseInstallAllFiles([addonFiles[0]]);
  deepEqual(events, [], "Should have no events");

  await promiseInstallAllFiles([addonFiles[1]]);
  deepEqual(events, [], "Should have no events");

  await promiseInstallAllFiles([addonFiles[2]]);

  deepEqual(events, [
    ["startup", ADDONS[2].id],
    ["startup", ADDONS[1].id],
    ["startup", ADDONS[0].id],
  ]);

  events.length = 0;

  await promiseInstallAllFiles([addonFiles[2]]);

  deepEqual(events, [
    ["shutdown", ADDONS[0].id],
    ["shutdown", ADDONS[1].id],
    ["shutdown", ADDONS[2].id],

    ["startup", ADDONS[2].id],
    ["startup", ADDONS[1].id],
    ["startup", ADDONS[0].id],
  ]);

  events.length = 0;

  await promiseInstallAllFiles([addonFiles[4]]);

  deepEqual(events, [
    ["startup", ADDONS[4].id],
  ]);

  events.length = 0;

  await promiseRestartManager();

  deepEqual(events, [
    ["shutdown", ADDONS[4].id],
    ["shutdown", ADDONS[3].id],
    ["shutdown", ADDONS[0].id],
    ["shutdown", ADDONS[1].id],
    ["shutdown", ADDONS[2].id],

    ["startup", ADDONS[2].id],
    ["startup", ADDONS[1].id],
    ["startup", ADDONS[0].id],
    ["startup", ADDONS[3].id],
    ["startup", ADDONS[4].id],
  ]);
});

