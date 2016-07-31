const TESTPAGE = `${SECURE_TESTROOT}webapi_checkavailable.html`;
const XPI_URL = `${SECURE_TESTROOT}addons/browser_webapi_install.xpi`;

const ID = "webapi_install@tests.mozilla.org";
// eh, would be good to just stat the real file instead of this...
const XPI_LEN = 4782;

function waitForClear() {
  const MSG = "WebAPICleanup";
  return new Promise(resolve => {
    let listener = {
      receiveMessage: function(msg) {
        if (msg.name == MSG) {
          Services.ppmm.removeMessageListener(MSG, listener);
          resolve();
        }
      }
    };

    Services.ppmm.addMessageListener(MSG, listener);
  });
}

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({
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
function* testInstall(browser, url, steps, description) {
  let success = yield ContentTask.spawn(browser, {url, steps}, function* (opts) {
    let { url, steps } = opts;
    let install = yield content.navigator.mozAddonManager.createInstall({url});
    if (!install) {
      yield Promise.reject("createInstall() did not return an install object");
    }

    // Check that the initial state of the AddonInstall is sane.
    if (install.state != "STATE_AVAILABLE") {
      yield Promise.reject("new install should be in STATE_AVAILABLE");
    }
    if (install.error != null) {
      yield Promise.reject("new install should have null error");
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
            }
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
          yield install.install();
        } else if (nextStep.action == "cancel") {
          yield install.cancel();
        } else {
          throw new Error(`unknown action ${nextStep.action}`);
        }
      } else {
        yield expectEvent(nextStep.event, nextStep.props);
      }
    }

    return true;
  });

  is(success, true, description);
}

function makeInstallTest(task) {
  return function*() {
    // withNewTab() will close the test tab before returning, at which point
    // the cleanup event will come from the content process.  We need to see
    // that event but don't want to race to install a listener for it after
    // the tab is closed.  So set up the listener now but don't yield the
    // listening promise until below.
    let clearPromise = waitForClear();

    yield BrowserTestUtils.withNewTab(TESTPAGE, task);

    yield clearPromise;
    is(AddonManager.webAPI.installs.size, 0, "AddonInstall was cleaned up");
  };
}

// Check the happy path for installing an add-on using the mozAddonManager API.
add_task(makeInstallTest(function* (browser) {
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

  yield testInstall(browser, XPI_URL, steps, "a basic install works");

  let version = Services.prefs.getIntPref("webapitest.active_version");
  is(version, 1, "the install really did work");

  // Sanity check to ensure that the test in makeInstallTest() that
  // installs.size == 0 means we actually did clean up.
  ok(AddonManager.webAPI.installs.size > 0, "webAPI is tracking the AddonInstall");

  let addons = yield promiseAddonsByIDs([ID]);
  isnot(addons[0], null, "Found the addon");

  yield addons[0].uninstall();

  addons = yield promiseAddonsByIDs([ID]);
  is(addons[0], null, "Addon was uninstalled");
}));

add_task(makeInstallTest(function* (browser) {
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

  yield testInstall(browser, XPI_URL, steps, "canceling an install works");

  let addons = yield promiseAddonsByIDs([ID]);
  is(addons[0], null, "The addon was not installed");

  ok(AddonManager.webAPI.installs.size > 0, "webAPI is tracking the AddonInstall");
}));

add_task(makeInstallTest(function* (browser) {
  let steps = [
    {action: "install"},
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

  yield testInstall(browser, XPI_URL + "bogus", steps, "install of a bad url fails");

  let addons = yield promiseAddonsByIDs([ID]);
  is(addons[0], null, "The addon was not installed");

  ok(AddonManager.webAPI.installs.size > 0, "webAPI is tracking the AddonInstall");
}));

add_task(function* test_permissions() {
  function testBadUrl(url, pattern, successMessage) {
    return BrowserTestUtils.withNewTab(TESTPAGE, function* (browser) {
      let result = yield ContentTask.spawn(browser, {url, pattern}, function (opts) {
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

  yield testBadUrl("i am not a url", "NS_ERROR_MALFORMED_URI",
                   "Installing from an unparseable URL fails");

  yield testBadUrl("https://addons.not-really-mozilla.org/impostor.xpi",
                   "not permitted",
                   "Installing from non-approved URL fails");
});
