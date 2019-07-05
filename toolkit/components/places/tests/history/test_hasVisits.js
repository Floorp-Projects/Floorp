/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests for `History.hasVisits` as implemented in History.jsm

"use strict";

add_task(async function test_has_visits_error_cases() {
  Assert.throws(
    () => PlacesUtils.history.hasVisits(),
    /TypeError: Invalid url or guid: undefined/,
    "passing a null into History.hasVisits should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.hasVisits(1),
    /TypeError: Invalid url or guid: 1/,
    "passing an invalid url into History.hasVisits should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.hasVisits({}),
    /TypeError: Invalid url or guid: \[object Object\]/,
    `passing an invalid (not of type URI or nsIURI) object to History.hasVisits
     should throw a TypeError`
  );
});

add_task(async function test_history_has_visits() {
  const TEST_URL = "http://mozilla.com/";
  await PlacesUtils.history.clear();
  Assert.equal(
    await PlacesUtils.history.hasVisits(TEST_URL),
    false,
    "Test Url should not be in history."
  );
  Assert.equal(
    await PlacesUtils.history.hasVisits(Services.io.newURI(TEST_URL)),
    false,
    "Test Url should not be in history."
  );
  await PlacesTestUtils.addVisits(TEST_URL);
  Assert.equal(
    await PlacesUtils.history.hasVisits(TEST_URL),
    true,
    "Test Url should be in history."
  );
  Assert.equal(
    await PlacesUtils.history.hasVisits(Services.io.newURI(TEST_URL)),
    true,
    "Test Url should be in history."
  );
  let guid = await PlacesTestUtils.fieldInDB(TEST_URL, "guid");
  Assert.equal(
    await PlacesUtils.history.hasVisits(guid),
    true,
    "Test Url should be in history."
  );
  await PlacesUtils.history.clear();
});
