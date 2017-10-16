const TESTPAGE = `${SECURE_TESTROOT}webapi_checkavailable.html`;
const XPI_URL = `${SECURE_TESTROOT}addons/browser_webapi_install.xpi`;
const XPI_SHA = "sha256:d4bab17ff9ba5f635e97c84021f4c527c502250d62ab7f6e6c9e8ee28822f772";

const ID = "webapi_install@tests.mozilla.org";
// eh, would be good to just stat the real file instead of this...
const XPI_LEN = 4782;

function waitForClear() {
  const MSG = "WebAPICleanup";
  return new Promise(resolve => {
    let listener = {
      receiveMessage(msg) {
        if (msg.name == MSG) {
          Services.mm.removeMessageListener(MSG, listener);
          resolve();
        }
      }
    };

    Services.mm.addMessageListener(MSG, listener, true);
  });
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webapi.testing", true],
          ["extensions.install.requireBuiltInCerts", false]],
  });
  info("added preferences");
});

// Wrapper around a common task to run in the content process to test
// the mozAddonManager API.  Takes a URL for the XPI to install and an
// array of steps, each of which can either be an action to take
// (i.e., start or cancel the install) or an install event to wait for.
// Steps that look for a specific event may also include a "props" property
// with properties that the AddonInstall object is expected to have when
// that event is triggered.
async function testInstall(browser, args, steps, description) {
  let success = await ContentTask.spawn(browser, {args, steps}, async function(opts) {
    let { args, steps } = opts;
    let install = await content.navigator.mozAddonManager.createInstall(args);
    if (!install) {
      await Promise.reject("createInstall() did not return an install object");
    }

    // Check that the initial state of the AddonInstall is sane.
    if (install.state != "STATE_AVAILABLE") {
      await Promise.reject("new install should be in STATE_AVAILABLE");
    }
    if (install.error != null) {
      await Promise.reject("new install should have null error");
    }

    const events = [
      "onDownloadStarted",
      "onDownloadProgress",
      "onDownloadEnded",
      "onDownloadCancelled",
      "onDownloadFailed",
      "onInstallStarted",
      "onInstallEnded",
      "onInstallCancelled",
      "onInstallFailed",
    ];
    let eventWaiter = null;
    let receivedEvents = [];
    let prevEvent = null;
    events.forEach(event => {
      install.addEventListener(event, e => {
        receivedEvents.push({
          event,
          state: install.state,
          error: install.error,
          progress: install.progress,
          maxProgress: install.maxProgress,
        });
        if (eventWaiter) {
          eventWaiter();
        }
      });
    });

    // Returns a promise that is resolved when the given event occurs
    // or rejects if a different event comes first or if props is supplied
    // and properties on the AddonInstall don't match those in props.
    function expectEvent(event, props) {
      return new Promise((resolve, reject) => {
        function check() {
          let received = receivedEvents.shift();
          // Skip any repeated onDownloadProgress events.
          while (received &&
                 received.event == prevEvent &&
                 prevEvent == "onDownloadProgress") {
            received = receivedEvents.shift();
          }
          // Wait for more events if we skipped all there were.
          if (!received) {
            eventWaiter = () => {
              eventWaiter = null;
              check();
            };
            return;
          }
          prevEvent = received.event;
          if (received.event != event) {
            let err = new Error(`expected ${event} but got ${received.event}`);
            reject(err);
          }
          if (props) {
            for (let key of Object.keys(props)) {
              if (received[key] != props[key]) {
                throw new Error(`AddonInstall property ${key} was ${received[key]} but expected ${props[key]}`);
              }
            }
          }
          resolve();
        }
        check();
      });
    }

    while (steps.length > 0) {
      let nextStep = steps.shift();
      if (nextStep.action) {
        if (nextStep.action == "install") {
          try {
            await install.install();
            if (nextStep.expectError) {
              throw new Error("Expected install to fail but it did not");
            }
          } catch (err) {
            if (!nextStep.expectError) {
              throw new Error("Install failed unexpectedly");
            }
          }
        } else if (nextStep.action == "cancel") {
          await install.cancel();
        } else {
          throw new Error(`unknown action ${nextStep.action}`);
        }
      } else {
        await expectEvent(nextStep.event, nextStep.props);
      }
    }

    return true;
  });

  is(success, true, description);
}

function makeInstallTest(task) {
  return async function() {
    // withNewTab() will close the test tab before returning, at which point
    // the cleanup event will come from the content process.  We need to see
    // that event but don't want to race to install a listener for it after
    // the tab is closed.  So set up the listener now but don't yield the
    // listening promise until below.
    let clearPromise = waitForClear();

    await BrowserTestUtils.withNewTab(TESTPAGE, task);

    await clearPromise;
    is(AddonManager.webAPI.installs.size, 0, "AddonInstall was cleaned up");
  };
}

function makeRegularTest(options, what) {
  return makeInstallTest(async function(browser) {
    let steps = [
      {action: "install"},
      {
        event: "onDownloadStarted",
        props: {state: "STATE_DOWNLOADING"},
      },
      {
        event: "onDownloadProgress",
        props: {maxProgress: XPI_LEN},
      },
      {
        event: "onDownloadEnded",
        props: {
          state: "STATE_DOWNLOADED",
          progress: XPI_LEN,
          maxProgress: XPI_LEN,
        },
      },
      {
        event: "onInstallStarted",
        props: {state: "STATE_INSTALLING"},
      },
      {
        event: "onInstallEnded",
        props: {state: "STATE_INSTALLED"},
      },
    ];

    let promptPromise = promiseNotification("addon-installed");

    await testInstall(browser, options, steps, what);

    await promptPromise;

    let version = Services.prefs.getIntPref("webapitest.active_version");
    is(version, 1, "the install really did work");

    // Sanity check to ensure that the test in makeInstallTest() that
    // installs.size == 0 means we actually did clean up.
    ok(AddonManager.webAPI.installs.size > 0, "webAPI is tracking the AddonInstall");

    let addons = await promiseAddonsByIDs([ID]);
    isnot(addons[0], null, "Found the addon");

    await addons[0].uninstall();

    addons = await promiseAddonsByIDs([ID]);
    is(addons[0], null, "Addon was uninstalled");
  });
}

add_task(makeRegularTest({url: XPI_URL}, "a basic install works"));
add_task(makeRegularTest({url: XPI_URL, hash: null}, "install with hash=null works"));
add_task(makeRegularTest({url: XPI_URL, hash: ""}, "install with empty string for hash works"));
add_task(makeRegularTest({url: XPI_URL, hash: XPI_SHA}, "install with hash works"));

add_task(makeInstallTest(async function(browser) {
  let steps = [
    {action: "cancel"},
    {
      event: "onDownloadCancelled",
      props: {
        state: "STATE_CANCELLED",
        error: null,
      },
    }
  ];

  await testInstall(browser, {url: XPI_URL}, steps, "canceling an install works");

  let addons = await promiseAddonsByIDs([ID]);
  is(addons[0], null, "The addon was not installed");

  ok(AddonManager.webAPI.installs.size > 0, "webAPI is tracking the AddonInstall");
}));

add_task(makeInstallTest(async function(browser) {
  let steps = [
    {action: "install", expectError: true},
    {
      event: "onDownloadStarted",
      props: {state: "STATE_DOWNLOADING"},
    },
    {event: "onDownloadProgress"},
    {
      event: "onDownloadFailed",
      props: {
        state: "STATE_DOWNLOAD_FAILED",
        error: "ERROR_NETWORK_FAILURE",
      },
    }
  ];

  await testInstall(browser, {url: XPI_URL + "bogus"}, steps, "install of a bad url fails");

  let addons = await promiseAddonsByIDs([ID]);
  is(addons[0], null, "The addon was not installed");

  ok(AddonManager.webAPI.installs.size > 0, "webAPI is tracking the AddonInstall");
}));

add_task(makeInstallTest(async function(browser) {
  let steps = [
    {action: "install", expectError: true},
    {
      event: "onDownloadStarted",
      props: {state: "STATE_DOWNLOADING"},
    },
    {event: "onDownloadProgress"},
    {
      event: "onDownloadFailed",
      props: {
        state: "STATE_DOWNLOAD_FAILED",
        error: "ERROR_INCORRECT_HASH",
      },
    }
  ];

  await testInstall(browser, {url: XPI_URL, hash: "sha256:bogus"}, steps, "install with bad hash fails");

  let addons = await promiseAddonsByIDs([ID]);
  is(addons[0], null, "The addon was not installed");

  ok(AddonManager.webAPI.installs.size > 0, "webAPI is tracking the AddonInstall");
}));

add_task(async function test_permissions() {
  function testBadUrl(url, pattern, successMessage) {
    return BrowserTestUtils.withNewTab(TESTPAGE, async function(browser) {
      let result = await ContentTask.spawn(browser, {url, pattern}, function(opts) {
        return new Promise(resolve => {
          content.navigator.mozAddonManager.createInstall({url: opts.url})
            .then(() => {
              resolve({success: false, message: "createInstall should not have succeeded"});
            }, err => {
              if (err.message.match(new RegExp(opts.pattern))) {
                resolve({success: true});
              }
              resolve({success: false, message: `Wrong error message: ${err.message}`});
            });
        });
      });
      is(result.success, true, result.message || successMessage);
    });
  }

  await testBadUrl("i am not a url", "NS_ERROR_MALFORMED_URI",
                   "Installing from an unparseable URL fails");

  await testBadUrl("https://addons.not-really-mozilla.org/impostor.xpi",
                   "not permitted",
                   "Installing from non-approved URL fails");
});
