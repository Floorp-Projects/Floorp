// load our utility script
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);

var rootDir = getRootDirectory(gTestPath);
scriptLoader.loadSubScript(rootDir + "/aboutcrashes_utils.js", this);

function cleanup_and_finish() {
  try {
    cleanup_fake_appdir();
  } catch(ex) {}
  Services.prefs.clearUserPref("breakpad.reportURL");
  gBrowser.removeTab(gBrowser.selectedTab);
  finish();
}

/*
 * check_crash_list
 *
 * Check that the list of crashes displayed by about:crashes matches
 * the list of crashes that we placed in the pending+submitted directories.
 */
function check_crash_list(tab, crashes) {
  let doc = gBrowser.getBrowserForTab(tab).contentDocument;
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
    if (browser.contentWindow.location != 'about:crashes') {
      browser.removeEventListener("load", csp_onload, true);
      // loaded the crash report page
      ok(true, 'got submission onload');
      // grab the Crash ID here to verify later
      CrashID = browser.contentWindow.location.search.split("=")[1];
      CrashURL = browser.contentWindow.location.toString();
      // check the JSON content vs. what we submitted
      let result = JSON.parse(browser.contentDocument.documentElement.textContent);
      is(result.upload_file_minidump, "MDMP", "minidump file sent properly");
      is(result.Throttleable, 0, "correctly sent as non-throttleable");
      // we checked these, they're set by the submission process,
      // so they won't be in the "extra" data.
      delete result.upload_file_minidump;
      delete result.Throttleable;
      // Likewise, this is discarded before it gets to the server
      delete SubmittedCrash.extra.ServerURL;

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
      executeSoon(function() {
                    browser.addEventListener("pageshow", csp_pageshow, true);
                    // now navigate back
                    browser.goBack();
                  });
    }
  }
  function csp_fail() {
    browser.removeEventListener("CrashSubmitFailed", csp_fail, true);
    ok(false, "failed to submit crash report!");
    cleanup_and_finish();
  }
  browser.addEventListener("CrashSubmitFailed", csp_fail, true);
  browser.addEventListener("load", csp_onload, true);
  function csp_pageshow() {
    browser.removeEventListener("pageshow", csp_pageshow, true);
    executeSoon(function () {
                  is(browser.contentWindow.location, "about:crashes", "navigated back successfully");
                  let link = browser.contentDocument.getElementById(CrashID);
                  isnot(link, null, "crash report link changed correctly");
                  if (link)
                    is(link.href, CrashURL, "crash report link points to correct href");
                  cleanup_and_finish();
                });
  }

  // try submitting the pending report
  for each(let crash in crashes) {
    if (crash.pending) {
      SubmittedCrash = crash;
      break;
    }
  }
  EventUtils.sendMouseEvent({type:'click'}, SubmittedCrash.id,
                            browser.contentWindow);
}

function test() {
  waitForExplicitFinish();
  let appD = make_fake_appdir();
  let crD = appD.clone();
  crD.append("Crash Reports");
  let crashes = add_fake_crashes(crD, 1);
  // we don't need much data here, it's not going to a real Socorro
  crashes.push(addPendingCrashreport(crD, {'ServerURL': 'http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs',
                                           'ProductName': 'Test App',
                                           // test that we don't truncate
                                           // at = (bug 512853)
                                           'Foo': 'ABC=XYZ'
                                          }));
  crashes.sort(function(a,b) b.date - a.date);

  // set this pref so we can link to our test server
  Services.prefs.setCharPref("breakpad.reportURL",
                             "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs?id=");

  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  let browser = gBrowser.getBrowserForTab(tab);
  browser.addEventListener("load", function test_load() {
                             browser.removeEventListener("load", test_load, true);
                             executeSoon(function () {
                                           check_crash_list(tab, crashes);
                                           check_submit_pending(tab, crashes);
                                         });
                          }, true);
  browser.loadURI("about:crashes", null, null);
}
