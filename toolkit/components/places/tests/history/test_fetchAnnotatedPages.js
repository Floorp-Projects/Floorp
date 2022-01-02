/* Any copyright is dedicated to the Public Domain. http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function test_error_cases() {
  Assert.throws(
    () => PlacesUtils.history.fetchAnnotatedPages(),
    /TypeError: annotations should be an Array and not null/,
    "Should throw an exception for a null parameter"
  );
  Assert.throws(
    () => PlacesUtils.history.fetchAnnotatedPages("3"),
    /TypeError: annotations should be an Array and not null/,
    "Should throw an exception for a parameter of the wrong type"
  );
  Assert.throws(
    () => PlacesUtils.history.fetchAnnotatedPages([3]),
    /TypeError: all annotation values should be strings/,
    "Should throw an exception for a non-string annotation name"
  );
});

add_task(async function test_fetchAnnotatedPages_no_matching() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  const TEST_URL = "http://example.com/1";
  await PlacesTestUtils.addVisits(TEST_URL);
  Assert.ok(
    await PlacesTestUtils.isPageInDB(TEST_URL),
    "Should have inserted the page into the database."
  );

  let result = await PlacesUtils.history.fetchAnnotatedPages(["test/anno"]);

  Assert.equal(result.size, 0, "Should be no items returned.");
});

add_task(async function test_fetchAnnotatedPages_simple_match() {
  await PlacesUtils.history.clear();

  const TEST_URL = "http://example.com/1";
  await PlacesTestUtils.addVisits(TEST_URL);
  Assert.ok(
    await PlacesTestUtils.isPageInDB(TEST_URL),
    "Should have inserted the page into the database."
  );

  await PlacesUtils.history.update({
    url: TEST_URL,
    annotations: new Map([["test/anno", "testContent"]]),
  });

  let result = await PlacesUtils.history.fetchAnnotatedPages(["test/anno"]);

  Assert.equal(
    result.size,
    1,
    "Should have returned one match for the annotation"
  );

  Assert.deepEqual(
    result.get("test/anno"),
    [
      {
        uri: new URL(TEST_URL),
        content: "testContent",
      },
    ],
    "Should have returned the page and its content for the annotation"
  );
});

add_task(async function test_fetchAnnotatedPages_multiple_match() {
  await PlacesUtils.history.clear();

  const TEST_URL1 = "http://example.com/1";
  const TEST_URL2 = "http://example.com/2";
  const TEST_URL3 = "http://example.com/3";
  await PlacesTestUtils.addVisits([
    { uri: TEST_URL1 },
    { uri: TEST_URL2 },
    { uri: TEST_URL3 },
  ]);
  Assert.ok(
    await PlacesTestUtils.isPageInDB(TEST_URL1),
    "Should have inserted the first page into the database."
  );
  Assert.ok(
    await PlacesTestUtils.isPageInDB(TEST_URL2),
    "Should have inserted the second page into the database."
  );
  Assert.ok(
    await PlacesTestUtils.isPageInDB(TEST_URL3),
    "Should have inserted the third page into the database."
  );

  await PlacesUtils.history.update({
    url: TEST_URL1,
    annotations: new Map([["test/anno", "testContent1"]]),
  });

  await PlacesUtils.history.update({
    url: TEST_URL2,
    annotations: new Map([
      ["test/anno", "testContent2"],
      ["test/anno2", 1234],
    ]),
  });

  let result = await PlacesUtils.history.fetchAnnotatedPages([
    "test/anno",
    "test/anno2",
  ]);

  Assert.equal(
    result.size,
    2,
    "Should have returned matches for both annotations"
  );

  Assert.deepEqual(
    result.get("test/anno"),
    [
      {
        uri: new URL(TEST_URL1),
        content: "testContent1",
      },
      {
        uri: new URL(TEST_URL2),
        content: "testContent2",
      },
    ],
    "Should have returned two pages and their content for the first annotation"
  );

  Assert.deepEqual(
    result.get("test/anno2"),
    [
      {
        uri: new URL(TEST_URL2),
        content: 1234,
      },
    ],
    "Should have returned one page for the second annotation"
  );
});
