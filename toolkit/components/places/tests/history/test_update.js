/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests for `History.update` as implemented in History.jsm

"use strict";

add_task(async function test_error_cases() {
  Assert.throws(
    () => PlacesUtils.history.update("not an object"),
    /Error: PageInfo: Input should be a valid object/,
    "passing a string as pageInfo should throw an Error"
  );
  Assert.throws(
    () => PlacesUtils.history.update(null),
    /Error: PageInfo: Input should be/,
    "passing a null as pageInfo should throw an Error"
  );
  Assert.throws(
    () =>
      PlacesUtils.history.update({
        description: "Test description",
      }),
    /Error: PageInfo: The following properties were expected: url, guid/,
    "not included a url or a guid should throw"
  );
  Assert.throws(
    () => PlacesUtils.history.update({ url: "not a valid url string" }),
    /Error: PageInfo: Invalid value for property/,
    "passing an invalid url should throw an Error"
  );
  Assert.throws(
    () =>
      PlacesUtils.history.update({
        url: "http://valid.uri.com",
        description: 123,
      }),
    /Error: PageInfo: Invalid value for property/,
    "passing a non-string description in pageInfo should throw an Error"
  );
  Assert.throws(
    () =>
      PlacesUtils.history.update({
        url: "http://valid.uri.com",
        guid: "invalid guid",
        description: "Test description",
      }),
    /Error: PageInfo: Invalid value for property/,
    "passing a invalid guid in pageInfo should throw an Error"
  );
  Assert.throws(
    () =>
      PlacesUtils.history.update({
        url: "http://valid.uri.com",
        previewImageURL: "not a valid url string",
      }),
    /Error: PageInfo: Invalid value for property/,
    "passing an invlid preview image url in pageInfo should throw an Error"
  );
  Assert.throws(
    () => {
      let imageName = "a-very-long-string".repeat(10000);
      let previewImageURL = `http://valid.uri.com/${imageName}.png`;
      PlacesUtils.history.update({
        url: "http://valid.uri.com",
        previewImageURL,
      });
    },
    /Error: PageInfo: Invalid value for property/,
    "passing an oversized previewImageURL in pageInfo should throw an Error"
  );
  Assert.throws(
    () => PlacesUtils.history.update({ url: "http://valid.uri.com" }),
    /TypeError: pageInfo object must at least/,
    "passing a pageInfo with neither description, previewImageURL, nor annotations should throw a TypeError"
  );
  Assert.throws(
    () =>
      PlacesUtils.history.update({
        url: "http://valid.uri.com",
        annotations: "asd",
      }),
    /Error: PageInfo: Invalid value for property/,
    "passing a pageInfo with incorrect annotations type should throw an Error"
  );
  Assert.throws(
    () =>
      PlacesUtils.history.update({
        url: "http://valid.uri.com",
        annotations: new Map(),
      }),
    /Error: PageInfo: Invalid value for property/,
    "passing a pageInfo with an empty annotations type should throw an Error"
  );
  Assert.throws(
    () =>
      PlacesUtils.history.update({
        url: "http://valid.uri.com",
        annotations: new Map([[1234, "value"]]),
      }),
    /Error: PageInfo: Invalid value for property/,
    "passing a pageInfo with an invalid key type should throw an Error"
  );
  Assert.throws(
    () =>
      PlacesUtils.history.update({
        url: "http://valid.uri.com",
        annotations: new Map([["test", ["myarray"]]]),
      }),
    /Error: PageInfo: Invalid value for property/,
    "passing a pageInfo with an invalid key type should throw an Error"
  );
  Assert.throws(
    () =>
      PlacesUtils.history.update({
        url: "http://valid.uri.com",
        annotations: new Map([["test", { anno: "value" }]]),
      }),
    /Error: PageInfo: Invalid value for property/,
    "passing a pageInfo with an invalid key type should throw an Error"
  );
});

add_task(async function test_description_change_saved() {
  await PlacesUtils.history.clear();

  let TEST_URL = "http://mozilla.org/test_description_change_saved";
  await PlacesTestUtils.addVisits(TEST_URL);
  Assert.ok(await PlacesTestUtils.isPageInDB(TEST_URL));

  let description = "Test description";
  await PlacesUtils.history.update({ url: TEST_URL, description });
  let descriptionInDB = await PlacesTestUtils.fieldInDB(
    TEST_URL,
    "description"
  );
  Assert.equal(
    description,
    descriptionInDB,
    "description should be updated via URL as expected"
  );

  description = "";
  await PlacesUtils.history.update({ url: TEST_URL, description });
  descriptionInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "description");
  Assert.strictEqual(
    null,
    descriptionInDB,
    "an empty description should set it to null in the database"
  );

  let guid = await PlacesTestUtils.fieldInDB(TEST_URL, "guid");
  description = "Test description";
  await PlacesUtils.history.update({ url: TEST_URL, guid, description });
  descriptionInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "description");
  Assert.equal(
    description,
    descriptionInDB,
    "description should be updated via GUID as expected"
  );

  description = "Test descipriton".repeat(1000);
  await PlacesUtils.history.update({ url: TEST_URL, description });
  descriptionInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "description");
  Assert.ok(
    !!descriptionInDB.length < description.length,
    "a long description should be truncated"
  );

  description = null;
  await PlacesUtils.history.update({ url: TEST_URL, description });
  descriptionInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "description");
  Assert.strictEqual(
    description,
    descriptionInDB,
    "a null description should set it to null in the database"
  );
});

add_task(async function test_previewImageURL_change_saved() {
  await PlacesUtils.history.clear();

  let TEST_URL = "http://mozilla.org/test_previewImageURL_change_saved";
  let IMAGE_URL = "http://mozilla.org/test_preview_image.png";
  await PlacesTestUtils.addVisits(TEST_URL);
  Assert.ok(await PlacesTestUtils.isPageInDB(TEST_URL));

  let previewImageURL = IMAGE_URL;
  await PlacesUtils.history.update({ url: TEST_URL, previewImageURL });
  let previewImageURLInDB = await PlacesTestUtils.fieldInDB(
    TEST_URL,
    "preview_image_url"
  );
  Assert.equal(
    previewImageURL,
    previewImageURLInDB,
    "previewImageURL should be updated via URL as expected"
  );

  previewImageURL = null;
  await PlacesUtils.history.update({ url: TEST_URL, previewImageURL });
  previewImageURLInDB = await PlacesTestUtils.fieldInDB(
    TEST_URL,
    "preview_image_url"
  );
  Assert.strictEqual(
    null,
    previewImageURLInDB,
    "a null previewImageURL should set it to null in the database"
  );

  let guid = await PlacesTestUtils.fieldInDB(TEST_URL, "guid");
  previewImageURL = IMAGE_URL;
  await PlacesUtils.history.update({ guid, previewImageURL });
  previewImageURLInDB = await PlacesTestUtils.fieldInDB(
    TEST_URL,
    "preview_image_url"
  );
  Assert.equal(
    previewImageURL,
    previewImageURLInDB,
    "previewImageURL should be updated via GUID as expected"
  );

  previewImageURL = "";
  await PlacesUtils.history.update({ url: TEST_URL, previewImageURL });
  previewImageURLInDB = await PlacesTestUtils.fieldInDB(
    TEST_URL,
    "preview_image_url"
  );
  Assert.strictEqual(
    null,
    previewImageURLInDB,
    "an empty previewImageURL should set it to null in the database"
  );
});

add_task(async function test_change_description_and_preview_saved() {
  await PlacesUtils.history.clear();

  let TEST_URL = "http://mozilla.org/test_change_both_saved";
  await PlacesTestUtils.addVisits(TEST_URL);
  Assert.ok(await PlacesTestUtils.isPageInDB(TEST_URL));

  let description = "Test description";
  let previewImageURL = "http://mozilla.org/test_preview_image.png";

  await PlacesUtils.history.update({
    url: TEST_URL,
    description,
    previewImageURL,
  });
  let descriptionInDB = await PlacesTestUtils.fieldInDB(
    TEST_URL,
    "description"
  );
  let previewImageURLInDB = await PlacesTestUtils.fieldInDB(
    TEST_URL,
    "preview_image_url"
  );
  Assert.equal(
    description,
    descriptionInDB,
    "description should be updated via URL as expected"
  );
  Assert.equal(
    previewImageURL,
    previewImageURLInDB,
    "previewImageURL should be updated via URL as expected"
  );

  // Update description should not touch other fields
  description = null;
  await PlacesUtils.history.update({ url: TEST_URL, description });
  descriptionInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "description");
  previewImageURLInDB = await PlacesTestUtils.fieldInDB(
    TEST_URL,
    "preview_image_url"
  );
  Assert.strictEqual(
    description,
    descriptionInDB,
    "description should be updated via URL as expected"
  );
  Assert.equal(
    previewImageURL,
    previewImageURLInDB,
    "previewImageURL should not be updated"
  );
});

/**
 * Gets annotation information from the database for the specified URL and
 * annotation name.
 *
 * @param {String} pageUrl The URL to search for.
 * @param {String} annoName The name of the annotation to search for.
 * @return {Array} An array of objects containing the annotations found.
 */
async function getAnnotationInfoFromDB(pageUrl, annoName) {
  let db = await PlacesUtils.promiseDBConnection();

  let rows = await db.execute(
    `
    SELECT a.content, a.flags, a.expiration, a.type FROM moz_anno_attributes n
    JOIN moz_annos a ON n.id = a.anno_attribute_id
    JOIN moz_places h ON h.id = a.place_id
    WHERE h.url_hash = hash(:pageUrl) AND h.url = :pageUrl
      AND n.name = :annoName
  `,
    { annoName, pageUrl }
  );

  let result = rows.map(row => {
    return {
      content: row.getResultByName("content"),
      flags: row.getResultByName("flags"),
      expiration: row.getResultByName("expiration"),
      type: row.getResultByName("type"),
    };
  });

  return result;
}

add_task(async function test_simple_change_annotations() {
  await PlacesUtils.history.clear();

  const TEST_URL = "http://mozilla.org/test_change_both_saved";
  await PlacesTestUtils.addVisits(TEST_URL);
  Assert.ok(
    await PlacesTestUtils.isPageInDB(TEST_URL),
    "Should have inserted the page into the database."
  );

  await PlacesUtils.history.update({
    url: TEST_URL,
    annotations: new Map([["test/annotation", "testContent"]]),
  });

  let pageInfo = await PlacesUtils.history.fetch(TEST_URL, {
    includeAnnotations: true,
  });

  Assert.equal(
    pageInfo.annotations.size,
    1,
    "Should have one annotation for the page"
  );
  Assert.equal(
    pageInfo.annotations.get("test/annotation"),
    "testContent",
    "Should have the correct annotation"
  );

  let annotationInfo = await getAnnotationInfoFromDB(
    TEST_URL,
    "test/annotation"
  );
  Assert.deepEqual(
    {
      content: "testContent",
      flags: 0,
      type: Ci.nsIAnnotationService.TYPE_STRING,
      expiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
    },
    annotationInfo[0],
    "Should have stored the correct annotation data in the db"
  );

  await PlacesUtils.history.update({
    url: TEST_URL,
    annotations: new Map([["test/annotation2", "testAnno"]]),
  });

  pageInfo = await PlacesUtils.history.fetch(TEST_URL, {
    includeAnnotations: true,
  });

  Assert.equal(
    pageInfo.annotations.size,
    2,
    "Should have two annotations for the page"
  );
  Assert.equal(
    pageInfo.annotations.get("test/annotation"),
    "testContent",
    "Should have the correct value for the first annotation"
  );
  Assert.equal(
    pageInfo.annotations.get("test/annotation2"),
    "testAnno",
    "Should have the correct value for the second annotation"
  );

  await PlacesUtils.history.update({
    url: TEST_URL,
    annotations: new Map([["test/annotation", 1234]]),
  });

  pageInfo = await PlacesUtils.history.fetch(TEST_URL, {
    includeAnnotations: true,
  });

  Assert.equal(
    pageInfo.annotations.size,
    2,
    "Should still have two annotations for the page"
  );
  Assert.equal(
    pageInfo.annotations.get("test/annotation"),
    1234,
    "Should have the updated the first annotation value"
  );
  Assert.equal(
    pageInfo.annotations.get("test/annotation2"),
    "testAnno",
    "Should have kept the value for the second annotation"
  );

  annotationInfo = await getAnnotationInfoFromDB(TEST_URL, "test/annotation");
  Assert.deepEqual(
    {
      content: 1234,
      flags: 0,
      type: Ci.nsIAnnotationService.TYPE_INT64,
      expiration: Ci.nsIAnnotationService.EXPIRE_NEVER,
    },
    annotationInfo[0],
    "Should have updated the annotation data in the db"
  );

  await PlacesUtils.history.update({
    url: TEST_URL,
    annotations: new Map([["test/annotation", null]]),
  });

  pageInfo = await PlacesUtils.history.fetch(TEST_URL, {
    includeAnnotations: true,
  });

  Assert.equal(
    pageInfo.annotations.size,
    1,
    "Should have removed only the first annotation"
  );
  Assert.strictEqual(
    pageInfo.annotations.get("test/annotation"),
    undefined,
    "Should have removed only the first annotation"
  );
  Assert.equal(
    pageInfo.annotations.get("test/annotation2"),
    "testAnno",
    "Should have kept the value for the second annotation"
  );

  await PlacesUtils.history.update({
    url: TEST_URL,
    annotations: new Map([["test/annotation2", null]]),
  });

  pageInfo = await PlacesUtils.history.fetch(TEST_URL, {
    includeAnnotations: true,
  });

  Assert.equal(pageInfo.annotations.size, 0, "Should have no annotations left");

  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`
    SELECT * FROM moz_annos
  `);
  Assert.equal(rows.length, 0, "Should be no annotations left in the db");
});

add_task(async function test_change_multiple_annotations() {
  await PlacesUtils.history.clear();

  const TEST_URL = "http://mozilla.org/test_change_both_saved";
  await PlacesTestUtils.addVisits(TEST_URL);
  Assert.ok(
    await PlacesTestUtils.isPageInDB(TEST_URL),
    "Should have inserted the page into the database."
  );

  await PlacesUtils.history.update({
    url: TEST_URL,
    annotations: new Map([
      ["test/annotation", "testContent"],
      ["test/annotation2", "testAnno"],
    ]),
  });

  let pageInfo = await PlacesUtils.history.fetch(TEST_URL, {
    includeAnnotations: true,
  });

  Assert.equal(
    pageInfo.annotations.size,
    2,
    "Should have inserted the two annotations for the page."
  );
  Assert.equal(
    pageInfo.annotations.get("test/annotation"),
    "testContent",
    "Should have the correct value for the first annotation"
  );
  Assert.equal(
    pageInfo.annotations.get("test/annotation2"),
    "testAnno",
    "Should have the correct value for the second annotation"
  );

  await PlacesUtils.history.update({
    url: TEST_URL,
    annotations: new Map([
      ["test/annotation", 123456],
      ["test/annotation2", 135246],
    ]),
  });

  pageInfo = await PlacesUtils.history.fetch(TEST_URL, {
    includeAnnotations: true,
  });

  Assert.equal(
    pageInfo.annotations.size,
    2,
    "Should have two annotations for the page"
  );
  Assert.equal(
    pageInfo.annotations.get("test/annotation"),
    123456,
    "Should have the correct value for the first annotation"
  );
  Assert.equal(
    pageInfo.annotations.get("test/annotation2"),
    135246,
    "Should have the correct value for the second annotation"
  );

  await PlacesUtils.history.update({
    url: TEST_URL,
    annotations: new Map([
      ["test/annotation", null],
      ["test/annotation2", null],
    ]),
  });

  pageInfo = await PlacesUtils.history.fetch(TEST_URL, {
    includeAnnotations: true,
  });

  Assert.equal(pageInfo.annotations.size, 0, "Should have no annotations left");
});

add_task(async function test_annotations_nonexisting_page() {
  info("Adding annotations to a non existing page should be silent");
  await PlacesUtils.history.update({
    url: "http://nonexisting.moz/",
    annotations: new Map([["test/annotation", null]]),
  });
});

add_task(async function test_annotations_nonexisting_page() {
  info("Adding annotations to a non existing page should be silent");
  await PlacesUtils.history.update({
    url: "http://nonexisting.moz/",
    annotations: new Map([["test/annotation", null]]),
  });
});
