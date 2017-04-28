var { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
Cu.import("resource://gre/modules/NewTabUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.importGlobalProperties(["btoa"]);

XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

const PREF_NEWTAB_ENHANCED = "browser.newtabpage.enhanced";

// use time at the start of the tests, chnaging it inside timeDaysAgo()
// may cause tiny time differences, which break expected sql ordering
const TIME_NOW = (new Date()).getTime();

Services.prefs.setBoolPref(PREF_NEWTAB_ENHANCED, true);

do_get_profile();

// utility function to compute past timestamp in microseconds
function timeDaysAgo(numDays) {
  return (TIME_NOW - (numDays * 24 * 60 * 60 * 1000)) * 1000;
}

// tests that timestamp falls within 10 days of now
function isVisitDateOK(timestampMS) {
  let range = 10 * 24 * 60 * 60 * 1000;
  return Math.abs(Date.now() - timestampMS) < range;
}

// a set up function to prep the activity stream provider
function setUpActivityStreamTest() {
  return Task.spawn(function*() {
    yield PlacesTestUtils.clearHistory();
    yield PlacesUtils.bookmarks.eraseEverything();
    let faviconExpiredPromise = new Promise(resolve => {
      Services.obs.addObserver(resolve, "places-favicons-expired");
    });
    PlacesUtils.favicons.expireAllFavicons();
    yield faviconExpiredPromise;
  });
}

function do_check_links(actualLinks, expectedLinks) {
  Assert.ok(Array.isArray(actualLinks));
  Assert.equal(actualLinks.length, expectedLinks.length);
  for (let i = 0; i < expectedLinks.length; i++) {
    let expected = expectedLinks[i];
    let actual = actualLinks[i];
    Assert.equal(actual.url, expected.url);
    Assert.equal(actual.title, expected.title);
    Assert.equal(actual.frecency, expected.frecency);
    Assert.equal(actual.lastVisitDate, expected.lastVisitDate);
  }
}

function makeLinks(frecRangeStart, frecRangeEnd, step) {
  let links = [];
  // Remember, links are ordered by frecency descending.
  for (let i = frecRangeEnd; i > frecRangeStart; i -= step) {
    links.push(makeLink(i));
  }
  return links;
}

function makeLink(frecency) {
  return {
    url: "http://example" + frecency + ".com/",
    title: "My frecency is " + frecency,
    frecency,
    lastVisitDate: 0,
  };
}
