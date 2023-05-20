/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests setAndFetchFaviconForPage on bookmarked redirects.

add_task(async function same_host_redirect() {
  // Add a bookmarked page that redirects to another page, set a favicon on the
  // latter and check the former gets it too, if they are in the same host.
  let srcUrl = "http://bookmarked.com/";
  let destUrl = "https://other.bookmarked.com/";
  await PlacesTestUtils.addVisits([
    { uri: srcUrl, transition: TRANSITION_LINK },
    {
      uri: destUrl,
      transition: TRANSITION_REDIRECT_TEMPORARY,
      referrer: srcUrl,
    },
  ]);
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: srcUrl,
  });

  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
  });

  let promise = PlacesTestUtils.waitForNotification("favicon-changed", events =>
    events.some(e => e.url == srcUrl && e.faviconUrl == SMALLPNG_DATA_URI.spec)
  );

  PlacesUtils.favicons.setAndFetchFaviconForPage(
    Services.io.newURI(destUrl),
    SMALLPNG_DATA_URI,
    true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    null,
    Services.scriptSecurityManager.getSystemPrincipal()
  );

  await promise;

  // The favicon should be set also on the bookmarked url that redirected.
  let { dataLen } = await PlacesUtils.promiseFaviconData(srcUrl);
  Assert.equal(dataLen, SMALLPNG_DATA_LEN, "Check favicon dataLen");
});

add_task(async function other_host_redirect() {
  // Add a bookmarked page that redirects to another page, set a favicon on the
  // latter and check the former gets it too, if they are in the same host.
  let srcUrl = "http://first.com/";
  let destUrl = "https://notfirst.com/";
  await PlacesTestUtils.addVisits([
    { uri: srcUrl, transition: TRANSITION_LINK },
    {
      uri: destUrl,
      transition: TRANSITION_REDIRECT_TEMPORARY,
      referrer: srcUrl,
    },
  ]);
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: srcUrl,
  });

  let promise = Promise.race([
    PlacesTestUtils.waitForNotification("favicon-changed", events =>
      events.some(
        e => e.url == srcUrl && e.faviconUrl == SMALLPNG_DATA_URI.spec
      )
    ),
    new Promise((resolve, reject) =>
      do_timeout(300, () => reject(new Error("timeout")))
    ),
  ]);

  PlacesUtils.favicons.setAndFetchFaviconForPage(
    Services.io.newURI(destUrl),
    SMALLPNG_DATA_URI,
    true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    null,
    Services.scriptSecurityManager.getSystemPrincipal()
  );

  await Assert.rejects(promise, /timeout/);
});
