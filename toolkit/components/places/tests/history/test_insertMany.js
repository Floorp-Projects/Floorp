/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests for `History.insertMany` as implemented in History.jsm

"use strict";

add_task(async function test_error_cases() {
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

add_task(async function test_insertMany() {
  const BAD_URLS = ["about:config", "chrome://browser/content/browser.xul"];
  const GOOD_URLS = [1, 2, 3].map(x => { return `http://mozilla.com/${x}`; });

  let makePageInfos = async function(urls, filter = x => x) {
    let pageInfos = [];
    for (let url of urls) {
      let uri = NetUtil.newURI(url);

      let pageInfo = {
        title: `Visit to ${url}`,
        visits: [
          {transition: TRANSITION_LINK}
        ]
      };

      pageInfo.url = await filter(uri);
      pageInfos.push(pageInfo);
    }
    return pageInfos;
  };

  let inserter = async function(name, filter, useCallbacks) {
    function promiseManyFrecenciesChanged() {
      return new Promise((resolve, reject) => {
        let obs = new NavHistoryObserver();
        obs.onManyFrecenciesChanged = () => {
          PlacesUtils.history.removeObserver(obs);
          resolve();
        };
        obs.onFrecencyChanged = () => {
          PlacesUtils.history.removeObserver(obs);
          reject();
        };
        PlacesUtils.history.addObserver(obs);
      });
    }

    info(name);
    info(`filter: ${filter}`);
    info(`useCallbacks: ${useCallbacks}`);
    await PlacesTestUtils.clearHistory();

    let result;
    let allUrls = GOOD_URLS.concat(BAD_URLS);
    let pageInfos = await makePageInfos(allUrls, filter);

    if (useCallbacks) {
      let onResultUrls = [];
      let onErrorUrls = [];
      result = await PlacesUtils.history.insertMany(pageInfos, pageInfo => {
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
      let promiseManyFrecencies = promiseManyFrecenciesChanged();
      result = await PlacesUtils.history.insertMany(pageInfos);
      await promiseManyFrecencies;
    }

    Assert.equal(undefined, result, "insertMany returned undefined");

    for (let url of allUrls) {
      let expected = GOOD_URLS.includes(url);
      Assert.equal(expected, await PlacesTestUtils.isPageInDB(url), `isPageInDB for ${url} is ${expected}`);
      Assert.equal(expected, await PlacesTestUtils.visitsInDB(url), `visitsInDB for ${url} is ${expected}`);
    }
  };

  try {
    for (let useCallbacks of [false, true]) {
      await inserter("Testing History.insertMany() with an nsIURI", x => x, useCallbacks);
      await inserter("Testing History.insertMany() with a string url", x => x.spec, useCallbacks);
      await inserter("Testing History.insertMany() with a URL object", x => new URL(x.spec), useCallbacks);
    }
    // Test rejection when no items added
    let pageInfos = await makePageInfos(BAD_URLS);
    PlacesUtils.history.insertMany(pageInfos).then(() => {
      Assert.ok(false, "History.insertMany rejected promise with all bad URLs");
    }, error => {
      Assert.equal("No items were added to history.", error.message, "History.insertMany rejected promise with all bad URLs");
    });
  } finally {
    await PlacesTestUtils.clearHistory();
  }
});

add_task(async function test_transitions() {
  const places = Object.keys(PlacesUtils.history.TRANSITIONS).map(transition => {
    return { url: `http://places.test/${transition}`,
             visits: [
               { transition: PlacesUtils.history.TRANSITIONS[transition] }
             ]
           };
  });
  // Should not reject.
  await PlacesUtils.history.insertMany(places);
  // Check callbacks.
  let count = 0;
  await PlacesUtils.history.insertMany(places, pageInfo => {
    ++count;
  });
  Assert.equal(count, Object.keys(PlacesUtils.history.TRANSITIONS).length);
});

add_task(async function test_guid() {
  const guidA = "aaaaaaaaaaaa";
  const guidB = "bbbbbbbbbbbb";
  const guidC = "cccccccccccc";

  await PlacesUtils.history.insertMany([
    {
      title: "foo",
      url: "http://example.com/foo",
      guid: guidA,
      visits: [
        { transition: TRANSITION_LINK, date: new Date() }
      ]
    }
  ]);

  Assert.ok(await PlacesUtils.history.fetch(guidA),
            "Record is inserted with correct GUID");

  let expectedGuids = new Set([guidB, guidC]);
  await PlacesUtils.history.insertMany([
    {
      title: "bar",
      url: "http://example.com/bar",
      guid: guidB,
      visits: [
        { transition: TRANSITION_LINK, date: new Date() }
      ]
    },
    {
      title: "baz",
      url: "http://example.com/baz",
      guid: guidC,
      visits: [
        { transition: TRANSITION_LINK, date: new Date() }
      ]
    }
  ], pageInfo => {
    Assert.ok(expectedGuids.has(pageInfo.guid));
    expectedGuids.delete(pageInfo.guid);
  });
  Assert.equal(expectedGuids.size, 0);


  Assert.ok(await PlacesUtils.history.fetch(guidB), "Record B is fetchable after insertMany");
  Assert.ok(await PlacesUtils.history.fetch(guidC), "Record C is fetchable after insertMany");
});
