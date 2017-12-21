/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

add_task(async function test_execute() {
  let testURI = uri("http://mozilla.com/");
  let testItem = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "",
    url: testURI,
  });
  let testItemId = await PlacesUtils.promiseItemId(testItem.guid);
  let testAnnoName = "moz-test-places/annotations";
  let testAnnoVal = "test";
  let earlierDate = new Date(Date.now() - 1000);

  annosvc.addObserver(annoObserver);
  // create new string annotation
  try {
    annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, 0);
  } catch (ex) {
    do_throw("unable to add page-annotation");
  }
  Assert.equal(annoObserver.PAGE_lastSet_URI, testURI.spec);
  Assert.equal(annoObserver.PAGE_lastSet_AnnoName, testAnnoName);

  // get string annotation
  Assert.ok(annosvc.pageHasAnnotation(testURI, testAnnoName));
  var storedAnnoVal = annosvc.getPageAnnotation(testURI, testAnnoName);
  Assert.ok(testAnnoVal === storedAnnoVal);
  // string item-annotation
  let item = await PlacesUtils.bookmarks.fetch(testItem.guid);

  // Verify that lastModified equals dateAdded before we set the annotation.
  Assert.equal(item.lastModified.getTime(), item.dateAdded.getTime());
  // Workaround possible VM timers issues moving last modified to the past.
  await PlacesUtils.bookmarks.update({
    guid: item.guid,
    dateAdded: earlierDate,
    lastModified: earlierDate,
  });

  try {
    annosvc.setItemAnnotation(testItemId, testAnnoName, testAnnoVal, 0, 0);
  } catch (ex) {
    do_throw("unable to add item annotation " + ex);
  }

  let updatedItem = await PlacesUtils.bookmarks.fetch(testItem.guid);

  // verify that setting the annotation updates the last modified time
  Assert.ok(updatedItem.lastModified > item.lastModified);
  Assert.equal(annoObserver.ITEM_lastSet_Id, testItemId);
  Assert.equal(annoObserver.ITEM_lastSet_AnnoName, testAnnoName);

  try {
    var annoVal = annosvc.getItemAnnotation(testItemId, testAnnoName);
    // verify the anno value
    Assert.ok(testAnnoVal === annoVal);
  } catch (ex) {
    do_throw("unable to get item annotation");
  }

  // test getPagesWithAnnotation
  var uri2 = uri("http://www.tests.tld");
  await PlacesTestUtils.addVisits(uri2);
  annosvc.setPageAnnotation(uri2, testAnnoName, testAnnoVal, 0, 0);
  var pages = annosvc.getPagesWithAnnotation(testAnnoName);
  Assert.equal(pages.length, 2);
  // Don't rely on the order
  Assert.ok(!pages[0].equals(pages[1]));
  Assert.ok(pages[0].equals(testURI) || pages[1].equals(testURI));
  Assert.ok(pages[0].equals(uri2) || pages[1].equals(uri2));

  // test getItemsWithAnnotation
  let testItem2 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "",
    url: uri2,
  });
  let testItemId2 = await PlacesUtils.promiseItemId(testItem2.guid);
  annosvc.setItemAnnotation(testItemId2, testAnnoName, testAnnoVal, 0, 0);
  var items = annosvc.getItemsWithAnnotation(testAnnoName);
  Assert.equal(items.length, 2);
  // Don't rely on the order
  Assert.ok(items[0] != items[1]);
  Assert.ok(items[0] == testItemId || items[1] == testItemId);
  Assert.ok(items[0] == testItemId2 || items[1] == testItemId2);

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
  Assert.equal(flags.value, 0);
  Assert.equal(exp.value, 0);
  Assert.equal(storageType.value, Ci.nsIAnnotationService.TYPE_STRING);
  annosvc.getItemAnnotationInfo(testItemId, testAnnoName, flags, exp, storageType);
  Assert.equal(flags.value, 0);
  Assert.equal(exp.value, 0);
  Assert.equal(storageType.value, Ci.nsIAnnotationService.TYPE_STRING);

  // get annotation names for a uri
  var annoNames = annosvc.getPageAnnotationNames(testURI);
  Assert.equal(annoNames.length, 1);
  Assert.equal(annoNames[0], "moz-test-places/annotations");

  // get annotation names for an item
  annoNames = annosvc.getItemAnnotationNames(testItemId);
  Assert.equal(annoNames.length, 1);
  Assert.equal(annoNames[0], "moz-test-places/annotations");

  // copy annotations to another uri
  var newURI = uri("http://mozilla.org");
  await PlacesTestUtils.addVisits(newURI);
  annosvc.setPageAnnotation(testURI, "oldAnno", "new", 0, 0);
  annosvc.setPageAnnotation(newURI, "oldAnno", "old", 0, 0);
  annoNames = annosvc.getPageAnnotationNames(newURI);
  Assert.equal(annoNames.length, 1);
  Assert.equal(annoNames[0], "oldAnno");
  var oldAnnoNames = annosvc.getPageAnnotationNames(testURI);
  Assert.equal(oldAnnoNames.length, 2);
  var copiedAnno = oldAnnoNames[0];
  annosvc.copyPageAnnotations(testURI, newURI, false);
  var newAnnoNames = annosvc.getPageAnnotationNames(newURI);
  Assert.equal(newAnnoNames.length, 2);
  Assert.ok(annosvc.pageHasAnnotation(newURI, "oldAnno"));
  Assert.ok(annosvc.pageHasAnnotation(newURI, copiedAnno));
  Assert.equal(annosvc.getPageAnnotation(newURI, "oldAnno"), "old");
  annosvc.setPageAnnotation(newURI, "oldAnno", "new", 0, 0);
  annosvc.copyPageAnnotations(testURI, newURI, true);
  newAnnoNames = annosvc.getPageAnnotationNames(newURI);
  Assert.equal(newAnnoNames.length, 2);
  Assert.ok(annosvc.pageHasAnnotation(newURI, "oldAnno"));
  Assert.ok(annosvc.pageHasAnnotation(newURI, copiedAnno));
  Assert.equal(annosvc.getPageAnnotation(newURI, "oldAnno"), "new");


  // copy annotations to another item
  newURI = uri("http://mozilla.org");
  let newItem = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "",
    url: newURI,
  });
  let newItemId = await PlacesUtils.promiseItemId(newItem.guid);
  item = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "",
    url: testURI,
  });
  var itemId = await PlacesUtils.promiseItemId(item.guid);
  annosvc.setItemAnnotation(itemId, "oldAnno", "new", 0, 0);
  annosvc.setItemAnnotation(itemId, "testAnno", "test", 0, 0);
  annosvc.setItemAnnotation(newItemId, "oldAnno", "old", 0, 0);
  annoNames = annosvc.getItemAnnotationNames(newItemId);
  Assert.equal(annoNames.length, 1);
  Assert.equal(annoNames[0], "oldAnno");
  oldAnnoNames = annosvc.getItemAnnotationNames(itemId);
  Assert.equal(oldAnnoNames.length, 2);
  copiedAnno = oldAnnoNames[0];
  annosvc.copyItemAnnotations(itemId, newItemId, false);
  newAnnoNames = annosvc.getItemAnnotationNames(newItemId);
  Assert.equal(newAnnoNames.length, 2);
  Assert.ok(annosvc.itemHasAnnotation(newItemId, "oldAnno"));
  Assert.ok(annosvc.itemHasAnnotation(newItemId, copiedAnno));
  Assert.equal(annosvc.getItemAnnotation(newItemId, "oldAnno"), "old");
  annosvc.setItemAnnotation(newItemId, "oldAnno", "new", 0, 0);
  annosvc.copyItemAnnotations(itemId, newItemId, true);
  newAnnoNames = annosvc.getItemAnnotationNames(newItemId);
  Assert.equal(newAnnoNames.length, 2);
  Assert.ok(annosvc.itemHasAnnotation(newItemId, "oldAnno"));
  Assert.ok(annosvc.itemHasAnnotation(newItemId, copiedAnno));
  Assert.equal(annosvc.getItemAnnotation(newItemId, "oldAnno"), "new");

  // test int32 anno type
  var int32Key = testAnnoName + "/types/Int32";
  var int32Val = 23;
  annosvc.setPageAnnotation(testURI, int32Key, int32Val, 0, 0);
  Assert.ok(annosvc.pageHasAnnotation(testURI, int32Key));
  flags = {}, exp = {}, storageType = {};
  annosvc.getPageAnnotationInfo(testURI, int32Key, flags, exp, storageType);
  Assert.equal(flags.value, 0);
  Assert.equal(exp.value, 0);
  Assert.equal(storageType.value, Ci.nsIAnnotationService.TYPE_INT32);
  var storedVal = annosvc.getPageAnnotation(testURI, int32Key);
  Assert.ok(int32Val === storedVal);
  annosvc.setItemAnnotation(testItemId, int32Key, int32Val, 0, 0);
  Assert.ok(annosvc.itemHasAnnotation(testItemId, int32Key));
  annosvc.getItemAnnotationInfo(testItemId, int32Key, flags, exp, storageType);
  Assert.equal(flags.value, 0);
  Assert.equal(exp.value, 0);
  storedVal = annosvc.getItemAnnotation(testItemId, int32Key);
  Assert.ok(int32Val === storedVal);

  // test int64 anno type
  var int64Key = testAnnoName + "/types/Int64";
  var int64Val = 4294967296;
  annosvc.setPageAnnotation(testURI, int64Key, int64Val, 0, 0);
  annosvc.getPageAnnotationInfo(testURI, int64Key, flags, exp, storageType);
  Assert.equal(flags.value, 0);
  Assert.equal(exp.value, 0);
  storedVal = annosvc.getPageAnnotation(testURI, int64Key);
  Assert.ok(int64Val === storedVal);
  annosvc.setItemAnnotation(testItemId, int64Key, int64Val, 0, 0);
  Assert.ok(annosvc.itemHasAnnotation(testItemId, int64Key));
  annosvc.getItemAnnotationInfo(testItemId, int64Key, flags, exp, storageType);
  Assert.equal(flags.value, 0);
  Assert.equal(exp.value, 0);
  storedVal = annosvc.getItemAnnotation(testItemId, int64Key);
  Assert.ok(int64Val === storedVal);

  // test double anno type
  var doubleKey = testAnnoName + "/types/Double";
  var doubleVal = 0.000002342;
  annosvc.setPageAnnotation(testURI, doubleKey, doubleVal, 0, 0);
  annosvc.getPageAnnotationInfo(testURI, doubleKey, flags, exp, storageType);
  Assert.equal(flags.value, 0);
  Assert.equal(exp.value, 0);
  storedVal = annosvc.getPageAnnotation(testURI, doubleKey);
  Assert.ok(doubleVal === storedVal);
  annosvc.setItemAnnotation(testItemId, doubleKey, doubleVal, 0, 0);
  Assert.ok(annosvc.itemHasAnnotation(testItemId, doubleKey));
  annosvc.getItemAnnotationInfo(testItemId, doubleKey, flags, exp, storageType);
  Assert.equal(flags.value, 0);
  Assert.equal(exp.value, 0);
  Assert.equal(storageType.value, Ci.nsIAnnotationService.TYPE_DOUBLE);
  storedVal = annosvc.getItemAnnotation(testItemId, doubleKey);
  Assert.ok(doubleVal === storedVal);

  // test annotation removal
  annosvc.removePageAnnotation(testURI, int32Key);

  annosvc.setItemAnnotation(testItemId, testAnnoName, testAnnoVal, 0, 0);
  // verify that removing an annotation updates the last modified date
  testItem = await PlacesUtils.bookmarks.fetch(testItem.guid);

  var lastModified3 = testItem.lastModified;
  // Workaround possible VM timers issues moving last modified to the past.
  await PlacesUtils.bookmarks.update({
    guid: testItem.guid,
    dateAdded: earlierDate,
    lastModified: earlierDate,
  });
  annosvc.removeItemAnnotation(testItemId, int32Key);

  testItem = await PlacesUtils.bookmarks.fetch(testItem.guid);
  var lastModified4 = testItem.lastModified;
  do_print("verify that removing an annotation updates the last modified date");
  do_print("lastModified3 = " + lastModified3);
  do_print("lastModified4 = " + lastModified4);
  Assert.ok(lastModified4 > lastModified3);

  Assert.equal(annoObserver.PAGE_lastRemoved_URI, testURI.spec);
  Assert.equal(annoObserver.PAGE_lastRemoved_AnnoName, int32Key);
  Assert.equal(annoObserver.ITEM_lastRemoved_Id, testItemId);
  Assert.equal(annoObserver.ITEM_lastRemoved_AnnoName, int32Key);

  // test that getItems/PagesWithAnnotation returns an empty array after
  // removing all items/pages which had the annotation set, see bug 380317.
  Assert.equal(annosvc.getItemsWithAnnotation(int32Key).length, 0);
  Assert.equal(annosvc.getPagesWithAnnotation(int32Key).length, 0);

  // Setting item annotations on invalid item ids should throw
  var invalidIds = [-1, 0, 37643];
  for (var id of invalidIds) {
    try {
      annosvc.setItemAnnotation(id, "foo", "bar", 0, 0);
      do_throw("setItemAnnotation* should throw for invalid item id: " + id);
    } catch (ex) { }
  }

  // setting an annotation with EXPIRE_HISTORY for an item should throw
  item = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "",
    url: testURI,
  });
  itemId = await PlacesUtils.promiseItemId(item.guid);
  try {
    annosvc.setItemAnnotation(itemId, "foo", "bar", 0, annosvc.EXPIRE_WITH_HISTORY);
    do_throw("setting an item annotation with EXPIRE_HISTORY should throw");
  } catch (ex) {
  }

  annosvc.removeObserver(annoObserver);
});

add_task(async function test_getAnnotationsHavingName() {
  let url = uri("http://cat.mozilla.org");
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "cat",
    url,
  });
  let id = await PlacesUtils.promiseItemId(bookmark.guid);

  let folder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "pillow",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
  let fid = await PlacesUtils.promiseItemId(folder.guid);

  const ANNOS = {
    "int": 7,
    "double": 7.7,
    "string": "seven"
  };
  for (let name in ANNOS) {
    PlacesUtils.annotations.setPageAnnotation(
      url, name, ANNOS[name], 0,
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
    Assert.equal(results.length, 3);

    for (let result of results) {
      Assert.equal(result.annotationName, name);
      Assert.equal(result.annotationValue, ANNOS[name]);
      if (result.uri)
        Assert.ok(result.uri.equals(url));
      else
        Assert.ok(result.itemId > 0);

      if (result.itemId != -1) {
        if (result.uri)
          Assert.equal(result.itemId, id);
        else
          Assert.equal(result.itemId, fid);
        do_check_guid_for_bookmark(result.itemId, result.guid);
      } else {
        do_check_guid_for_uri(result.uri, result.guid);
      }
    }
  }
});
