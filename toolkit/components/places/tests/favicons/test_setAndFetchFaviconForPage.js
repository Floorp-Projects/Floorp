/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the normal operation of setAndFetchFaviconForPage.
 */

////////////////////////////////////////////////////////////////////////////////
/// Globals

const FAVICON_URI = NetUtil.newURI(do_get_file("favicon-normal32.png"));
const FAVICON_DATA = readFileData(do_get_file("favicon-normal32.png"));
const FAVICON_MIMETYPE = "image/png";

////////////////////////////////////////////////////////////////////////////////
/// Tests

function run_test()
{
  // Check that the favicon loaded correctly before starting the actual tests.
  do_check_eq(FAVICON_DATA.length, 344);
  run_next_test();
}

add_test(function test_normal()
{
  let pageURI = NetUtil.newURI("http://example.com/normal");
  waitForFaviconChanged(pageURI, FAVICON_URI,
                        function test_normal_callback() {
    do_check_true(isUrlHidden(pageURI));
    do_check_eq(frecencyForUrl(pageURI), 0);
    checkFaviconDataForPage(pageURI, FAVICON_MIMETYPE, FAVICON_DATA,
                            run_next_test);
  });
  PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI, FAVICON_URI, true);
});

add_test(function test_aboutURI_bookmarked()
{
  let pageURI = NetUtil.newURI("about:testAboutURI_bookmarked");
  waitForFaviconChanged(pageURI, FAVICON_URI,
                        function test_aboutURI_bookmarked_callback() {
    checkFaviconDataForPage(pageURI, FAVICON_MIMETYPE, FAVICON_DATA,
                            run_next_test);
  });

  PlacesUtils.bookmarks.insertBookmark(
                          PlacesUtils.unfiledBookmarksFolderId, pageURI,
                          PlacesUtils.bookmarks.DEFAULT_INDEX, pageURI.spec);
  PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI, FAVICON_URI, true);
});

add_test(function test_privateBrowsing_bookmarked()
{
  if (!"@mozilla.org/privatebrowsing;1" in Cc) {
    run_next_next();
    return;
  }

  let pageURI = NetUtil.newURI("http://example.com/privateBrowsing_bookmarked");
  waitForFaviconChanged(pageURI, FAVICON_URI,
                        function test_privateBrowsing_bookmarked_callback() {
    checkFaviconDataForPage(pageURI, FAVICON_MIMETYPE, FAVICON_DATA,
                            run_next_test);
  });

  // Enable private browsing while changing the favicon.
  let pb = Cc["@mozilla.org/privatebrowsing;1"]
           .getService(Ci.nsIPrivateBrowsingService);
  Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session",
                             true);
  pb.privateBrowsingEnabled = true;

  PlacesUtils.bookmarks.insertBookmark(
                          PlacesUtils.unfiledBookmarksFolderId, pageURI,
                          PlacesUtils.bookmarks.DEFAULT_INDEX, pageURI.spec);
  PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI, FAVICON_URI, true);

  // The setAndFetchFaviconForPage function calls CanAddURI synchronously,
  // thus we can exit Private Browsing Mode immediately.
  pb.privateBrowsingEnabled = false;
  Services.prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
});

add_test(function test_disabledHistory_bookmarked()
{
  let pageURI = NetUtil.newURI("http://example.com/disabledHistory_bookmarked");
  waitForFaviconChanged(pageURI, FAVICON_URI,
                        function test_disabledHistory_bookmarked_callback() {
    checkFaviconDataForPage(pageURI, FAVICON_MIMETYPE, FAVICON_DATA,
                            run_next_test);
  });

  // Disable history while changing the favicon.
  Services.prefs.setBoolPref("places.history.enabled", false);

  PlacesUtils.bookmarks.insertBookmark(
                          PlacesUtils.unfiledBookmarksFolderId, pageURI,
                          PlacesUtils.bookmarks.DEFAULT_INDEX, pageURI.spec);
  PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI, FAVICON_URI, true);

  // The setAndFetchFaviconForPage function calls CanAddURI synchronously, thus
  // we can set the preference back to true immediately.  We don't clear the
  // preference because not all products enable Places by default.
  Services.prefs.setBoolPref("places.history.enabled", true);
});
