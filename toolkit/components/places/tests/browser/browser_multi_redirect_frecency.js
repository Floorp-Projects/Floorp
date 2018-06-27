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
const TYPED_VISIT_BONUS =
  Services.prefs.getIntPref("places.frecency.typedVisitBonus");

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

async function waitVisitedNotifications() {
  let redirectNotified = false;
  await PlacesTestUtils.waitForNotification("onVisits", visits => {
    is(visits.length, 1, "Was notified for the right number of visits.");
    let {uri} = visits[0];
    info("Received onVisits: " + uri.spec);
    if (uri.equals(REDIRECT_URI)) {
      redirectNotified = true;
    }
    return uri.equals(TARGET_URI);
  }, "history");
  return redirectNotified;
}

let firstRedirectBonus = 0;
let nextRedirectBonus = 0;
let targetBonus = 0;

add_task(async function test_multiple_redirect() {
  // The redirect source bonus overrides the link bonus.
  let visitedPromise = waitVisitedNotifications();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: REDIRECT_URI.spec,
  }, async function() {
    info("Waiting for onVisits");
    let redirectNotified = await visitedPromise;
    ok(redirectNotified, "The redirect should have been notified");

    firstRedirectBonus += REDIRECT_SOURCE_VISIT_BONUS;
    await check_uri(REDIRECT_URI, firstRedirectBonus, 1);
    nextRedirectBonus += REDIRECT_SOURCE_VISIT_BONUS;
    await check_uri(INTERMEDIATE_URI_1, nextRedirectBonus, 1);
    await check_uri(INTERMEDIATE_URI_2, nextRedirectBonus, 1);
    // TODO Bug 487813 - This should be TYPED_VISIT_BONUS, however as we don't
    // currently track redirects across multiple redirects, we fallback to the
    // PERM_REDIRECT_VISIT_BONUS.
    targetBonus += PERM_REDIRECT_VISIT_BONUS;
    await check_uri(TARGET_URI, targetBonus, 0);
  });
});

add_task(async function test_multiple_redirect_typed() {
  // The typed bonus wins because the redirect is permanent.
  PlacesUtils.history.markPageAsTyped(REDIRECT_URI);
  let visitedPromise = waitVisitedNotifications();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: REDIRECT_URI.spec,
  }, async function() {
    info("Waiting for onVisits");
    let redirectNotified = await visitedPromise;
    ok(redirectNotified, "The redirect should have been notified");

    firstRedirectBonus += TYPED_VISIT_BONUS;
    await check_uri(REDIRECT_URI, firstRedirectBonus, 1);
    nextRedirectBonus += REDIRECT_SOURCE_VISIT_BONUS;
    await check_uri(INTERMEDIATE_URI_1, nextRedirectBonus, 1);
    await check_uri(INTERMEDIATE_URI_2, nextRedirectBonus, 1);
    // TODO Bug 487813 - This should be TYPED_VISIT_BONUS, however as we don't
    // currently track redirects across multiple redirects, we fallback to the
    // PERM_REDIRECT_VISIT_BONUS.
    targetBonus += PERM_REDIRECT_VISIT_BONUS;
    await check_uri(TARGET_URI, targetBonus, 0);
  });
});

add_task(async function test_second_typed_visit() {
  // The typed bonus wins because the redirect is permanent.
  PlacesUtils.history.markPageAsTyped(REDIRECT_URI);
  let visitedPromise = waitVisitedNotifications();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: REDIRECT_URI.spec,
  }, async function() {
    info("Waiting for onVisits");
    let redirectNotified = await visitedPromise;
    ok(redirectNotified, "The redirect should have been notified");

    firstRedirectBonus += TYPED_VISIT_BONUS;
    await check_uri(REDIRECT_URI, firstRedirectBonus, 1);
    nextRedirectBonus += REDIRECT_SOURCE_VISIT_BONUS;
    await check_uri(INTERMEDIATE_URI_1, nextRedirectBonus, 1);
    await check_uri(INTERMEDIATE_URI_2, nextRedirectBonus, 1);
    // TODO Bug 487813 - This should be TYPED_VISIT_BONUS, however as we don't
    // currently track redirects across multiple redirects, we fallback to the
    // PERM_REDIRECT_VISIT_BONUS.
    targetBonus += PERM_REDIRECT_VISIT_BONUS;
    await check_uri(TARGET_URI, targetBonus, 0);
  });
});

add_task(async function test_subsequent_link_visit() {
  // Another non typed visit.
  let visitedPromise = waitVisitedNotifications();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: REDIRECT_URI.spec,
  }, async function() {
    info("Waiting for onVisits");
    let redirectNotified = await visitedPromise;
    ok(redirectNotified, "The redirect should have been notified");

    firstRedirectBonus += REDIRECT_SOURCE_VISIT_BONUS;
    await check_uri(REDIRECT_URI, firstRedirectBonus, 1);
    nextRedirectBonus += REDIRECT_SOURCE_VISIT_BONUS;
    await check_uri(INTERMEDIATE_URI_1, nextRedirectBonus, 1);
    await check_uri(INTERMEDIATE_URI_2, nextRedirectBonus, 1);
    // TODO Bug 487813 - This should be TYPED_VISIT_BONUS, however as we don't
    // currently track redirects across multiple redirects, we fallback to the
    // PERM_REDIRECT_VISIT_BONUS.
    targetBonus += PERM_REDIRECT_VISIT_BONUS;
    await check_uri(TARGET_URI, targetBonus, 0);
  });
});
