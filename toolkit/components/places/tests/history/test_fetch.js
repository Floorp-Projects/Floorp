/* Any copyright is dedicated to the Public Domain. http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function test_fetch_existent() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  // Populate places and historyvisits.
  let uriString = `http://mozilla.com/test_browserhistory/test_fetch`;
  let uri = NetUtil.newURI(uriString);
  let title = `Test Visit ${Math.random()}`;
  let dates = [];
  let visits = [];
  let transitions = [ PlacesUtils.history.TRANSITION_LINK,
                      PlacesUtils.history.TRANSITION_TYPED,
                      PlacesUtils.history.TRANSITION_BOOKMARK,
                      PlacesUtils.history.TRANSITION_REDIRECT_TEMPORARY,
                      PlacesUtils.history.TRANSITION_REDIRECT_PERMANENT,
                      PlacesUtils.history.TRANSITION_DOWNLOAD,
                      PlacesUtils.history.TRANSITION_FRAMED_LINK,
                      PlacesUtils.history.TRANSITION_RELOAD ];
  let guid = "";
  for (let i = 0; i != transitions.length; i++) {
    dates.push(new Date(Date.now() - (i * 10000000)));
    visits.push({
      uri,
      title,
      transition: transitions[i],
      visitDate: dates[i]
    });
  }
  await PlacesTestUtils.addVisits(visits);
  Assert.ok((await PlacesTestUtils.isPageInDB(uri)));
  Assert.equal(await PlacesTestUtils.visitsInDB(uri), visits.length);

  // Store guid for further use in testing.
  guid = await PlacesTestUtils.fieldInDB(uri, "guid");
  Assert.ok(guid, guid);

  // Initialize the objects to compare against.
  let idealPageInfo = {
    url: new URL(uriString), guid, title
  };
  let idealVisits = visits.map(v => {
    return {
      date: v.visitDate,
      transition: v.transition
    };
  });

  // We should check these 4 cases:
  // 1, 2: visits not included, by URL and guid (same result expected).
  // 3, 4: visits included, by URL and guid (same result expected).
  for (let includeVisits of [true, false]) {
    for (let guidOrURL of [uri, guid]) {
      let pageInfo = await PlacesUtils.history.fetch(guidOrURL, {includeVisits});
      if (includeVisits) {
        idealPageInfo.visits = idealVisits;
      } else {
        // We need to explicitly delete this property since deepEqual looks at
        // the list of properties as well (`visits in pageInfo` is true here).
        delete idealPageInfo.visits;
      }

      // Since idealPageInfo doesn't contain a frecency, check it and delete.
      Assert.ok(typeof pageInfo.frecency === "number");
      delete pageInfo.frecency;

      // Visits should be from newer to older.
      if (includeVisits) {
        for (let i = 0; i !== pageInfo.visits.length - 1; i++) {
          Assert.lessOrEqual(pageInfo.visits[i + 1].date.getTime(), pageInfo.visits[i].date.getTime());
        }
      }
      Assert.deepEqual(idealPageInfo, pageInfo);
    }
  }
});

add_task(async function test_fetch_page_meta_info() {
  await PlacesUtils.history.clear();

  let TEST_URI = NetUtil.newURI("http://mozilla.com/test_fetch_page_meta_info");
  await PlacesTestUtils.addVisits(TEST_URI);
  Assert.ok(page_in_database(TEST_URI));

  // Test fetching the null values
  let includeMeta = true;
  let pageInfo = await PlacesUtils.history.fetch(TEST_URI, {includeMeta});
  Assert.strictEqual(null, pageInfo.previewImageURL, "fetch should return a null previewImageURL");
  Assert.equal("", pageInfo.description, "fetch should return a empty string description");

  // Now set the pageMetaInfo for this page
  let description = "Test description";
  let previewImageURL = "http://mozilla.com/test_preview_image.png";
  await PlacesUtils.history.update({ url: TEST_URI, description, previewImageURL });

  includeMeta = true;
  pageInfo = await PlacesUtils.history.fetch(TEST_URI, {includeMeta});
  Assert.equal(previewImageURL, pageInfo.previewImageURL.href, "fetch should return a previewImageURL");
  Assert.equal(description, pageInfo.description, "fetch should return a description");

  includeMeta = false;
  pageInfo = await PlacesUtils.history.fetch(TEST_URI, {includeMeta});
  Assert.ok(!("description" in pageInfo), "fetch should not return a description if includeMeta is false");
  Assert.ok(!("previewImageURL" in pageInfo), "fetch should not return a previewImageURL if includeMeta is false");
});

add_task(async function test_fetch_annotations() {
  await PlacesUtils.history.clear();

  const TEST_URI = "http://mozilla.com/test_fetch_page_meta_info";
  await PlacesTestUtils.addVisits(TEST_URI);
  Assert.ok(page_in_database(TEST_URI));

  let includeAnnotations = true;
  let pageInfo = await PlacesUtils.history.fetch(TEST_URI, {includeAnnotations});
  Assert.equal(pageInfo.annotations.size, 0,
    "fetch should return an empty annotation map");

  PlacesUtils.annotations.setPageAnnotation(
    Services.io.newURI(TEST_URI),
    "test/annotation",
    "testContent",
    0,
    PlacesUtils.annotations.EXPIRE_NEVER
  );

  pageInfo = await PlacesUtils.history.fetch(TEST_URI, {includeAnnotations});
  Assert.equal(pageInfo.annotations.size, 1,
    "fetch should have only one annotation");

  Assert.equal(pageInfo.annotations.get("test/annotation"), "testContent",
    "fetch should return the expected annotation");

  PlacesUtils.annotations.setPageAnnotation(
    Services.io.newURI(TEST_URI),
    "test/annotation2",
    123,
    0,
    PlacesUtils.annotations.EXPIRE_NEVER
  );

  pageInfo = await PlacesUtils.history.fetch(TEST_URI, {includeAnnotations});
  Assert.equal(pageInfo.annotations.size, 2,
    "fetch should have returned two annotations");
  Assert.equal(pageInfo.annotations.get("test/annotation"), "testContent",
    "fetch should still have the first annotation");
  Assert.equal(pageInfo.annotations.get("test/annotation2"), 123,
    "fetch should have the second annotation");

  includeAnnotations = false;
  pageInfo = await PlacesUtils.history.fetch(TEST_URI, {includeAnnotations});
  Assert.ok(!("annotations" in pageInfo),
    "fetch should not return annotations if includeAnnotations is false");
});

add_task(async function test_fetch_nonexistent() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  let uri = NetUtil.newURI("http://doesntexist.in.db");
  let pageInfo = await PlacesUtils.history.fetch(uri);
  Assert.equal(pageInfo, null);
});

add_task(async function test_error_cases() {
  Assert.throws(
    () => PlacesUtils.history.fetch("3"),
      /TypeError: 3 is not a valid /
  );
  Assert.throws(
    () => PlacesUtils.history.fetch({not: "a valid string or guid"}),
    /TypeError: Invalid url or guid/
  );
  Assert.throws(
    () => PlacesUtils.history.fetch("http://valid.uri.com", "not an object"),
      /TypeError: options should be/
  );
  Assert.throws(
    () => PlacesUtils.history.fetch("http://valid.uri.com", null),
      /TypeError: options should be/
  );
  Assert.throws(
    () => PlacesUtils.history.fetch("http://valid.uri.come", {includeVisits: "not a boolean"}),
      /TypeError: includeVisits should be a/
  );
  Assert.throws(
    () => PlacesUtils.history.fetch("http://valid.uri.come", {includeMeta: "not a boolean"}),
      /TypeError: includeMeta should be a/
  );
  Assert.throws(
    () => PlacesUtils.history.fetch("http://valid.url.com", {includeAnnotations: "not a boolean"}),
      /TypeError: includeAnnotations should be a/
  );
});
