/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const ROOT_URI = "http://mochi.test:8888/tests/toolkit/components/places/tests/browser/";
const REDIRECT_URI = Services.io.newURI(ROOT_URI + "redirect.sjs");
const TARGET_URI = Services.io.newURI(ROOT_URI + "redirect-target.html");

const REDIRECT_SOURCE_VISIT_BONUS =
  Services.prefs.getIntPref("places.frecency.redirectSourceVisitBonus");
const LINK_VISIT_BONUS =
  Services.prefs.getIntPref("places.frecency.linkVisitBonus");
const TYPED_VISIT_BONUS =
  Services.prefs.getIntPref("places.frecency.typedVisitBonus");

// Ensure that decay frecency doesn't kick in during tests (as a result
// of idle-daily).
Services.prefs.setCharPref("places.frecency.decayRate", "1.0");

registerCleanupFunction(async function() {
  Services.prefs.clearUserPref("places.frecency.decayRate");
  await PlacesUtils.history.clear();
});

let redirectSourceFrecency = 0;
let redirectTargetFrecency = 0;

async function check_uri(uri, frecency, hidden) {
  is(await PlacesTestUtils.fieldInDB(uri, "frecency"), frecency,
    "Frecency of the page is the expected one");
  is(await PlacesTestUtils.fieldInDB(uri, "hidden"), hidden,
    "Hidden value of the page is the expected one");
}

add_task(async function redirect_check_new_typed_visit() {
  // Used to verify the redirect bonus overrides the typed bonus.
  PlacesUtils.history.markPageAsTyped(REDIRECT_URI);

  redirectSourceFrecency += REDIRECT_SOURCE_VISIT_BONUS;
  redirectTargetFrecency += TYPED_VISIT_BONUS;
  let redirectNotified = false;

  let visitedPromise = PlacesTestUtils.waitForNotification("page-visited", visits => {
    is(visits.length, 1, "Was notified for the right number of visits.");
    let {url} = visits[0];
    info("Received 'page-visited': " + url);
    if (url == REDIRECT_URI.spec) {
      redirectNotified = true;
    }
    return url == TARGET_URI.spec;
  }, "places");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, REDIRECT_URI.spec);
  info("Waiting for onVisits");
  await visitedPromise;
  ok(redirectNotified, "The redirect should have been notified");

  await check_uri(REDIRECT_URI, redirectSourceFrecency, 1);
  await check_uri(TARGET_URI, redirectTargetFrecency, 0);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function redirect_check_second_typed_visit() {
  // A second visit with a typed url.
  PlacesUtils.history.markPageAsTyped(REDIRECT_URI);

  redirectSourceFrecency += REDIRECT_SOURCE_VISIT_BONUS;
  redirectTargetFrecency += TYPED_VISIT_BONUS;
  let redirectNotified = false;

  let visitedPromise = PlacesTestUtils.waitForNotification("page-visited", visits => {
    is(visits.length, 1, "Was notified for the right number of visits.");
    let {url} = visits[0];
    info("Received 'page-visited': " + url);
    if (url == REDIRECT_URI.spec) {
      redirectNotified = true;
    }
    return url == TARGET_URI.spec;
  }, "places");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, REDIRECT_URI.spec);
  info("Waiting for onVisits");
  await visitedPromise;
  ok(redirectNotified, "The redirect should have been notified");

  await check_uri(REDIRECT_URI, redirectSourceFrecency, 1);
  await check_uri(TARGET_URI, redirectTargetFrecency, 0);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function redirect_check_subsequent_link_visit() {
  // Another visit, but this time as a visited url.
  redirectSourceFrecency += REDIRECT_SOURCE_VISIT_BONUS;
  redirectTargetFrecency += LINK_VISIT_BONUS;
  let redirectNotified = false;

  let visitedPromise = PlacesTestUtils.waitForNotification("page-visited", visits => {
    is(visits.length, 1, "Was notified for the right number of visits.");
    let {url} = visits[0];
    info("Received 'page-visited': " + url);
    if (url == REDIRECT_URI.spec) {
      redirectNotified = true;
    }
    return url == TARGET_URI.spec;
  }, "places");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, REDIRECT_URI.spec);
  info("Waiting for onVisits");
  await visitedPromise;
  ok(redirectNotified, "The redirect should have been notified");

  await check_uri(REDIRECT_URI, redirectSourceFrecency, 1);
  await check_uri(TARGET_URI, redirectTargetFrecency, 0);

  BrowserTestUtils.removeTab(tab);
});
