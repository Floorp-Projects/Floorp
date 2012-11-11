/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Ensure that clicking the button in the Offline mode neterror page updates
   global history. See bug 680727. */
/* TEST_PATH=toolkit/components/places/tests/browser/browser_bug680727.js make -C $(OBJDIR) mochitest-browser-chrome */


const kUniqueURI = Services.io.newURI("http://mochi.test:8888/#bug_680727",
                                      null, null);
var gAsyncHistory = 
  Cc["@mozilla.org/browser/history;1"].getService(Ci.mozIAsyncHistory);

let proxyPrefValue;

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();

  // Tests always connect to localhost, and per bug 87717, localhost is now
  // reachable in offline mode.  To avoid this, disable any proxy.
  proxyPrefValue = Services.prefs.getIntPref("network.proxy.type");
  Services.prefs.setIntPref("network.proxy.type", 0);

  // Clear network cache.
  Components.classes["@mozilla.org/network/cache-service;1"]
            .getService(Components.interfaces.nsICacheService)
            .evictEntries(Components.interfaces.nsICache.STORE_ANYWHERE);

  // Go offline, expecting the error page.
  Services.io.offline = true;
  window.addEventListener("DOMContentLoaded", errorListener, false);
  content.location = kUniqueURI.spec;
}

//------------------------------------------------------------------------------
// listen to loading the neterror page. (offline mode)
function errorListener() {
  if(content.location == "about:blank") {
    info("got about:blank, which is expected once, so return");
    return;
  }

  window.removeEventListener("DOMContentLoaded", errorListener, false);
  ok(Services.io.offline, "Services.io.offline is true.");

  // This is an error page.
  is(gBrowser.contentDocument.documentURI.substring(0, 27),
     "about:neterror?e=netOffline",
     "Document URI is the error page.");

  // But location bar should show the original request.
  is(content.location.href, kUniqueURI.spec,
     "Docshell URI is the original URI.");

  // Global history does not record URI of a failed request.
  promiseAsyncUpdates().then(function() {
    gAsyncHistory.isURIVisited(kUniqueURI, errorAsyncListener);
  });
}

function errorAsyncListener(aURI, aIsVisited) {
  ok(kUniqueURI.equals(aURI) && !aIsVisited,
     "The neterror page is not listed in global history.");

  Services.prefs.setIntPref("network.proxy.type", proxyPrefValue);

  // Now press the "Try Again" button, with offline mode off.
  Services.io.offline = false;

  window.addEventListener("DOMContentLoaded", reloadListener, false);

  ok(gBrowser.contentDocument.getElementById("errorTryAgain"),
     "The error page has got a #errorTryAgain element");
  gBrowser.contentDocument.getElementById("errorTryAgain").click();
}

//------------------------------------------------------------------------------
// listen to reload of neterror.
function reloadListener() {
  window.removeEventListener("DOMContentLoaded", reloadListener, false);

  // This listener catches "DOMContentLoaded" on being called
  // nsIWPL::onLocationChange(...). That is right *AFTER*
  // IHistory::VisitURI(...) is called.
  ok(!Services.io.offline, "Services.io.offline is false.");

  // This is not an error page.
  is(gBrowser.contentDocument.documentURI, kUniqueURI.spec, 
     "Document URI is not the offline-error page, but the original URI.");

  // Check if global history remembers the successfully-requested URI.
  promiseAsyncUpdates().then(function() {
    gAsyncHistory.isURIVisited(kUniqueURI, reloadAsyncListener);
  });
}

function reloadAsyncListener(aURI, aIsVisited) {
  ok(kUniqueURI.equals(aURI) && aIsVisited, "We have visited the URI.");
  promiseClearHistory().then(finish);
}

registerCleanupFunction(function() {
  Services.prefs.setIntPref("network.proxy.type", proxyPrefValue);
  Services.io.offline = false;
  window.removeEventListener("DOMContentLoaded", errorListener, false);
  window.removeEventListener("DOMContentLoaded", reloadListener, false);
  gBrowser.removeCurrentTab();
});
