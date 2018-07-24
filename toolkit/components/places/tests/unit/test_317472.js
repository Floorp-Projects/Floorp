/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const charset = "UTF-8";
const CHARSET_ANNO = PlacesUtils.CHARSET_ANNO;

const TEST_URI = uri("http://foo.com");
const TEST_BOOKMARKED_URI = uri("http://bar.com");

add_task(async function test_execute() {
  // add pages to history
  await PlacesTestUtils.addVisits(TEST_URI);
  await PlacesTestUtils.addVisits(TEST_BOOKMARKED_URI);

  // create bookmarks on TEST_BOOKMARKED_URI
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_BOOKMARKED_URI,
    title: TEST_BOOKMARKED_URI.spec,
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: TEST_BOOKMARKED_URI,
    title: TEST_BOOKMARKED_URI.spec,
  });

  // set charset on not-bookmarked page
  await PlacesUtils.setCharsetForURI(TEST_URI, charset);
  // set charset on bookmarked page
  await PlacesUtils.setCharsetForURI(TEST_BOOKMARKED_URI, charset);

  // check that we have created a page annotation
  Assert.equal(PlacesUtils.annotations.getPageAnnotation(TEST_URI, CHARSET_ANNO), charset);

  let pageInfo = await PlacesUtils.history.fetch(TEST_URI, {includeAnnotations: true});
  Assert.equal(pageInfo.annotations.get(PlacesUtils.CHARSET_ANNO), charset,
    "Should return correct charset for a not-bookmarked page");

  pageInfo = await PlacesUtils.history.fetch(TEST_BOOKMARKED_URI, {includeAnnotations: true});
  Assert.equal(pageInfo.annotations.get(PlacesUtils.CHARSET_ANNO), charset,
    "Should return correct charset for a bookmarked page");

  await PlacesUtils.history.clear();

  pageInfo = await PlacesUtils.history.fetch(TEST_URI, {includeAnnotations: true});
  Assert.ok(!pageInfo, "Should not return pageInfo for a page after history cleared");

  // check that page annotation has been removed
  try {
    PlacesUtils.annotations.getPageAnnotation(TEST_URI, CHARSET_ANNO);
    do_throw("Charset page annotation has not been removed correctly");
  } catch (e) {}

  pageInfo = await PlacesUtils.history.fetch(TEST_BOOKMARKED_URI, {includeAnnotations: true});
  Assert.equal(pageInfo.annotations.get(PlacesUtils.CHARSET_ANNO), charset,
    "Charset should still be set for a bookmarked page after history clear");

  await PlacesUtils.setCharsetForURI(TEST_BOOKMARKED_URI, "");
  pageInfo = await PlacesUtils.history.fetch(TEST_BOOKMARKED_URI, {includeAnnotations: true});
  Assert.notEqual(pageInfo.annotations.get(PlacesUtils.CHARSET_ANNO), charset,
    "Should not have a charset after it has been removed from the page");
});
