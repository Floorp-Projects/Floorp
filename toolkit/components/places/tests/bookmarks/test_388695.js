/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get bookmark service
let bm = PlacesUtils.bookmarks;

// Test that Bookmarks fetch properly orders its results based on
// the last modified value. Note we cannot rely on dateAdded due to
// the low PR_Now() resolution.

add_task(async function sort_bookmark_by_relevance() {
    let now = new Date();
    let modifiedTime = new Date(now.setHours(now.getHours() - 2));

    let url = "http://foo.tld.com/";
    let parentGuid = (await bm.insert({type: bm.TYPE_FOLDER,
                                       title: "test folder",
                                       parentGuid: bm.unfiledGuid})).guid;
    let item1Guid = (await bm.insert({url,
                                      parentGuid})).guid;
    let item2Guid = (await bm.insert({url,
                                      parentGuid,
                                      dateAdded: modifiedTime,
                                      lastModified: modifiedTime})).guid;
    let bms = [];
    await bm.fetch({url}, bm1 => bms.push(bm1));
    Assert.equal(bms[0].guid, item1Guid);
    Assert.equal(bms[1].guid, item2Guid);
    await bm.update({guid: item2Guid, title: "modified"});

    let bms1 = [];
    await bm.fetch({url}, bm2 => bms1.push(bm2));
    Assert.equal(bms1[0].guid, item2Guid);
    Assert.equal(bms1[1].guid, item1Guid);
});
