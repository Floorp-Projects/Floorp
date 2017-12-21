/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests for `History.insert` as implemented in History.jsm

"use strict";

add_task(async function test_insert_error_cases() {
  const TEST_URL = "http://mozilla.com";

  Assert.throws(
    () => PlacesUtils.history.insert(),
    /TypeError: pageInfo must be an object/,
    "passing a null into History.insert should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.insert(1),
    /TypeError: pageInfo must be an object/,
    "passing a non object into History.insert should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.insert({}),
    /TypeError: PageInfo object must have a url property/,
    "passing an object without a url to History.insert should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.insert({url: 123}),
    /TypeError: Invalid url or guid: 123/,
    "passing an object with an invalid url to History.insert should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.insert({url: TEST_URL}),
    /TypeError: PageInfo object must have an array of visits/,
    "passing an object without a visits property to History.insert should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.insert({url: TEST_URL, visits: 1}),
    /TypeError: PageInfo object must have an array of visits/,
    "passing an object with a non-array visits property to History.insert should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.insert({url: TEST_URL, visits: []}),
    /TypeError: PageInfo object must have an array of visits/,
    "passing an object with an empty array as the visits property to History.insert should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.insert({
      url: TEST_URL,
      visits: [
        {
          transition: TRANSITION_LINK,
          date: "a"
        }
      ]}),
    /TypeError: Expected a Date, got a/,
    "passing a visit object with an invalid date to History.insert should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.insert({
      url: TEST_URL,
      visits: [
        {
          transition: TRANSITION_LINK
        },
        {
          transition: TRANSITION_LINK,
          date: "a"
        }
      ]}),
    /TypeError: Expected a Date, got a/,
    "passing a second visit object with an invalid date to History.insert should throw a TypeError"
  );
  let futureDate = new Date();
  futureDate.setDate(futureDate.getDate() + 1000);
  Assert.throws(
    () => PlacesUtils.history.insert({
      url: TEST_URL,
      visits: [
        {
          transition: TRANSITION_LINK,
          date: futureDate,
        }
      ]}),
    `TypeError: date: ${futureDate} is not a valid date`,
    "passing a visit object with a future date to History.insert should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.insert({
      url: TEST_URL,
      visits: [
        {transition: "a"}
      ]}),
    /TypeError: transition: a is not a valid transition type/,
    "passing a visit object with an invalid transition to History.insert should throw a TypeError"
  );
});

add_task(async function test_history_insert() {
  const TEST_URL = "http://mozilla.com/";

  let inserter = async function(name, filter, referrer, date, transition) {
    info(name);
    info(`filter: ${filter}, referrer: ${referrer}, date: ${date}, transition: ${transition}`);

    let uri = NetUtil.newURI(TEST_URL + Math.random());
    let title = "Visit " + Math.random();

    let pageInfo = {
      title,
      visits: [
        {transition, referrer, date, }
      ]
    };

    pageInfo.url = await filter(uri);

    let result = await PlacesUtils.history.insert(pageInfo);

    Assert.ok(PlacesUtils.isValidGuid(result.guid), "guid for pageInfo object is valid");
    Assert.equal(uri.spec, result.url.href, "url is correct for pageInfo object");
    Assert.equal(title, result.title, "title is correct for pageInfo object");
    Assert.equal(TRANSITION_LINK, result.visits[0].transition, "transition is correct for pageInfo object");
    if (referrer) {
      Assert.equal(referrer, result.visits[0].referrer.href, "url of referrer for visit is correct");
    } else {
      Assert.equal(null, result.visits[0].referrer, "url of referrer for visit is correct");
    }
    if (date) {
      Assert.equal(Number(date),
                   Number(result.visits[0].date),
                   "date of visit is correct");
    }

    Assert.ok(await PlacesTestUtils.isPageInDB(uri), "Page was added");
    Assert.ok(await PlacesTestUtils.visitsInDB(uri), "Visit was added");
  };

  try {
    for (let referrer of [TEST_URL, null]) {
      for (let date of [new Date(), null]) {
        for (let transition of [TRANSITION_LINK, null]) {
          await inserter("Testing History.insert() with an nsIURI", x => x, referrer, date, transition);
          await inserter("Testing History.insert() with a string url", x => x.spec, referrer, date, transition);
          await inserter("Testing History.insert() with a URL object", x => new URL(x.spec), referrer, date, transition);
        }
      }
    }
  } finally {
    await PlacesTestUtils.clearHistory();
  }
});
