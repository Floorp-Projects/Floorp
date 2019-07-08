/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * What this is aimed to test:
 *
 * History.clear() should expire everything but bookmarked pages and valid
 * annos.
 */

add_task(async function test_historyClear() {
  let as = PlacesUtils.annotations;
  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h

  // Expire all expirable pages.
  setMaxPages(0);

  // Add some bookmarked page with visit and annotations.
  for (let i = 0; i < 5; i++) {
    let pageURI = uri("http://item_anno." + i + ".mozilla.org/");
    // This visit will be expired.
    await PlacesTestUtils.addVisits({ uri: pageURI });
    let bm = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: pageURI,
      title: null,
    });
    let id = await PlacesUtils.promiseItemId(bm.guid);
    // Will persist because it's an EXPIRE_NEVER item anno.
    as.setItemAnnotation(id, "persist", "test", 0, as.EXPIRE_NEVER);
    // Will persist because the page is bookmarked.
    await PlacesUtils.history.update({
      url: pageURI,
      annotations: new Map([["persist", "test"]]),
    });
  }

  // Add some visited page and annotations for each.
  for (let i = 0; i < 5; i++) {
    // All page annotations related to these expired pages are expected to
    // expire as well.
    let pageURI = uri("http://page_anno." + i + ".mozilla.org/");
    await PlacesTestUtils.addVisits({ uri: pageURI });
    await PlacesUtils.history.update({
      url: pageURI,
      annotations: new Map([["expire", "test"]]),
    });
  }

  // Expire all visits for the bookmarks
  await PlacesUtils.history.clear();

  Assert.equal((await getPagesWithAnnotation("expire")).length, 0);

  let pages = await getPagesWithAnnotation("persist");
  Assert.equal(pages.length, 5);

  let items = await getItemsWithAnnotation("persist");
  Assert.equal(items.length, 5);

  for (let guid of items) {
    // Check item exists.
    Assert.ok(await PlacesUtils.bookmarks.fetch({ guid }), "item exists");
  }
});
