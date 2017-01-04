/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

// Tests for `History.insert` and `History.insertMany`, as implemented in History.jsm

"use strict";

add_task(function* test_insert_error_cases() {
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

add_task(function* test_history_insert() {
  const TEST_URL = "http://mozilla.com/";

  let inserter = Task.async(function*(name, filter, referrer, date, transition) {
    do_print(name);
    do_print(`filter: ${filter}, referrer: ${referrer}, date: ${date}, transition: ${transition}`);

    let uri = NetUtil.newURI(TEST_URL + Math.random());
    let title = "Visit " + Math.random();

    let pageInfo = {
      title,
      visits: [
        {transition, referrer, date, }
      ]
    };

    pageInfo.url = yield filter(uri);

    let result = yield PlacesUtils.history.insert(pageInfo);

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

    Assert.ok(yield PlacesTestUtils.isPageInDB(uri), "Page was added");
    Assert.ok(yield PlacesTestUtils.visitsInDB(uri), "Visit was added");
  });

  try {
    for (let referrer of [TEST_URL, null]) {
      for (let date of [new Date(), null]) {
        for (let transition of [TRANSITION_LINK, null]) {
          yield inserter("Testing History.insert() with an nsIURI", x => x, referrer, date, transition);
          yield inserter("Testing History.insert() with a string url", x => x.spec, referrer, date, transition);
          yield inserter("Testing History.insert() with a URL object", x => new URL(x.spec), referrer, date, transition);
        }
      }
    }
  } finally {
    yield PlacesTestUtils.clearHistory();
  }
});

add_task(function* test_insert_multiple_error_cases() {
  let validPageInfo = {
    url: "http://mozilla.com",
    visits: [
      {transition: TRANSITION_LINK}
    ]
  };

  Assert.throws(
    () => PlacesUtils.history.insertMany(),
    /TypeError: pageInfos must be an array/,
    "passing a null into History.insertMany should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.insertMany([]),
    /TypeError: pageInfos may not be an empty array/,
    "passing an empty array into History.insertMany should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.insertMany([validPageInfo, {}]),
    /TypeError: PageInfo object must have a url property/,
    "passing a second invalid PageInfo object to History.insertMany should throw a TypeError"
  );
});

add_task(function* test_history_insertMany() {
  const BAD_URLS = ["about:config", "chrome://browser/content/browser.xul"];
  const GOOD_URLS = [1, 2, 3].map(x => { return `http://mozilla.com/${x}`; });

  let makePageInfos = Task.async(function*(urls, filter = x => x) {
    let pageInfos = [];
    for (let url of urls) {
      let uri = NetUtil.newURI(url);

      let pageInfo = {
        title: `Visit to ${url}`,
        visits: [
          {transition: TRANSITION_LINK}
        ]
      };

      pageInfo.url = yield filter(uri);
      pageInfos.push(pageInfo);
    }
    return pageInfos;
  });

  let inserter = Task.async(function*(name, filter, useCallbacks) {
    do_print(name);
    do_print(`filter: ${filter}`);
    do_print(`useCallbacks: ${useCallbacks}`);
    yield PlacesTestUtils.clearHistory();

    let result;
    let allUrls = GOOD_URLS.concat(BAD_URLS);
    let pageInfos = yield makePageInfos(allUrls, filter);

    if (useCallbacks) {
      let onResultUrls = [];
      let onErrorUrls = [];
      result = yield PlacesUtils.history.insertMany(pageInfos, pageInfo => {
        let url = pageInfo.url.href;
        Assert.ok(GOOD_URLS.includes(url), "onResult callback called for correct url");
        onResultUrls.push(url);
        Assert.equal(`Visit to ${url}`, pageInfo.title, "onResult callback provides the correct title");
        Assert.ok(PlacesUtils.isValidGuid(pageInfo.guid), "onResult callback provides a valid guid");
      }, pageInfo => {
        let url = pageInfo.url.href;
        Assert.ok(BAD_URLS.includes(url), "onError callback called for correct uri");
        onErrorUrls.push(url);
        Assert.equal(undefined, pageInfo.title, "onError callback provides the correct title");
        Assert.equal(undefined, pageInfo.guid, "onError callback provides the expected guid");
      });
      Assert.equal(GOOD_URLS.sort().toString(), onResultUrls.sort().toString(), "onResult callback was called for each good url");
      Assert.equal(BAD_URLS.sort().toString(), onErrorUrls.sort().toString(), "onError callback was called for each bad url");
    } else {
      result = yield PlacesUtils.history.insertMany(pageInfos);
    }

    Assert.equal(undefined, result, "insertMany returned undefined");

    for (let url of allUrls) {
      let expected = GOOD_URLS.includes(url);
      Assert.equal(expected, yield PlacesTestUtils.isPageInDB(url), `isPageInDB for ${url} is ${expected}`);
      Assert.equal(expected, yield PlacesTestUtils.visitsInDB(url), `visitsInDB for ${url} is ${expected}`);
    }
  });

  try {
    for (let useCallbacks of [false, true]) {
      yield inserter("Testing History.insertMany() with an nsIURI", x => x, useCallbacks);
      yield inserter("Testing History.insertMany() with a string url", x => x.spec, useCallbacks);
      yield inserter("Testing History.insertMany() with a URL object", x => new URL(x.spec), useCallbacks);
    }
    // Test rejection when no items added
    let pageInfos = yield makePageInfos(BAD_URLS);
    PlacesUtils.history.insertMany(pageInfos).then(() => {
      Assert.ok(false, "History.insertMany rejected promise with all bad URLs");
    }, error => {
      Assert.equal("No items were added to history.", error.message, "History.insertMany rejected promise with all bad URLs");
    });
  } finally {
    yield PlacesTestUtils.clearHistory();
  }
});
