"use strict";

/**
 * Open a dummy page, then open about:cache and verify the opened page shows up in the cache.
 */
add_task(function*() {
  const kRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/",
                                                    "https://example.com/");
  const kTestPage = kRoot + "dummy.html";
  // Open the dummy page to get it cached.
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, kTestPage, true);
  yield BrowserTestUtils.removeTab(tab);

  tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:cache", true);
  let expectedPageCheck = function(uri) {
    info("Saw load for " + uri);
    // Can't easily use searchParms and new URL() because it's an about: URI...
    return uri.startsWith("about:cache?") && uri.includes("storage=disk");
  };
  let diskPageLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, expectedPageCheck);
  yield ContentTask.spawn(tab.linkedBrowser, null, function() {
    ok(!content.document.nodePrincipal.isSystemPrincipal,
       "about:cache should not have system principal");
    let principalURI = content.document.nodePrincipal.URI;
    let channel = content.document.docShell.currentDocumentChannel;
    ok(!channel.loadInfo.loadingPrincipal, "Loading principal should be null.");
    is(principalURI && principalURI.spec, content.document.location.href, "Principal matches location");
    let links = [... content.document.querySelectorAll("a[href*=disk]")];
    is(links.length, 1, "Should have 1 link to the disk entries");
    links[0].click();
  });
  yield diskPageLoaded;
  info("about:cache disk subpage loaded");

  expectedPageCheck = function(uri) {
    info("Saw load for " + uri);
    return uri.startsWith("about:cache-entry") && uri.includes("dummy.html");
  };
  let triggeringURISpec = tab.linkedBrowser.currentURI.spec;
  let entryLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, expectedPageCheck);
  yield ContentTask.spawn(tab.linkedBrowser, kTestPage, function(kTestPage) {
    ok(!content.document.nodePrincipal.isSystemPrincipal,
       "about:cache with query params should still not have system principal");
    let principalURI = content.document.nodePrincipal.URI;
    is(principalURI && principalURI.spec, content.document.location.href, "Principal matches location");
    let channel = content.document.docShell.currentDocumentChannel;
    principalURI = channel.loadInfo.triggeringPrincipal.URI;
    is(principalURI && principalURI.spec, "about:cache", "Triggering principal matches previous location");
    ok(!channel.loadInfo.loadingPrincipal, "Loading principal should be null.");
    let links = [... content.document.querySelectorAll("a[href*='" + kTestPage + "']")];
    is(links.length, 1, "Should have 1 link to the entry for " + kTestPage);
    links[0].click();
  });
  yield entryLoaded;
  info("about:cache entry loaded");


  yield ContentTask.spawn(tab.linkedBrowser, triggeringURISpec, function(triggeringURISpec) {
    ok(!content.document.nodePrincipal.isSystemPrincipal,
       "about:cache-entry should also not have system principal");
    let principalURI = content.document.nodePrincipal.URI;
    is(principalURI && principalURI.spec, content.document.location.href, "Principal matches location");
    let channel = content.document.docShell.currentDocumentChannel;
    principalURI = channel.loadInfo.triggeringPrincipal.URI;
    is(principalURI && principalURI.spec, triggeringURISpec, "Triggering principal matches previous location");
    ok(!channel.loadInfo.loadingPrincipal, "Loading principal should be null.");
    ok(content.document.querySelectorAll("th").length,
       "Should have several table headers with data.");
  });
  yield BrowserTestUtils.removeTab(tab);
});
