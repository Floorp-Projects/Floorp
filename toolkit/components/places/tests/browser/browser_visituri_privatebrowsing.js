/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const INITIAL_URL = "http://example.com/tests/toolkit/components/places/tests/browser/begin.html";
const FINAL_URL = "http://example.com/tests/toolkit/components/places/tests/browser/final.html";

let gTab;

/**
 * One-time observer callback.
 */
function waitForObserve(name, callback)
{
  let observer = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
    observe: function(subject, topic, data)
    {
      Services.obs.removeObserver(observer, name);
      callback(subject, topic, data);
    }
  };

  Services.obs.addObserver(observer, name, false);
}

/**
 * One-time DOMContentLoaded callback.
 */
function waitForLoad(callback)
{
  gTab.linkedBrowser.addEventListener("load", function()
  {
    gTab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    callback();
  }, true);
}

function test()
{
  if (!("@mozilla.org/privatebrowsing;1" in Cc)) {
    todo(false, "PB service is not available, bail out");
    return;
  }

  gTab = gBrowser.selectedTab = gBrowser.addTab();

  waitForExplicitFinish();

  Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  pb.privateBrowsingEnabled = true;

  waitForObserve("uri-visit-saved", function(subject, topic, data)
  {
    let uri = subject.QueryInterface(Ci.nsIURI);
    is(uri.spec, FINAL_URL, "received expected visit");
    if (uri.spec != FINAL_URL)
      return;
    gBrowser.removeCurrentTab();
    waitForClearHistory(finish);
  });

  content.location.href = INITIAL_URL;
  waitForLoad(function()
  {
    pb.privateBrowsingEnabled = false;
    try {
      Services.prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
    } catch(ex) {}
    executeSoon(function()
    {
      content.location.href = FINAL_URL;
    });
  });
}
