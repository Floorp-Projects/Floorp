/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test cancelling add-on update checks while in progress (bug 925389)

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

// Install one extension
// Start download of update check (but delay HTTP response)
// Cancel update check
//  - ensure we get cancel notification
// complete HTTP response
//  - ensure no callbacks after cancel
//  - ensure update is gone

// Create an addon update listener containing a promise
// that resolves when the update is cancelled
function makeCancelListener() {
  let resolve, reject;
  let promise = new Promise((_resolve, _reject) => {
    resolve = _resolve;
    reject = _reject;
  });

  return {
    onUpdateAvailable(addon, install) {
      reject("Should not have seen onUpdateAvailable notification");
    },

    onUpdateFinished(aAddon, aError) {
      info("onUpdateCheckFinished: " + aAddon.id + " " + aError);
      resolve(aError);
    },
    promise,
  };
}

let testserver = createHttpServer({ hosts: ["example.com"] });

// Set up the HTTP server so that we can control when it responds
let _httpResolve;
function resetUpdateListener() {
  return new Promise(resolve => {
    _httpResolve = resolve;
  });
}

testserver.registerPathHandler("/data/test_update.json", (req, resp) => {
  resp.processAsync();
  _httpResolve([req, resp]);
});

const UPDATE_RESPONSE = {
  addons: {
    "addon1@tests.mozilla.org": {
      updates: [
        {
          version: "2.0",
          update_link: "http://example.com/addons/test_update.xpi",
          applications: {
            gecko: {
              strict_min_version: "1",
              strict_max_version: "1",
            },
          },
        },
      ],
    },
  },
};

add_task(async function cancel_during_check() {
  await promiseStartupManager();

  await promiseInstallWebExtension({
    manifest: {
      name: "Test Addon 1",
      version: "1.0",
      applications: {
        gecko: {
          id: "addon1@tests.mozilla.org",
          update_url: "http://example.com/data/test_update.json",
        },
      },
    },
  });

  let a1 = await promiseAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(a1, null);

  let requestPromise = resetUpdateListener();
  let listener = makeCancelListener();
  a1.findUpdates(listener, AddonManager.UPDATE_WHEN_USER_REQUESTED);

  // Wait for the http request to arrive
  let [, /* request */ response] = await requestPromise;

  // cancelUpdate returns true if there is an update check in progress
  Assert.ok(a1.cancelUpdate());

  let updateResult = await listener.promise;
  Assert.equal(AddonManager.UPDATE_STATUS_CANCELLED, updateResult);

  // Now complete the HTTP request
  response.write(JSON.stringify(UPDATE_RESPONSE));
  response.finish();

  // trying to cancel again should return false, i.e. nothing to cancel
  Assert.ok(!a1.cancelUpdate());
});

// Test that update check is cancelled if the XPI provider shuts down while
// the update check is in progress
add_task(async function shutdown_during_check() {
  // Reset our HTTP listener
  let requestPromise = resetUpdateListener();

  let a1 = await promiseAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(a1, null);

  let listener = makeCancelListener();
  a1.findUpdates(listener, AddonManager.UPDATE_WHEN_USER_REQUESTED);

  // Wait for the http request to arrive
  let [, /* request */ response] = await requestPromise;

  await promiseShutdownManager();

  let updateResult = await listener.promise;
  Assert.equal(AddonManager.UPDATE_STATUS_CANCELLED, updateResult);

  // Now complete the HTTP request
  response.write(JSON.stringify(UPDATE_RESPONSE));
  response.finish();
});
