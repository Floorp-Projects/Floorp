const SERVER_URL =
  "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs";
var oldServerUrl = null;

function set_fake_crashreporter_url() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  oldServerUrl = env.get("MOZ_CRASHREPORTER_URL");
  env.set("MOZ_CRASHREPORTER_URL", SERVER_URL);
}

function reset_fake_crashreporter_url() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  env.set("MOZ_CRASHREPORTER_URL", oldServerUrl);
}

function cleanup_and_finish() {
  try {
    cleanup_fake_appdir();
  } catch (ex) {}
  Services.prefs.clearUserPref("breakpad.reportURL");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  reset_fake_crashreporter_url();
  finish();
}

/*
 * check_submit_incomplete
 *
 * Check that crash reports that are missing the .extra file can be submitted.
 * This will check if the submission process works by clicking on the "Submit"
 * button in the about:crashes page and then verifies that the submission
 * received on the server side contains the minimum set of crash annotations
 * required by a valid report.
 */
function check_submit_incomplete(tab, crash) {
  let crashReportPromise = TestUtils.topicObserved(
    "crash-report-status",
    (subject, data) => {
      return data == "success";
    }
  );
  crashReportPromise.then(result => {
    let crashData = convertPropertyBag(result[0]);

    ok(crashData.serverCrashID, "Should have a serverCrashID set.");
    ok(crashData.extra.ProductName, "Should have a ProductName field.");
    ok(crashData.extra.ReleaseChannel, "Should have a ReleaseChannel field.");
    ok(crashData.extra.Vendor, "Should have a Vendor field.");
    ok(crashData.extra.Version, "Should have a Version field.");
    ok(crashData.extra.BuildID, "Should have a BuildID field.");
    ok(crashData.extra.ProductID, "Should have a ProductID field.");

    cleanup_and_finish();
  });

  const browser = gBrowser.getBrowserForTab(tab);
  function csp_onsuccess() {
    browser.removeEventListener("CrashSubmitSucceeded", csp_onsuccess, true);
    ok(true, "the crash report was submitted successfully");
  }
  function csp_fail() {
    browser.removeEventListener("CrashSubmitFailed", csp_fail, true);
    ok(false, "failed to submit crash report!");
  }
  browser.addEventListener("CrashSubmitSucceeded", csp_onsuccess, true);
  browser.addEventListener("CrashSubmitFailed", csp_fail, true);

  ContentTask.spawn(browser, crash.id, id => {
    const submitButton = content.document
      .getElementById(id)
      .getElementsByClassName("submit-button")[0];
    submitButton.click();
  });
}

function convertPropertyBag(aBag) {
  let result = {};
  for (let { name, value } of aBag.enumerator) {
    if (value instanceof Ci.nsIPropertyBag) {
      value = convertPropertyBag(value);
    }
    result[name] = value;
  }
  return result;
}

function test() {
  waitForExplicitFinish();
  set_fake_crashreporter_url();
  const appD = make_fake_appdir();
  const crD = appD.clone();
  crD.append("Crash Reports");
  let crash = addIncompletePendingCrashreport(crD, Date.now());

  BrowserTestUtils.openNewForegroundTab(gBrowser, "about:crashes").then(tab => {
    check_submit_incomplete(tab, crash);
  });
}
