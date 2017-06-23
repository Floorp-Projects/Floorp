/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests for `History.update` as implemented in History.jsm

"use strict";

add_task(async function test_error_cases() {
  Assert.throws(
    () => PlacesUtils.history.update("not an object"),
    /TypeError: pageInfo must be/,
    "passing a string as pageInfo should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.update(null),
    /TypeError: pageInfo must be/,
    "passing a null as pageInfo should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.update({url: "not a valid url string"}),
    /TypeError: not a valid url string is/,
    "passing an invalid url should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.update({
      url: "http://valid.uri.com",
      description: 123
    }),
    /TypeError: description property of/,
    "passing a non-string description in pageInfo should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.update({
      url: "http://valid.uri.com",
      guid: "invalid guid",
      description: "Test description"
    }),
    /TypeError: guid property of/,
    "passing a invalid guid in pageInfo should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.update({
      url: "http://valid.uri.com",
      previewImageURL: "not a valid url string"
    }),
    /TypeError: not a valid url string is/,
    "passing an invlid preview image url in pageInfo should throw a TypeError"
  );
  Assert.throws(
    () => {
      let imageName = "a-very-long-string".repeat(10000);
      let previewImageURL = `http://valid.uri.com/${imageName}.png`;
      PlacesUtils.history.update({
        url: "http://valid.uri.com",
        previewImageURL
      });
    },
    /TypeError: previewImageURL property of/,
    "passing an oversized previewImageURL in pageInfo should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.update({ url: "http://valid.uri.com" }),
    /TypeError: pageInfo object must at least/,
    "passing a pageInfo with neither description nor previewImageURL should throw a TypeError"
  );
});

add_task(async function test_description_change_saved() {
  await PlacesTestUtils.clearHistory();

  let TEST_URL = NetUtil.newURI("http://mozilla.org/test_description_change_saved");
  await PlacesTestUtils.addVisits(TEST_URL);
  Assert.ok(page_in_database(TEST_URL));

  let description = "Test description";
  await PlacesUtils.history.update({ url: TEST_URL, description });
  let descriptionInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "description");
  Assert.equal(description, descriptionInDB, "description should be updated via URL as expected");

  description = "";
  await PlacesUtils.history.update({ url: TEST_URL, description });
  descriptionInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "description");
  Assert.strictEqual(null, descriptionInDB, "an empty description should set it to null in the database");

  let guid = await PlacesTestUtils.fieldInDB(TEST_URL, "guid");
  description = "Test description";
  await PlacesUtils.history.update({ url: TEST_URL, guid, description });
  descriptionInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "description");
  Assert.equal(description, descriptionInDB, "description should be updated via GUID as expected");

  description = "Test descipriton".repeat(1000);
  await PlacesUtils.history.update({ url: TEST_URL, description });
  descriptionInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "description");
  Assert.ok(0 < descriptionInDB.length < description.length, "a long description should be truncated");

  description = null;
  await PlacesUtils.history.update({ url: TEST_URL, description});
  descriptionInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "description");
  Assert.strictEqual(description, descriptionInDB, "a null description should set it to null in the database");
});

add_task(async function test_previewImageURL_change_saved() {
  await PlacesTestUtils.clearHistory();

  let TEST_URL = NetUtil.newURI("http://mozilla.org/test_previewImageURL_change_saved");
  let IMAGE_URL = "http://mozilla.org/test_preview_image.png";
  await PlacesTestUtils.addVisits(TEST_URL);
  Assert.ok(page_in_database(TEST_URL));

  let previewImageURL = IMAGE_URL;
  await PlacesUtils.history.update({ url: TEST_URL, previewImageURL });
  let previewImageURLInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "preview_image_url");
  Assert.equal(previewImageURL, previewImageURLInDB, "previewImageURL should be updated via URL as expected");

  previewImageURL = null;
  await PlacesUtils.history.update({ url: TEST_URL, previewImageURL});
  previewImageURLInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "preview_image_url");
  Assert.strictEqual(null, previewImageURLInDB, "a null previewImageURL should set it to null in the database");

  let guid = await PlacesTestUtils.fieldInDB(TEST_URL, "guid");
  previewImageURL = IMAGE_URL;
  await PlacesUtils.history.update({ url: TEST_URL, guid, previewImageURL });
  previewImageURLInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "preview_image_url");
  Assert.equal(previewImageURL, previewImageURLInDB, "previewImageURL should be updated via GUID as expected");
});

add_task(async function test_change_both_saved() {
  await PlacesTestUtils.clearHistory();

  let TEST_URL = NetUtil.newURI("http://mozilla.org/test_change_both_saved");
  await PlacesTestUtils.addVisits(TEST_URL);
  Assert.ok(page_in_database(TEST_URL));

  let description = "Test description";
  let previewImageURL = "http://mozilla.org/test_preview_image.png";

  await PlacesUtils.history.update({ url: TEST_URL, description, previewImageURL });
  let descriptionInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "description");
  let previewImageURLInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "preview_image_url");
  Assert.equal(description, descriptionInDB, "description should be updated via URL as expected");
  Assert.equal(previewImageURL, previewImageURLInDB, "previewImageURL should be updated via URL as expected");

  // Update description should not touch other fields
  description = null;
  await PlacesUtils.history.update({ url: TEST_URL, description });
  descriptionInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "description");
  previewImageURLInDB = await PlacesTestUtils.fieldInDB(TEST_URL, "preview_image_url");
  Assert.strictEqual(description, descriptionInDB, "description should be updated via URL as expected");
  Assert.equal(previewImageURL, previewImageURLInDB, "previewImageURL should not be updated");
});
