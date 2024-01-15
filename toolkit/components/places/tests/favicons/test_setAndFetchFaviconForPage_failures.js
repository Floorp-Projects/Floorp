/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file tests setAndFetchFaviconForPage when it is called with invalid
 * arguments, and when no favicon is stored for the given arguments.
 */

let faviconURI = Services.io.newURI(
  "http://example.org/tests/toolkit/components/places/tests/browser/favicon-normal16.png"
);
add_task(async function () {
  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
  });

  // We'll listen for favicon changes for the whole test, to ensure only the
  // last one will send a notification. Due to thread serialization, at that
  // point we can be sure previous calls didn't send a notification.
  let lastPageURI = Services.io.newURI("http://example.com/verification");
  let promiseIconChanged = PlacesTestUtils.waitForNotification(
    "favicon-changed",
    events =>
      events.some(
        e => e.url == lastPageURI.spec && e.faviconUrl == SMALLPNG_DATA_URI.spec
      )
  );

  info("Test null page uri");
  Assert.throws(
    () => {
      PlacesUtils.favicons.setAndFetchFaviconForPage(
        null,
        faviconURI,
        true,
        PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
        null,
        Services.scriptSecurityManager.getSystemPrincipal()
      );
    },
    /NS_ERROR_ILLEGAL_VALUE/,
    "Exception expected because aPageURI is null"
  );

  info("Test null favicon uri");
  Assert.throws(
    () => {
      PlacesUtils.favicons.setAndFetchFaviconForPage(
        Services.io.newURI("http://example.com/null_faviconURI"),
        null,
        true,
        PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
        null,
        Services.scriptSecurityManager.getSystemPrincipal()
      );
    },
    /NS_ERROR_ILLEGAL_VALUE/,
    "Exception expected because aFaviconURI is null."
  );

  info("Test about uri");
  PlacesUtils.favicons.setAndFetchFaviconForPage(
    Services.io.newURI("about:testAboutURI"),
    faviconURI,
    true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    null,
    Services.scriptSecurityManager.getSystemPrincipal()
  );

  info("Test private browsing non bookmarked uri");
  let pageURI = Services.io.newURI("http://example.com/privateBrowsing");
  await PlacesTestUtils.addVisits({
    uri: pageURI,
    transitionType: TRANSITION_TYPED,
  });
  PlacesUtils.favicons.setAndFetchFaviconForPage(
    pageURI,
    faviconURI,
    true,
    PlacesUtils.favicons.FAVICON_LOAD_PRIVATE,
    null,
    Services.scriptSecurityManager.getSystemPrincipal()
  );

  info("Test disabled history");
  pageURI = Services.io.newURI("http://example.com/disabledHistory");
  await PlacesTestUtils.addVisits({
    uri: pageURI,
    transition: TRANSITION_TYPED,
  });
  Services.prefs.setBoolPref("places.history.enabled", false);

  PlacesUtils.favicons.setAndFetchFaviconForPage(
    pageURI,
    faviconURI,
    true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    null,
    Services.scriptSecurityManager.getSystemPrincipal()
  );

  // The setAndFetchFaviconForPage function calls CanAddURI synchronously, thus
  // we can set the preference back to true immediately.
  Services.prefs.setBoolPref("places.history.enabled", true);

  info("Test error icon");
  // This error icon must stay in sync with FAVICON_ERRORPAGE_URL in
  // nsIFaviconService.idl and aboutNetError.html.
  let faviconErrorPageURI = Services.io.newURI(
    "chrome://global/skin/icons/info.svg"
  );
  pageURI = Services.io.newURI("http://example.com/errorIcon");
  await PlacesTestUtils.addVisits({
    uri: pageURI,
    transition: TRANSITION_TYPED,
  });
  PlacesUtils.favicons.setAndFetchFaviconForPage(
    pageURI,
    faviconErrorPageURI,
    true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    null,
    Services.scriptSecurityManager.getSystemPrincipal()
  );

  info("Test nonexisting page");
  PlacesUtils.favicons.setAndFetchFaviconForPage(
    Services.io.newURI("http://example.com/nonexistingPage"),
    faviconURI,
    true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    null,
    Services.scriptSecurityManager.getSystemPrincipal()
  );

  info("Final sanity check");
  // This is the only test that should cause the waitForFaviconChanged
  // callback to be invoked.
  await PlacesTestUtils.addVisits({
    uri: lastPageURI,
    transition: TRANSITION_TYPED,
  });
  PlacesUtils.favicons.setAndFetchFaviconForPage(
    lastPageURI,
    SMALLPNG_DATA_URI,
    true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    null,
    Services.scriptSecurityManager.getSystemPrincipal()
  );

  await promiseIconChanged;
});
