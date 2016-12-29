/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
} catch (ex) {
  do_throw("Could not get nav-bookmarks-service\n");
}

// Get annotation service
try {
  var annosvc = Cc["@mozilla.org/browser/annotation-service;1"].getService(Ci.nsIAnnotationService);
} catch (ex) {
  do_throw("Could not get annotation service\n");
}

var annoObserver = {
  PAGE_lastSet_URI: "",
  PAGE_lastSet_AnnoName: "",

  onPageAnnotationSet(aURI, aName) {
    this.PAGE_lastSet_URI = aURI.spec;
    this.PAGE_lastSet_AnnoName = aName;
  },

  ITEM_lastSet_Id: -1,
  ITEM_lastSet_AnnoName: "",
  onItemAnnotationSet(aItemId, aName) {
    this.ITEM_lastSet_Id = aItemId;
    this.ITEM_lastSet_AnnoName = aName;
  },

  PAGE_lastRemoved_URI: "",
  PAGE_lastRemoved_AnnoName: "",
  onPageAnnotationRemoved(aURI, aName) {
    this.PAGE_lastRemoved_URI = aURI.spec;
    this.PAGE_lastRemoved_AnnoName = aName;
  },

  ITEM_lastRemoved_Id: -1,
  ITEM_lastRemoved_AnnoName: "",
  onItemAnnotationRemoved(aItemId, aName) {
    this.ITEM_lastRemoved_Id = aItemId;
    this.ITEM_lastRemoved_AnnoName = aName;
  }
};

// main
function run_test()
{
  run_next_test();
}

add_task(function* test_execute()
{
  var testURI = uri("http://mozilla.com/");
  var testItemId = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder, testURI, -1, "");
  var testAnnoName = "moz-test-places/annotations";
  var testAnnoVal = "test";

  annosvc.addObserver(annoObserver);
  // create new string annotation
  try {
    annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, 0);
  } catch (ex) {
    do_throw("unable to add page-annotation");
  }
  do_check_eq(annoObserver.PAGE_lastSet_URI, testURI.spec);
  do_check_eq(annoObserver.PAGE_lastSet_AnnoName, testAnnoName);

  // get string annotation
  do_check_true(annosvc.pageHasAnnotation(testURI, testAnnoName));
  var storedAnnoVal = annosvc.getPageAnnotation(testURI, testAnnoName);
  do_check_true(testAnnoVal === storedAnnoVal);
  // string item-annotation
  try {
    var lastModified = bmsvc.getItemLastModified(testItemId);
    // Verify that lastModified equals dateAdded before we set the annotation.
    do_check_eq(lastModified, bmsvc.getItemDateAdded(testItemId));
    // Workaround possible VM timers issues moving last modified to the past.
    bmsvc.setItemLastModified(testItemId, --lastModified);
    annosvc.setItemAnnotation(testItemId, testAnnoName, testAnnoVal, 0, 0);
    var lastModified2 = bmsvc.getItemLastModified(testItemId);
    // verify that setting the annotation updates the last modified time
    do_check_true(lastModified2 > lastModified);
  } catch (ex) {
    do_throw("unable to add item annotation");
  }
  do_check_eq(annoObserver.ITEM_lastSet_Id, testItemId);
  do_check_eq(annoObserver.ITEM_lastSet_AnnoName, testAnnoName);

  try {
    var annoVal = annosvc.getItemAnnotation(testItemId, testAnnoName);
    // verify the anno value
    do_check_true(testAnnoVal === annoVal);
  } catch (ex) {
    do_throw("unable to get item annotation");
  }

  // test getPagesWithAnnotation
  var uri2 = uri("http://www.tests.tld");
  yield PlacesTestUtils.addVisits(uri2);
  annosvc.setPageAnnotation(uri2, testAnnoName, testAnnoVal, 0, 0);
  var pages = annosvc.getPagesWithAnnotation(testAnnoName);
  do_check_eq(pages.length, 2);
  // Don't rely on the order
  do_check_false(pages[0].equals(pages[1]));
  do_check_true(pages[0].equals(testURI) || pages[1].equals(testURI));
  do_check_true(pages[0].equals(uri2) || pages[1].equals(uri2));

  // test getItemsWithAnnotation
  var testItemId2 = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder, uri2, -1, "");
  annosvc.setItemAnnotation(testItemId2, testAnnoName, testAnnoVal, 0, 0);
  var items = annosvc.getItemsWithAnnotation(testAnnoName);
  do_check_eq(items.length, 2);
  // Don't rely on the order
  do_check_true(items[0] != items[1]);
  do_check_true(items[0] == testItemId || items[1] == testItemId);
  do_check_true(items[0] == testItemId2 || items[1] == testItemId2);

  // get annotation that doesn't exist
  try {
    annosvc.getPageAnnotation(testURI, "blah");
    do_throw("fetching page-annotation that doesn't exist, should've thrown");
  } catch (ex) {}
  try {
    annosvc.getItemAnnotation(testURI, "blah");
    do_throw("fetching item-annotation that doesn't exist, should've thrown");
  } catch (ex) {}

  // get annotation info
  var flags = {}, exp = {}, storageType = {};
  annosvc.getPageAnnotationInfo(testURI, testAnnoName, flags, exp, storageType);
  do_check_eq(flags.value, 0);
  do_check_eq(exp.value, 0);
  do_check_eq(storageType.value, Ci.nsIAnnotationService.TYPE_STRING);
  annosvc.getItemAnnotationInfo(testItemId, testAnnoName, flags, exp, storageType);
  do_check_eq(flags.value, 0);
  do_check_eq(exp.value, 0);
  do_check_eq(storageType.value, Ci.nsIAnnotationService.TYPE_STRING);

  // get annotation names for a uri
  var annoNames = annosvc.getPageAnnotationNames(testURI);
  do_check_eq(annoNames.length, 1);
  do_check_eq(annoNames[0], "moz-test-places/annotations");

  // get annotation names for an item
  annoNames = annosvc.getItemAnnotationNames(testItemId);
  do_check_eq(annoNames.length, 1);
  do_check_eq(annoNames[0], "moz-test-places/annotations");

  // copy annotations to another uri
  var newURI = uri("http://mozilla.org");
  yield PlacesTestUtils.addVisits(newURI);
  annosvc.setPageAnnotation(testURI, "oldAnno", "new", 0, 0);
  annosvc.setPageAnnotation(newURI, "oldAnno", "old", 0, 0);
  annoNames = annosvc.getPageAnnotationNames(newURI);
  do_check_eq(annoNames.length, 1);
  do_check_eq(annoNames[0], "oldAnno");
  var oldAnnoNames = annosvc.getPageAnnotationNames(testURI);
  do_check_eq(oldAnnoNames.length, 2);
  var copiedAnno = oldAnnoNames[0];
  annosvc.copyPageAnnotations(testURI, newURI, false);
  var newAnnoNames = annosvc.getPageAnnotationNames(newURI);
  do_check_eq(newAnnoNames.length, 2);
  do_check_true(annosvc.pageHasAnnotation(newURI, "oldAnno"));
  do_check_true(annosvc.pageHasAnnotation(newURI, copiedAnno));
  do_check_eq(annosvc.getPageAnnotation(newURI, "oldAnno"), "old");
  annosvc.setPageAnnotation(newURI, "oldAnno", "new", 0, 0);
  annosvc.copyPageAnnotations(testURI, newURI, true);
  newAnnoNames = annosvc.getPageAnnotationNames(newURI);
  do_check_eq(newAnnoNames.length, 2);
  do_check_true(annosvc.pageHasAnnotation(newURI, "oldAnno"));
  do_check_true(annosvc.pageHasAnnotation(newURI, copiedAnno));
  do_check_eq(annosvc.getPageAnnotation(newURI, "oldAnno"), "new");


  // copy annotations to another item
  newURI = uri("http://mozilla.org");
  var newItemId = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder, newURI, -1, "");
  var itemId = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder, testURI, -1, "");
  annosvc.setItemAnnotation(itemId, "oldAnno", "new", 0, 0);
  annosvc.setItemAnnotation(itemId, "testAnno", "test", 0, 0);
  annosvc.setItemAnnotation(newItemId, "oldAnno", "old", 0, 0);
  annoNames = annosvc.getItemAnnotationNames(newItemId);
  do_check_eq(annoNames.length, 1);
  do_check_eq(annoNames[0], "oldAnno");
  oldAnnoNames = annosvc.getItemAnnotationNames(itemId);
  do_check_eq(oldAnnoNames.length, 2);
  copiedAnno = oldAnnoNames[0];
  annosvc.copyItemAnnotations(itemId, newItemId, false);
  newAnnoNames = annosvc.getItemAnnotationNames(newItemId);
  do_check_eq(newAnnoNames.length, 2);
  do_check_true(annosvc.itemHasAnnotation(newItemId, "oldAnno"));
  do_check_true(annosvc.itemHasAnnotation(newItemId, copiedAnno));
  do_check_eq(annosvc.getItemAnnotation(newItemId, "oldAnno"), "old");
  annosvc.setItemAnnotation(newItemId, "oldAnno", "new", 0, 0);
  annosvc.copyItemAnnotations(itemId, newItemId, true);
  newAnnoNames = annosvc.getItemAnnotationNames(newItemId);
  do_check_eq(newAnnoNames.length, 2);
  do_check_true(annosvc.itemHasAnnotation(newItemId, "oldAnno"));
  do_check_true(annosvc.itemHasAnnotation(newItemId, copiedAnno));
  do_check_eq(annosvc.getItemAnnotation(newItemId, "oldAnno"), "new");

  // test int32 anno type
  var int32Key = testAnnoName + "/types/Int32";
  var int32Val = 23;
  annosvc.setPageAnnotation(testURI, int32Key, int32Val, 0, 0);
  do_check_true(annosvc.pageHasAnnotation(testURI, int32Key));
  flags = {}, exp = {}, storageType = {};
  annosvc.getPageAnnotationInfo(testURI, int32Key, flags, exp, storageType);
  do_check_eq(flags.value, 0);
  do_check_eq(exp.value, 0);
  do_check_eq(storageType.value, Ci.nsIAnnotationService.TYPE_INT32);
  var storedVal = annosvc.getPageAnnotation(testURI, int32Key);
  do_check_true(int32Val === storedVal);
  annosvc.setItemAnnotation(testItemId, int32Key, int32Val, 0, 0);
  do_check_true(annosvc.itemHasAnnotation(testItemId, int32Key));
  annosvc.getItemAnnotationInfo(testItemId, int32Key, flags, exp, storageType);
  do_check_eq(flags.value, 0);
  do_check_eq(exp.value, 0);
  storedVal = annosvc.getItemAnnotation(testItemId, int32Key);
  do_check_true(int32Val === storedVal);

  // test int64 anno type
  var int64Key = testAnnoName + "/types/Int64";
  var int64Val = 4294967296;
  annosvc.setPageAnnotation(testURI, int64Key, int64Val, 0, 0);
  annosvc.getPageAnnotationInfo(testURI, int64Key, flags, exp, storageType);
  do_check_eq(flags.value, 0);
  do_check_eq(exp.value, 0);
  storedVal = annosvc.getPageAnnotation(testURI, int64Key);
  do_check_true(int64Val === storedVal);
  annosvc.setItemAnnotation(testItemId, int64Key, int64Val, 0, 0);
  do_check_true(annosvc.itemHasAnnotation(testItemId, int64Key));
  annosvc.getItemAnnotationInfo(testItemId, int64Key, flags, exp, storageType);
  do_check_eq(flags.value, 0);
  do_check_eq(exp.value, 0);
  storedVal = annosvc.getItemAnnotation(testItemId, int64Key);
  do_check_true(int64Val === storedVal);

  // test double anno type
  var doubleKey = testAnnoName + "/types/Double";
  var doubleVal = 0.000002342;
  annosvc.setPageAnnotation(testURI, doubleKey, doubleVal, 0, 0);
  annosvc.getPageAnnotationInfo(testURI, doubleKey, flags, exp, storageType);
  do_check_eq(flags.value, 0);
  do_check_eq(exp.value, 0);
  storedVal = annosvc.getPageAnnotation(testURI, doubleKey);
  do_check_true(doubleVal === storedVal);
  annosvc.setItemAnnotation(testItemId, doubleKey, doubleVal, 0, 0);
  do_check_true(annosvc.itemHasAnnotation(testItemId, doubleKey));
  annosvc.getItemAnnotationInfo(testItemId, doubleKey, flags, exp, storageType);
  do_check_eq(flags.value, 0);
  do_check_eq(exp.value, 0);
  do_check_eq(storageType.value, Ci.nsIAnnotationService.TYPE_DOUBLE);
  storedVal = annosvc.getItemAnnotation(testItemId, doubleKey);
  do_check_true(doubleVal === storedVal);

  // test annotation removal
  annosvc.removePageAnnotation(testURI, int32Key);

  annosvc.setItemAnnotation(testItemId, testAnnoName, testAnnoVal, 0, 0);
  // verify that removing an annotation updates the last modified date
  var lastModified3 = bmsvc.getItemLastModified(testItemId);
  // Workaround possible VM timers issues moving last modified to the past.
  bmsvc.setItemLastModified(testItemId, --lastModified3);
  annosvc.removeItemAnnotation(testItemId, int32Key);
  var lastModified4 = bmsvc.getItemLastModified(testItemId);
  do_print("verify that removing an annotation updates the last modified date");
  do_print("lastModified3 = " + lastModified3);
  do_print("lastModified4 = " + lastModified4);
  do_check_true(lastModified4 > lastModified3);

  do_check_eq(annoObserver.PAGE_lastRemoved_URI, testURI.spec);
  do_check_eq(annoObserver.PAGE_lastRemoved_AnnoName, int32Key);
  do_check_eq(annoObserver.ITEM_lastRemoved_Id, testItemId);
  do_check_eq(annoObserver.ITEM_lastRemoved_AnnoName, int32Key);

  // test that getItems/PagesWithAnnotation returns an empty array after
  // removing all items/pages which had the annotation set, see bug 380317.
  do_check_eq(annosvc.getItemsWithAnnotation(int32Key).length, 0);
  do_check_eq(annosvc.getPagesWithAnnotation(int32Key).length, 0);

  // Setting item annotations on invalid item ids should throw
  var invalidIds = [-1, 0, 37643];
  for (var id of invalidIds) {
    try {
      annosvc.setItemAnnotation(id, "foo", "bar", 0, 0);
      do_throw("setItemAnnotation* should throw for invalid item id: " + id)
    }
    catch (ex) { }
  }

  // setting an annotation with EXPIRE_HISTORY for an item should throw
  itemId = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder, testURI, -1, "");
  try {
    annosvc.setItemAnnotation(itemId, "foo", "bar", 0, annosvc.EXPIRE_WITH_HISTORY);
    do_throw("setting an item annotation with EXPIRE_HISTORY should throw");
  }
  catch (ex) {
  }

  annosvc.removeObserver(annoObserver);
});

add_test(function test_getAnnotationsHavingName() {
  let uri = NetUtil.newURI("http://cat.mozilla.org");
  let id = PlacesUtils.bookmarks.insertBookmark(
    PlacesUtils.unfiledBookmarksFolderId, uri,
    PlacesUtils.bookmarks.DEFAULT_INDEX, "cat");
  let fid = PlacesUtils.bookmarks.createFolder(
    PlacesUtils.unfiledBookmarksFolderId, "pillow",
    PlacesUtils.bookmarks.DEFAULT_INDEX);

  const ANNOS = {
    "int": 7,
    "double": 7.7,
    "string": "seven"
  };
  for (let name in ANNOS) {
    PlacesUtils.annotations.setPageAnnotation(
      uri, name, ANNOS[name], 0,
      PlacesUtils.annotations.EXPIRE_SESSION);
    PlacesUtils.annotations.setItemAnnotation(
      id, name, ANNOS[name], 0,
      PlacesUtils.annotations.EXPIRE_SESSION);
    PlacesUtils.annotations.setItemAnnotation(
      fid, name, ANNOS[name], 0,
      PlacesUtils.annotations.EXPIRE_SESSION);
  }

  for (let name in ANNOS) {
    let results = PlacesUtils.annotations.getAnnotationsWithName(name);
    do_check_eq(results.length, 3);

    for (let result of results) {
      do_check_eq(result.annotationName, name);
      do_check_eq(result.annotationValue, ANNOS[name]);
      if (result.uri)
        do_check_true(result.uri.equals(uri));
      else
        do_check_true(result.itemId > 0);

      if (result.itemId != -1) {
        if (result.uri)
          do_check_eq(result.itemId, id);
        else
          do_check_eq(result.itemId, fid);
        do_check_guid_for_bookmark(result.itemId, result.guid);
      }
      else {
        do_check_guid_for_uri(result.uri, result.guid);
      }
    }
  }

  run_next_test();
});
