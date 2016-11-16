"use strict";

// We run tests under two different configurations, from mochitest.ini and
// mochitest-remote.ini. When running from mochitest-remote.ini, the tests are
// copied to the sub-directory "test-oop-extensions", which we detect here, and
// use to select our configuration.
if (location.pathname.includes("test-oop-extensions")) {
  add_task(() => {
    return SpecialPowers.pushPrefEnv({set: [
      ["dom.ipc.processCount", 1],
      ["extensions.webextensions.remote", true],
    ]});
  });
}

/* exported waitForLoad */

function waitForLoad(win) {
  return new Promise(resolve => {
    win.addEventListener("load", function listener() {
      win.removeEventListener("load", listener, true);
      resolve();
    }, true);
  });
}

