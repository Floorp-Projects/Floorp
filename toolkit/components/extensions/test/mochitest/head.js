"use strict";

/* exported AppConstants, Assert */

var {AppConstants} = SpecialPowers.Cu.import("resource://gre/modules/AppConstants.jsm", {});

// We run tests under two different configurations, from mochitest.ini and
// mochitest-remote.ini. When running from mochitest-remote.ini, the tests are
// copied to the sub-directory "test-oop-extensions", which we detect here, and
// use to select our configuration.
let remote = location.pathname.includes("test-oop-extensions");
SpecialPowers.pushPrefEnv({set: [
  ["extensions.webextensions.remote", remote],
]});
if (remote) {
  // We don't want to reset this at the end of the test, so that we don't have
  // to spawn a new extension child process for each test unit.
  SpecialPowers.setIntPref("dom.ipc.keepProcessesAlive.extension", 1);
}

{
  let chromeScript = SpecialPowers.loadChromeScript(
    SimpleTest.getTestFileURL("chrome_cleanup_script.js"));

  SimpleTest.registerCleanupFunction(async () => {
    await new Promise(resolve => setTimeout(resolve, 0));

    chromeScript.sendAsyncMessage("check-cleanup");

    let results = await chromeScript.promiseOneMessage("cleanup-results");
    chromeScript.destroy();

    if (results.extraWindows.length || results.extraTabs.length) {
      ok(false, `Test left extra windows or tabs: ${JSON.stringify(results)}\n`);
    }
  });
}

let Assert = {
  rejects(promise, msg) {
    return promise.then(() => {
      ok(false, msg);
    }, () => {
      ok(true, msg);
    });
  },
};

/* exported waitForLoad */

function waitForLoad(win) {
  return new Promise(resolve => {
    win.addEventListener("load", function() {
      resolve();
    }, {capture: true, once: true});
  });
}

/* exported loadChromeScript */
function loadChromeScript(fn) {
  let wrapper = `
const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});
(${fn.toString()})();`;

  return SpecialPowers.loadChromeScript(new Function(wrapper));
}

/* exported consoleMonitor */
let consoleMonitor = {
  start(messages) {
    this.chromeScript = SpecialPowers.loadChromeScript(
      SimpleTest.getTestFileURL("mochitest_console.js"));
    this.chromeScript.sendAsyncMessage("consoleStart", messages);
  },

  async finished() {
    let done = this.chromeScript.promiseOneMessage("consoleDone").then(done => {
      this.chromeScript.destroy();
      return done;
    });
    this.chromeScript.sendAsyncMessage("waitForConsole");
    let test = await done;
    ok(test.ok, test.message);
  },
};
/* exported waitForState */

function waitForState(sw, state) {
  return new Promise(resolve => {
    if (sw.state === state) {
      return resolve();
    }
    sw.addEventListener("statechange", function onStateChange() {
      if (sw.state === state) {
        sw.removeEventListener("statechange", onStateChange);
        resolve();
      }
    });
  });
}
