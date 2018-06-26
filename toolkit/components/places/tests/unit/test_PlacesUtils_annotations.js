/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ANNOTATIONS = [{
  name: PlacesUtils.LMANNO_FEEDURI,
  value: "test",
  flags: 0,
  expires: Ci.nsIAnnotationService.EXPIRE_MONTHS,
}, {
  name: PlacesUtils.LMANNO_SITEURI,
  value: "test2",
  flags: 0,
  expires: Ci.nsIAnnotationService.EXPIRE_DAYS,
}];

function checkAnnotations(annotations, expectedAnnotations) {
  Assert.equal(annotations.length, expectedAnnotations.length,
    "Should have the expected number of annotations");

  for (let i = 0; i < annotations.length; i++) {
    Assert.deepEqual(annotations[i], TEST_ANNOTATIONS[i],
      "Should have the correct annotation data");
  }
}

add_task(async function test_getAnnotationsForItem() {
  let bms = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{
      title: "no annotations",
      url: "http://example.com",
    }, {
      title: "one annotations",
      url: "http://example.com/1",
      annos: [TEST_ANNOTATIONS[0]]
    }, {
      title: "two annotations",
      url: "http://example.com/2",
      annos: TEST_ANNOTATIONS
    }],
  });

  let ids = await PlacesUtils.promiseManyItemIds(bms.map(bm => bm.guid));

  for (let i = 0; i < bms.length; i++) {
    let annotations = await PlacesUtils.promiseAnnotationsForItem(ids.get(bms[i].guid));
    checkAnnotations(annotations, TEST_ANNOTATIONS.slice(0, i));
  }
});
