/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * What this is aimed to test:
 *
 * EXPIRE_NEVER annotations should be expired when a page is removed from the
 * database.
 * If the annotation is a page annotation this will happen when the page is
 * expired, namely when the page has no visits and is not bookmarked.
 */

add_task(async function test_annos_expire_never() {
  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h

  // Expire all expirable pages.
  setMaxPages(0);

  // Add some visited page and a couple expire never annotations for each.
  let now = getExpirablePRTime();
  for (let i = 0; i < 5; i++) {
    let pageURI = uri("http://page_anno." + i + ".mozilla.org/");
    await PlacesTestUtils.addVisits({ uri: pageURI, visitDate: now++ });
    await PlacesUtils.history.update({
      url: pageURI,
      annotations: new Map([
        ["page_expire1", "test"],
        ["page_expire2", "test"],
      ]),
    });
  }

  let pages = await getPagesWithAnnotation("page_expire1");
  Assert.equal(pages.length, 5);
  pages = await getPagesWithAnnotation("page_expire2");
  Assert.equal(pages.length, 5);

  // Add other visited page and a couple expire never annotations for each.
  // We won't expire these visits, so the annotations should survive.
  for (let i = 0; i < 5; i++) {
    let pageURI = uri("http://persist_page_anno." + i + ".mozilla.org/");
    await PlacesTestUtils.addVisits({ uri: pageURI, visitDate: now++ });
    await PlacesUtils.history.update({
      url: pageURI,
      annotations: new Map([
        ["page_persist1", "test"],
        ["page_persist2", "test"],
      ]),
    });
  }

  pages = await getPagesWithAnnotation("page_persist1");
  Assert.equal(pages.length, 5);
  pages = await getPagesWithAnnotation("page_persist2");
  Assert.equal(pages.length, 5);

  // Expire all visits for the first 5 pages and the bookmarks.
  await promiseForceExpirationStep(5);

  pages = await getPagesWithAnnotation("page_expire1");
  Assert.equal(pages.length, 0);
  pages = await getPagesWithAnnotation("page_expire2");
  Assert.equal(pages.length, 0);
  pages = await getPagesWithAnnotation("page_persist1");
  Assert.equal(pages.length, 5);
  pages = await getPagesWithAnnotation("page_persist2");
  Assert.equal(pages.length, 5);
});
