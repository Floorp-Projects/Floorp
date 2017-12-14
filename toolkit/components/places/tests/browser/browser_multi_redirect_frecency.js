/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const ROOT_URI = "http://mochi.test:8888/tests/toolkit/components/places/tests/browser/";
const REDIRECT_URI = Services.io.newURI(ROOT_URI + "redirect_thrice.sjs");
const INTERMEDIATE_URI_1 = Services.io.newURI(ROOT_URI + "redirect_twice_perma.sjs");
const INTERMEDIATE_URI_2 = Services.io.newURI(ROOT_URI + "redirect_once.sjs");
const TARGET_URI = Services.io.newURI(ROOT_URI + "final.html");

const REDIRECT_SOURCE_VISIT_BONUS =
  Services.prefs.getIntPref("places.frecency.redirectSourceVisitBonus");
const PERM_REDIRECT_VISIT_BONUS =
  Services.prefs.getIntPref("places.frecency.permRedirectVisitBonus");

// Ensure that decay frecency doesn't kick in during tests (as a result
// of idle-daily).
Services.prefs.setCharPref("places.frecency.decayRate", "1.0");

registerCleanupFunction(async function() {
  Services.prefs.clearUserPref("places.frecency.decayRate");
  await PlacesUtils.history.clear();
});

async function check_uri(uri, frecency, hidden) {
  is(await PlacesTestUtils.fieldInDB(uri, "frecency"), frecency,
    "Frecency of the page is the expected one");
  is(await PlacesTestUtils.fieldInDB(uri, "hidden"), hidden,
    "Hidden value of the page is the expected one");
}

let redirectSourceFrecency = 0;
let redirectTargetFrecency = 0;

add_task(async function test_multiple_redirect() {
  // Used to verify the redirect bonus overrides the typed bonus.
  PlacesUtils.history.markPageAsTyped(REDIRECT_URI);

  redirectSourceFrecency += REDIRECT_SOURCE_VISIT_BONUS;
  // TODO Bug 487813 - This should be TYPED_VISIT_BONUS, however as we don't
  // currently track redirects across multiple redirects, we fallback to the
  // PERM_REDIRECT_VISIT_BONUS.
  redirectTargetFrecency += PERM_REDIRECT_VISIT_BONUS;
  let redirectNotified = false;

  let visitedPromise = PlacesTestUtils.waitForNotification("onVisits", visits => {
    is(visits.length, 1, "Was notified for the right number of visits.");
    let {uri} = visits[0];
    info("Received onVisits: " + uri.spec);
    if (uri.equals(REDIRECT_URI)) {
      redirectNotified = true;
    }
    return uri.equals(TARGET_URI);
  }, "history");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, REDIRECT_URI.spec);
  info("Waiting for onVisits");
  await visitedPromise;
  ok(redirectNotified, "The redirect should have been notified");

  await check_uri(REDIRECT_URI, redirectSourceFrecency, 1);
  await check_uri(INTERMEDIATE_URI_1, redirectSourceFrecency, 1);
  await check_uri(INTERMEDIATE_URI_2, redirectSourceFrecency, 1);
  await check_uri(TARGET_URI, redirectTargetFrecency, 0);

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function redirect_check_second_typed_visit() {
  // A second visit with a typed url.
  PlacesUtils.history.markPageAsTyped(REDIRECT_URI);

  redirectSourceFrecency += REDIRECT_SOURCE_VISIT_BONUS;
  // TODO Bug 487813 - This should be TYPED_VISIT_BONUS, however as we don't
  // currently track redirects across multiple redirects, we fallback to the
  // PERM_REDIRECT_VISIT_BONUS.
  redirectTargetFrecency += PERM_REDIRECT_VISIT_BONUS;
  let redirectNotified = false;

  let visitedPromise = PlacesTestUtils.waitForNotification("onVisits", visits => {
    Assert.equal(visits.length, 1, "Was notified for the right number of visits.");
    let {uri} = visits[0];
    info("Received onVisits: " + uri.spec);
    if (uri.equals(REDIRECT_URI)) {
      redirectNotified = true;
    }
    return uri.equals(TARGET_URI);
  }, "history");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, REDIRECT_URI.spec);
  info("Waiting for onVisits");
  await visitedPromise;
  ok(redirectNotified, "The redirect should have been notified");

  await check_uri(REDIRECT_URI, redirectSourceFrecency, 1);
  await check_uri(INTERMEDIATE_URI_1, redirectSourceFrecency, 1);
  await check_uri(INTERMEDIATE_URI_2, redirectSourceFrecency, 1);
  await check_uri(TARGET_URI, redirectTargetFrecency, 0);

  await BrowserTestUtils.removeTab(tab);
});


add_task(async function redirect_check_subsequent_link_visit() {
  // Another visit, but this time as a visited url.
  redirectSourceFrecency += REDIRECT_SOURCE_VISIT_BONUS;
  // TODO Bug 487813 - This should be TYPED_VISIT_BONUS, however as we don't
  // currently track redirects across multiple redirects, we fallback to the
  // PERM_REDIRECT_VISIT_BONUS.
  redirectTargetFrecency += PERM_REDIRECT_VISIT_BONUS;
  let redirectNotified = false;

  let visitedPromise = PlacesTestUtils.waitForNotification("onVisits", visits => {
    Assert.equal(visits.length, 1, "Was notified for the right number of visits.");
    let {uri} = visits[0];
    info("Received onVisits: " + uri.spec);
    if (uri.equals(REDIRECT_URI)) {
      redirectNotified = true;
    }
    return uri.equals(TARGET_URI);
  }, "history");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, REDIRECT_URI.spec);
  info("Waiting for onVisits");
  await visitedPromise;
  ok(redirectNotified, "The redirect should have been notified");

  await check_uri(REDIRECT_URI, redirectSourceFrecency, 1);
  await check_uri(INTERMEDIATE_URI_1, redirectSourceFrecency, 1);
  await check_uri(INTERMEDIATE_URI_2, redirectSourceFrecency, 1);
  await check_uri(TARGET_URI, redirectTargetFrecency, 0);

  await BrowserTestUtils.removeTab(tab);
});
