/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const NUM_BOOKMARKS = 20;
const NUM_SEPARATORS = 5;
const NUM_FOLDERS = 10;
const NUM_ITEMS = NUM_BOOKMARKS + NUM_SEPARATORS + NUM_FOLDERS;
const MIN_RAND = -5;
const MAX_RAND = 40;

async function check_contiguous_indexes(bookmarks) {
  var indexes = [];
  for (let bm of bookmarks) {
    let bmIndex = (await PlacesUtils.bookmarks.fetch(bm.guid)).index;
    do_print(`Index: ${bmIndex}\n`);
    do_print("Checking duplicates\n");
    Assert.ok(!indexes.includes(bmIndex));
    do_print(`Checking out of range, found ${bookmarks.length} items\n`);
    Assert.ok(bmIndex >= 0 && bmIndex < bookmarks.length);
    indexes.push(bmIndex);
  }
  do_print("Checking all valid indexes have been used\n");
  Assert.equal(indexes.length, bookmarks.length);
}

add_task(async function test_bookmarks_indexing() {
  let bookmarks = [];
  // Insert bookmarks with random indexes.
  for (let i = 0; bookmarks.length < NUM_BOOKMARKS; i++) {
    let randIndex = Math.round(MIN_RAND + (Math.random() * (MAX_RAND - MIN_RAND)));
    try {
      let bm = await PlacesUtils.bookmarks.insert({
        index: randIndex,
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        title: `Test bookmark ${i}`,
        url: `http://${i}.mozilla.org/`,
      });
      if (randIndex < -1)
        do_throw("Creating a bookmark at an invalid index should throw");
      bookmarks.push(bm);
    } catch (ex) {
      if (randIndex >= -1)
        do_throw("Creating a bookmark at a valid index should not throw");
    }
  }
  await check_contiguous_indexes(bookmarks);

  // Insert separators with random indexes.
  for (let i = 0; bookmarks.length < NUM_BOOKMARKS + NUM_SEPARATORS; i++) {
    let randIndex = Math.round(MIN_RAND + (Math.random() * (MAX_RAND - MIN_RAND)));
    try {
      let bm = await PlacesUtils.bookmarks.insert({
        index: randIndex,
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      });
      if (randIndex < -1)
        do_throw("Creating a separator at an invalid index should throw");
      bookmarks.push(bm);
    } catch (ex) {
      if (randIndex >= -1)
        do_throw("Creating a separator at a valid index should not throw");
    }
  }
  await check_contiguous_indexes(bookmarks);

  // Insert folders with random indexes.
  for (let i = 0; bookmarks.length < NUM_ITEMS; i++) {
    let randIndex = Math.round(MIN_RAND + (Math.random() * (MAX_RAND - MIN_RAND)));
    try {
      let bm = await PlacesUtils.bookmarks.insert({
        index: randIndex,
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        title: `Test folder ${i}`,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      });
      if (randIndex < -1)
        do_throw("Creating a folder at an invalid index should throw");
      bookmarks.push(bm);
    } catch (ex) {
      if (randIndex >= -1)
        do_throw("Creating a folder at a valid index should not throw");
    }
  }
  await check_contiguous_indexes(bookmarks);

  // Execute some random bookmark delete.
  for (let i = 0; i < Math.ceil(NUM_ITEMS / 4); i++) {
    let bm = bookmarks.splice(Math.floor(Math.random() * bookmarks.length), 1)[0];
    do_print(`Removing item with guid ${bm.guid}\n`);
    await PlacesUtils.bookmarks.remove(bm);
  }
  await check_contiguous_indexes(bookmarks);

  // Execute some random bookmark move.  This will also try to move it to
  // invalid index values.
  for (let i = 0; i < Math.ceil(NUM_ITEMS / 4); i++) {
    let randIndex = Math.floor(Math.random() * bookmarks.length);
    let bm = bookmarks[randIndex];
    let newIndex = Math.round(MIN_RAND + (Math.random() * (MAX_RAND - MIN_RAND)));
    do_print(`Moving item with guid ${bm.guid} to index ${newIndex}\n`);
    try {
      bm.index = newIndex;
      await PlacesUtils.bookmarks.update(bm);
      if (newIndex < -1)
        do_throw("Moving an item to a negative index should throw\n");
    } catch (ex) {
      if (newIndex >= -1)
        do_throw("Moving an item to a valid index should not throw\n");
    }

  }
  await check_contiguous_indexes(bookmarks);
});
