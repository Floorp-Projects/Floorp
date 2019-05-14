/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Ensure that clicking the button in the Offline mode neterror page updates
   global history. See bug 680727. */
/* TEST_PATH=toolkit/components/places/tests/browser/browser_bug680727.js make -C $(OBJDIR) mochitest-browser-chrome */


const kUniqueURI = Services.io.newURI("http://mochi.test:8888/#bug_680727");
var proxyPrefValue;
var ourTab;

function test() {
  waitForExplicitFinish();

  // Tests always connect to localhost, and per bug 87717, localhost is now
  // reachable in offline mode.  To avoid this, disable any proxy.
  proxyPrefValue = Services.prefs.getIntPref("network.proxy.type");
  Services.prefs.setIntPref("network.proxy.type", 0);

  // Clear network cache.
  Services.cache2.clear();

  // Go offline, expecting the error page.
  Services.io.offline = true;

  BrowserTestUtils.openNewForegroundTab(gBrowser).then(tab => {
    ourTab = tab;
    BrowserTestUtils.waitForContentEvent(ourTab.linkedBrowser, "DOMContentLoaded")
                    .then(errorListener);
    BrowserTestUtils.loadURI(ourTab.linkedBrowser, kUniqueURI.spec);
  });
}

// ------------------------------------------------------------------------------
// listen to loading the neterror page. (offline mode)
function errorListener() {
  ok(Services.io.offline, "Services.io.offline is true.");

  // This is an error page.
  ContentTask.spawn(ourTab.linkedBrowser, kUniqueURI.spec, function(uri) {
    Assert.equal(content.document.documentURI.substring(0, 27),
      "about:neterror?e=netOffline", "Document URI is the error page.");

    // But location bar should show the original request.
    Assert.equal(content.location.href, uri, "Docshell URI is the original URI.");
  }).then(() => {
    // Global history does not record URI of a failed request.
    PlacesTestUtils.promiseAsyncUpdates().then(() => {
      PlacesUtils.history.hasVisits(kUniqueURI).then(isVisited => {
        errorAsyncListener(kUniqueURI, isVisited);
      });
    });
  });
}

function errorAsyncListener(aURI, aIsVisited) {
  ok(kUniqueURI.equals(aURI) && !aIsVisited,
     "The neterror page is not listed in global history.");

  Services.prefs.setIntPref("network.proxy.type", proxyPrefValue);

  // Now press the "Try Again" button, with offline mode off.
  Services.io.offline = false;

  BrowserTestUtils.waitForContentEvent(ourTab.linkedBrowser, "DOMContentLoaded")
                  .then(reloadListener);

  ContentTask.spawn(ourTab.linkedBrowser, null, function() {
    Assert.ok(content.document.querySelector("#netErrorButtonContainer > .try-again"),
      "The error page has got a .try-again element");
    content.document.querySelector("#netErrorButtonContainer > .try-again").click();
  });
}

// ------------------------------------------------------------------------------
// listen to reload of neterror.
function reloadListener() {
  // This listener catches "DOMContentLoaded" on being called
  // nsIWPL::onLocationChange(...). That is right *AFTER*
  // IHistory::VisitURI(...) is called.
  ok(!Services.io.offline, "Services.io.offline is false.");

  ContentTask.spawn(ourTab.linkedBrowser, kUniqueURI.spec, function(uri) {
    // This is not an error page.
    Assert.equal(content.document.documentURI, uri,
      "Document URI is not the offline-error page, but the original URI.");
  }).then(() => {
    // Check if global history remembers the successfully-requested URI.
    PlacesTestUtils.promiseAsyncUpdates().then(() => {
      PlacesUtils.history.hasVisits(kUniqueURI).then(isVisited => {
          reloadAsyncListener(kUniqueURI, isVisited);
      });
    });
  });
}

function reloadAsyncListener(aURI, aIsVisited) {
  ok(kUniqueURI.equals(aURI) && aIsVisited, "We have visited the URI.");
  PlacesUtils.history.clear().then(finish);
}

registerCleanupFunction(async function() {
  Services.prefs.setIntPref("network.proxy.type", proxyPrefValue);
  Services.io.offline = false;
  BrowserTestUtils.removeTab(ourTab);
});
