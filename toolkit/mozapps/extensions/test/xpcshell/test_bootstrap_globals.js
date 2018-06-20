/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that bootstrap.js has the expected globals defined
ChromeUtils.import("resource://gre/modules/Services.jsm");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

const ADDONS = {
  bootstrap_globals: {
    "install.rdf": {
      "id": "bootstrap_globals@tests.mozilla.org",
    },
    "bootstrap.js": String.raw`ChromeUtils.import("resource://gre/modules/Services.jsm");

var seenGlobals = new Set();
var scope = this;
function checkGlobal(name, type) {
  if (scope[name] && typeof(scope[name]) == type)
    seenGlobals.add(name);
}

var wrapped = {};
Services.obs.notifyObservers({ wrappedJSObject: wrapped }, "bootstrap-request-globals");
for (let [name, type] of wrapped.expectedGlobals) {
  checkGlobal(name, type);
}

function startup(data, reason) {
  Services.obs.notifyObservers({ wrappedJSObject: seenGlobals }, "bootstrap-seen-globals");
}

function install(data, reason) {}
function shutdown(data, reason) {}
function uninstall(data, reason) {}
`,
  },
};


const EXPECTED_GLOBALS = [
  ["console", "object"]
];

async function run_test() {
  do_test_pending();
  await promiseStartupManager();
  let sawGlobals = false;

  Services.obs.addObserver(function(subject) {
    subject.wrappedJSObject.expectedGlobals = EXPECTED_GLOBALS;
  }, "bootstrap-request-globals");

  Services.obs.addObserver(function({ wrappedJSObject: seenGlobals }) {
    for (let [name, ] of EXPECTED_GLOBALS)
      Assert.ok(seenGlobals.has(name));

    sawGlobals = true;
  }, "bootstrap-seen-globals");

  await AddonTestUtils.promiseInstallXPI(ADDONS.bootstrap_globals);
  Assert.ok(sawGlobals);
  await promiseShutdownManager();
  do_test_finished();
}
