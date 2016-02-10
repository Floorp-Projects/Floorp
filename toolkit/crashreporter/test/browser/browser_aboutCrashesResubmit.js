function cleanup_and_finish() {
  try {
    cleanup_fake_appdir();
  } catch(ex) {}
  Services.prefs.clearUserPref("breakpad.reportURL");
  BrowserTestUtils.removeTab(gBrowser.selectedTab).then(finish);
}

/*
 * check_crash_list
 *
 * Check that the list of crashes displayed by about:crashes matches
 * the list of crashes that we placed in the pending+submitted directories.
 *
 * NB: This function is run in the child process via ContentTask.spawn.
 */
function check_crash_list(crashes) {
  let doc = content.document;
  let crashlinks = doc.getElementById("tbody").getElementsByTagName("a");
  is(crashlinks.length, crashes.length,
    "about:crashes lists correct number of crash reports");
  // no point in checking this if the lists aren't the same length
  if (crashlinks.length == crashes.length) {
    for(let i=0; i<crashes.length; i++) {
      is(crashlinks[i].id, crashes[i].id, i + ": crash ID is correct");
      if (crashes[i].pending) {
        // we set the breakpad.reportURL pref in test()
        is(crashlinks[i].getAttribute("href"),
          "http://example.com/browser/toolkit/crashreporter/about/throttling",
          "pending URL links to the correct static page");
      }
    }
  }
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
  let browser = gBrowser.getBrowserForTab(tab);
  let SubmittedCrash = null;
  let CrashID = null;
  let CrashURL = null;
  function csp_onload() {
    // loaded the crash report page
    ok(true, 'got submission onload');

    ContentTask.spawn(browser, null, function() {
      // grab the Crash ID here to verify later
      let CrashID = content.location.search.split("=")[1];
      let CrashURL = content.location.toString();

      // check the JSON content vs. what we submitted
      let result = JSON.parse(content.document.documentElement.textContent);
      is(result.upload_file_minidump, "MDMP", "minidump file sent properly");
      is(result.memory_report, "Let's pretend this is a memory report",
         "memory report sent properly");
      is(+result.Throttleable, 0, "correctly sent as non-throttleable");
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
      for(let x in result) {
        if (x in SubmittedCrash.extra)
          is(result[x], SubmittedCrash.extra[x],
             "submitted value for " + x + " matches expected");
        else
          ok(false, "property " + x + " missing from submitted data!");
      }
      for(let y in SubmittedCrash.extra) {
        if (!(y in result))
          ok(false, "property " + y + " missing from result data!");
      }

      // NB: Despite appearances, this doesn't use a CPOW.
      BrowserTestUtils.waitForEvent(browser, "pageshow", true).then(csp_pageshow);

      // now navigate back
      browser.goBack();
    });
  }
  function csp_fail() {
    browser.removeEventListener("CrashSubmitFailed", csp_fail, true);
    ok(false, "failed to submit crash report!");
    cleanup_and_finish();
  }
  browser.addEventListener("CrashSubmitFailed", csp_fail, true);
  BrowserTestUtils.browserLoaded(browser, false, (url) => url !== "about:crashes").then(csp_onload);
  function csp_pageshow() {
    ContentTask.spawn(browser, { CrashID, CrashURL }, function({ CrashID, CrashURL }) {
                  is(content.location.href, "about:crashes", "navigated back successfully");
                  let link = content.document.getElementById(CrashID);
                  isnot(link, null, "crash report link changed correctly");
                  if (link)
                    is(link.href, CrashURL, "crash report link points to correct href");
                }).then(cleanup_and_finish);
  }

  // try submitting the pending report
  for (let crash of crashes) {
    if (crash.pending) {
      SubmittedCrash = crash;
      break;
    }
  }

  ContentTask.spawn(browser, SubmittedCrash.id, function(id) {
    let link = content.document.getElementById(id);
    link.click();
  });
}

function test() {
  waitForExplicitFinish();
  let appD = make_fake_appdir();
  let crD = appD.clone();
  crD.append("Crash Reports");
  let crashes = add_fake_crashes(crD, 1);
  // we don't need much data here, it's not going to a real Socorro
  crashes.push(addPendingCrashreport(crD,
                                     crashes[crashes.length - 1].date + 60000,
                                     {'ServerURL': 'http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs',
                                      'ProductName': 'Test App',
                                      // test that we don't truncate
                                      // at = (bug 512853)
                                      'Foo': 'ABC=XYZ'
                                     }));
  crashes.sort((a,b) => b.date - a.date);

  // set this pref so we can link to our test server
  Services.prefs.setCharPref("breakpad.reportURL",
                             "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs?id=");

  BrowserTestUtils.openNewForegroundTab(gBrowser, "about:crashes").then((tab) => {
    ContentTask.spawn(tab.linkedBrowser, crashes, check_crash_list)
               .then(() => check_submit_pending(tab, crashes));
  });
}
