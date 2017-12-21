/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /**
  * Check for correct functionality of PlacesUtils.setAnnotationsForItem/URI
  */

var as = PlacesUtils.annotations;

const TEST_URL = "http://test.mozilla.org/";

add_task(async function test_setAnnotationsFor() {
  let testURI = uri(TEST_URL);
  // add a bookmark
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "test",
    url: testURI,
  });

  let itemId = await PlacesUtils.promiseItemId(bookmark.guid);

  // create annotations array
  var testAnnos = [{ name: "testAnno/test0",
                     flags: 0,
                     value: "test0",
                     expires: Ci.nsIAnnotationService.EXPIRE_NEVER },
                   { name: "testAnno/test1",
                     flags: 0,
                     value: "test1",
                     expires: Ci.nsIAnnotationService.EXPIRE_NEVER },
                   { name: "testAnno/test2",
                     flags: 0,
                     value: "test2",
                     expires: Ci.nsIAnnotationService.EXPIRE_NEVER },
                   { name: "testAnno/test3",
                     flags: 0,
                     value: 0,
                     expires: Ci.nsIAnnotationService.EXPIRE_NEVER }];

  // Add item annotations
  PlacesUtils.setAnnotationsForItem(itemId, testAnnos);
  // Check for correct addition
  testAnnos.forEach(function(anno) {
    Assert.ok(as.itemHasAnnotation(itemId, anno.name));
    Assert.equal(as.getItemAnnotation(itemId, anno.name), anno.value);
  });

  // Add page annotations
  PlacesUtils.setAnnotationsForURI(testURI, testAnnos);
  // Check for correct addition
  testAnnos.forEach(function(anno) {
    Assert.ok(as.pageHasAnnotation(testURI, anno.name));
    Assert.equal(as.getPageAnnotation(testURI, anno.name), anno.value);
  });

  // To unset annotations we unset their values or set them to
  // null/undefined
  testAnnos[0].value = null;
  testAnnos[1].value = undefined;
  delete testAnnos[2].value;
  delete testAnnos[3].value;

  // Unset all item annotations
  PlacesUtils.setAnnotationsForItem(itemId, testAnnos);
  // Check for correct removal
  testAnnos.forEach(function(anno) {
    Assert.ok(!as.itemHasAnnotation(itemId, anno.name));
    // sanity: page annotations should not be removed here
    Assert.ok(as.pageHasAnnotation(testURI, anno.name));
  });

  // Unset all page annotations
  PlacesUtils.setAnnotationsForURI(testURI, testAnnos);
  // Check for correct removal
  testAnnos.forEach(function(anno) {
    Assert.ok(!as.pageHasAnnotation(testURI, anno.name));
  });
});
