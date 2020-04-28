function cleanup_and_finish() {
  try {
    cleanup_fake_appdir();
  } catch (ex) {}
  Services.prefs.clearUserPref("breakpad.reportURL");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  finish();
}

/*
 * check_crash_list
 *
 * Check that the list of crashes displayed by about:crashes matches
 * the list of crashes that we placed in the pending+submitted directories.
 *
 * This function is run in a separate JS context via SpecialPowers.spawn, so
 * it has no access to other functions or variables in this file.
 */
function check_crash_list(crashes) {
  const doc = content.document;
  const crashIdNodes = Array.from(doc.getElementsByClassName("crash-id"));
  const pageCrashIds = new Set(crashIdNodes.map(node => node.textContent));
  const crashIds = new Set(crashes.map(crash => crash.id));
  Assert.deepEqual(
    pageCrashIds,
    crashIds,
    "about:crashes lists the correct crash reports."
  );
}

/*
 * check_submit_pending
 *
 * Click on a pending crash in about:crashes, wait for it to be submitted (which
 * should redirect us to the crash report page). Verify that the data provided
 * by our test crash report server matches the data we submitted.
 * Additionally, click "back" and verify that the link now points to our new
 */
function check_submit_pending(tab, crashes) {
  const browser = gBrowser.getBrowserForTab(tab);
  let SubmittedCrash = null;
  let CrashID = null;
  let CrashURL = null;
  function csp_onsuccess() {
    const crashLinks = content.document.getElementsByClassName("crash-link");
    // Get the last link since it is appended to the end of the list
    const link = crashLinks[crashLinks.length - 1];
    link.click();
  }
  function csp_onload() {
    // loaded the crash report page
    ok(true, "got submission onload");

    SpecialPowers.spawn(browser, [], function() {
      // grab the Crash ID here to verify later
      let CrashID = content.location.search.split("=")[1];
      let CrashURL = content.location.toString();

      // check the JSON content vs. what we submitted
      let result = JSON.parse(content.document.documentElement.textContent);
      Assert.equal(
        result.upload_file_minidump,
        "MDMP",
        "minidump file sent properly"
      );
      Assert.equal(
        result.memory_report,
        "Let's pretend this is a memory report",
        "memory report sent properly"
      );
      Assert.equal(
        +result.Throttleable,
        0,
        "correctly sent as non-throttleable"
      );
      // we checked these, they're set by the submission process,
      // so they won't be in the "extra" data.
      delete result.upload_file_minidump;
      delete result.memory_report;
      delete result.Throttleable;

      return { id: CrashID, url: CrashURL, result };
    }).then(({ id, url, result }) => {
      // Likewise, this is discarded before it gets to the server
      delete SubmittedCrash.extra.ServerURL;

      CrashID = id;
      CrashURL = url;
      for (let x in result) {
        if (x in SubmittedCrash.extra) {
          is(
            result[x],
            SubmittedCrash.extra[x],
            "submitted value for " + x + " matches expected"
          );
        } else {
          ok(false, "property " + x + " missing from submitted data!");
        }
      }
      for (let y in SubmittedCrash.extra) {
        if (!(y in result)) {
          ok(false, "property " + y + " missing from result data!");
        }
      }

      // We can listen for pageshow like this because the tab is not remote.
      BrowserTestUtils.waitForEvent(browser, "pageshow", true).then(
        csp_pageshow
      );

      // now navigate back
      browser.goBack();
    });
  }
  function csp_fail() {
    browser.removeEventListener("CrashSubmitFailed", csp_fail, true);
    ok(false, "failed to submit crash report!");
    cleanup_and_finish();
  }
  browser.addEventListener("CrashSubmitSucceeded", csp_onsuccess, true);
  browser.addEventListener("CrashSubmitFailed", csp_fail, true);
  BrowserTestUtils.browserLoaded(
    browser,
    false,
    url => url !== "about:crashes"
  ).then(csp_onload);
  function csp_pageshow() {
    SpecialPowers.spawn(browser, [{ CrashID, CrashURL }], function({
      CrashID,
      CrashURL,
    }) {
      Assert.equal(
        content.location.href,
        "about:crashes",
        "navigated back successfully"
      );
      const link = content.document
        .getElementById(CrashID)
        .getElementsByClassName("crash-link")[0];
      Assert.notEqual(link, null, "crash report link changed correctly");
      if (link) {
        Assert.equal(
          link.href,
          CrashURL,
          "crash report link points to correct href"
        );
      }
    }).then(cleanup_and_finish);
  }

  // try submitting the pending report
  for (const crash of crashes) {
    if (crash.pending) {
      SubmittedCrash = crash;
      break;
    }
  }

  SpecialPowers.spawn(browser, [SubmittedCrash.id], id => {
    const submitButton = content.document
      .getElementById(id)
      .getElementsByClassName("submit-button")[0];
    submitButton.click();
  });
}

function test() {
  waitForExplicitFinish();
  const appD = make_fake_appdir();
  const crD = appD.clone();
  crD.append("Crash Reports");
  const crashes = add_fake_crashes(crD, 1);
  // we don't need much data here, it's not going to a real Socorro
  crashes.push(
    addPendingCrashreport(crD, crashes[crashes.length - 1].date + 60000, {
      ServerURL:
        "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs",
      ProductName: "Test App",
      Foo: "ABC=XYZ", // test that we don't truncat eat = (bug 512853)
    })
  );
  crashes.sort((a, b) => b.date - a.date);

  // set this pref so we can link to our test server
  Services.prefs.setCharPref(
    "breakpad.reportURL",
    "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs?id="
  );

  BrowserTestUtils.openNewForegroundTab(gBrowser, "about:crashes").then(tab => {
    SpecialPowers.spawn(
      tab.linkedBrowser,
      [crashes],
      check_crash_list
    ).then(() => check_submit_pending(tab, crashes));
  });
}
