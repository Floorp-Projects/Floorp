/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the normal operation of setAndFetchFaviconForPage.

let gTests = [
  {
    desc: "Normal test",
    href: "http://example.com/normal",
    loadType: PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    async setup() {
      await PlacesTestUtils.addVisits({
        uri: this.href,
        transition: TRANSITION_TYPED,
      });
    },
  },
  {
    desc: "Bookmarked about: uri",
    href: "about:testAboutURI_bookmarked",
    loadType: PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    async setup() {
      await PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        url: this.href,
      });
    },
  },
  {
    desc: "Bookmarked in private window",
    href: "http://example.com/privateBrowsing_bookmarked",
    loadType: PlacesUtils.favicons.FAVICON_LOAD_PRIVATE,
    async setup() {
      await PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        url: this.href,
      });
    },
  },
  {
    desc: "Bookmarked with disabled history",
    href: "http://example.com/disabledHistory_bookmarked",
    loadType: PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    async setup() {
      await PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        url: this.href,
      });
      Services.prefs.setBoolPref("places.history.enabled", false);
    },
    clean() {
      Services.prefs.setBoolPref("places.history.enabled", true);
    },
  },
];

add_task(async function() {
  let faviconURI = SMALLPNG_DATA_URI;
  let faviconMimeType = "image/png";

  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
  });

  for (let test of gTests) {
    info(test.desc);
    let pageURI = Services.io.newURI(test.href);

    await test.setup();

    let pageGuid;
    let promise = PlacesTestUtils.waitForNotification(
      "onPageChanged",
      (uri, prop, value, guid) => {
        pageGuid = guid;
        return (
          prop == Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON &&
          uri.equals(pageURI) &&
          value == faviconURI.spec
        );
      },
      "history"
    );

    PlacesUtils.favicons.setAndFetchFaviconForPage(
      pageURI,
      faviconURI,
      true,
      test.private,
      null,
      Services.scriptSecurityManager.getSystemPrincipal()
    );

    await promise;

    Assert.equal(
      pageGuid,
      await PlacesTestUtils.fieldInDB(pageURI, "guid"),
      "Page guid is correct"
    );
    let { dataLen, data, mimeType } = await PlacesUtils.promiseFaviconData(
      pageURI.spec
    );
    Assert.equal(faviconMimeType, mimeType, "Check expected MimeType");
    Assert.equal(
      SMALLPNG_DATA_LEN,
      data.length,
      "Check favicon data for the given page matches the provided data"
    );
    Assert.equal(
      dataLen,
      data.length,
      "Check favicon dataLen for the given page matches the provided data"
    );

    if (test.clean) {
      await test.clean();
    }
  }
});
