"use strict";

/* exported AppConstants, Assert, AppTestDelegate */

var { AppConstants } = SpecialPowers.ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
var { AppTestDelegate } = SpecialPowers.ChromeUtils.importESModule(
  "resource://specialpowers/AppTestDelegate.sys.mjs"
);

{
  let chromeScript = SpecialPowers.loadChromeScript(
    SimpleTest.getTestFileURL("chrome_cleanup_script.js")
  );

  SimpleTest.registerCleanupFunction(async () => {
    await new Promise(resolve => setTimeout(resolve, 0));

    chromeScript.sendAsyncMessage("check-cleanup");

    let results = await chromeScript.promiseOneMessage("cleanup-results");
    chromeScript.destroy();

    if (results.extraWindows.length || results.extraTabs.length) {
      ok(
        false,
        `Test left extra windows or tabs: ${JSON.stringify(results)}\n`
      );
    }
  });
}

let Assert = {
  // Cut-down version based on Assert.sys.mjs. Only supports regexp and objects as
  // the expected variables.
  rejects(promise, expected, msg) {
    return promise.then(
      () => {
        ok(false, msg);
      },
      actual => {
        let matched = false;
        if (Object.prototype.toString.call(expected) == "[object RegExp]") {
          if (expected.test(actual)) {
            matched = true;
          }
        } else if (actual instanceof expected) {
          matched = true;
        }

        if (matched) {
          ok(true, msg);
        } else {
          ok(false, `Unexpected exception for "${msg}": ${actual}`);
        }
      }
    );
  },
};

/* exported waitForLoad */

function waitForLoad(win) {
  return new Promise(resolve => {
    win.addEventListener(
      "load",
      function () {
        resolve();
      },
      { capture: true, once: true }
    );
  });
}

/* exported loadChromeScript */
function loadChromeScript(fn) {
  let wrapper = `
(${fn.toString()})();`;

  return SpecialPowers.loadChromeScript(new Function(wrapper));
}

/* exported consoleMonitor */
let consoleMonitor = {
  start(messages) {
    this.chromeScript = SpecialPowers.loadChromeScript(
      SimpleTest.getTestFileURL("mochitest_console.js")
    );
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

/* exported assertPersistentListeners */
async function assertPersistentListeners(
  extWrapper,
  apiNs,
  apiEvents,
  expected
) {
  const stringErr = await SpecialPowers.spawnChrome(
    [extWrapper.id, apiNs, apiEvents, expected],
    async (id, apiNs, apiEvents, expected) => {
      try {
        const { ExtensionTestCommon } = ChromeUtils.importESModule(
          "resource://testing-common/ExtensionTestCommon.sys.mjs"
        );
        const ext = { id };
        for (const event of apiEvents) {
          ExtensionTestCommon.testAssertions.assertPersistentListeners(
            ext,
            apiNs,
            event,
            {
              primed: expected.primed,
              persisted: expected.persisted,
              primedListenersCount: expected.primedListenersCount,
            }
          );
        }
      } catch (err) {
        return String(err);
      }
    }
  );
  ok(
    stringErr == undefined,
    stringErr ? stringErr : `Found expected primed and persistent listeners`
  );
}
