/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /**
  * Check for correct functionality of PlacesUtils.setAnnotationsForItem/URI
  */

var hs = PlacesUtils.history;
var bs = PlacesUtils.bookmarks;
var as = PlacesUtils.annotations;

const TEST_URL = "http://test.mozilla.org/";

function run_test() {
  var testURI = uri(TEST_URL);
  // add a bookmark
  var itemId = bs.insertBookmark(bs.unfiledBookmarksFolder, testURI,
                                 bs.DEFAULT_INDEX, "test");

  // create annotations array
  var testAnnos = [{ name: "testAnno/test0",
                     type: Ci.nsIAnnotationService.TYPE_STRING,
                     flags: 0,
                     value: "test0",
                     expires: Ci.nsIAnnotationService.EXPIRE_NEVER },
                   { name: "testAnno/test1",
                     type: Ci.nsIAnnotationService.TYPE_STRING,
                     flags: 0,
                     value: "test1",
                     expires: Ci.nsIAnnotationService.EXPIRE_NEVER },
                   { name: "testAnno/test2",
                     type: Ci.nsIAnnotationService.TYPE_STRING,
                     flags: 0,
                     value: "test2",
                     expires: Ci.nsIAnnotationService.EXPIRE_NEVER }];

  // Add item annotations
  PlacesUtils.setAnnotationsForItem(itemId, testAnnos);
  // Check for correct addition
  testAnnos.forEach(function(anno) {
    do_check_true(as.itemHasAnnotation(itemId, anno.name));
    do_check_eq(as.getItemAnnotation(itemId, anno.name), anno.value);
  });

  // Add page annotations
  PlacesUtils.setAnnotationsForURI(testURI, testAnnos);
  // Check for correct addition
  testAnnos.forEach(function(anno) {
    do_check_true(as.pageHasAnnotation(testURI, anno.name));
    do_check_eq(as.getPageAnnotation(testURI, anno.name), anno.value);
  });

  // To unset annotations we set their values to null
  testAnnos.forEach(function(anno) { anno.value = null; });

  // Unset all item annotations
  PlacesUtils.setAnnotationsForItem(itemId, testAnnos);
  // Check for correct removal
  testAnnos.forEach(function(anno) {
    do_check_false(as.itemHasAnnotation(itemId, anno.name));
    // sanity: page annotations should not be removed here
    do_check_true(as.pageHasAnnotation(testURI, anno.name));
  });

  // Unset all page annotations
  PlacesUtils.setAnnotationsForURI(testURI, testAnnos);
  // Check for correct removal
  testAnnos.forEach(function(anno) {
    do_check_false(as.pageHasAnnotation(testURI, anno.name));
  });
}
