/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const histsvc = PlacesUtils.history;

add_task(async function test_addBookmarksAndCheckGuids() {
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      title: "test folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      children: [{
        url: "http://test1.com/",
        title: "1 title",
      }, {
        url: "http://test2.com/",
        title: "2 title",
      }, {
        url: "http://test3.com/",
        title: "3 title",
      }, {
        type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      }, {
        title: "test folder 2",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      }]
    }]
  });

  let root = PlacesUtils.getFolderContents(bookmarks[0].guid).root;
  Assert.equal(root.childCount, 5);

  // check bookmark guids
  let bookmarkGuidZero = root.getChild(0).bookmarkGuid;
  Assert.equal(bookmarkGuidZero.length, 12);
  // bookmarks have bookmark guids
  Assert.equal(root.getChild(1).bookmarkGuid.length, 12);
  Assert.equal(root.getChild(2).bookmarkGuid.length, 12);
  // separator has bookmark guid
  Assert.equal(root.getChild(3).bookmarkGuid.length, 12);
  // folder has bookmark guid
  Assert.equal(root.getChild(4).bookmarkGuid.length, 12);
  // all bookmark guids are different.
  Assert.notEqual(bookmarkGuidZero, root.getChild(1).bookmarkGuid);
  Assert.notEqual(root.getChild(1).bookmarkGuid, root.getChild(2).bookmarkGuid);
  Assert.notEqual(root.getChild(2).bookmarkGuid, root.getChild(3).bookmarkGuid);
  Assert.notEqual(root.getChild(3).bookmarkGuid, root.getChild(4).bookmarkGuid);

  // check page guids
  let pageGuidZero = root.getChild(0).pageGuid;
  Assert.equal(pageGuidZero.length, 12);
  // bookmarks have page guids
  Assert.equal(root.getChild(1).pageGuid.length, 12);
  Assert.equal(root.getChild(2).pageGuid.length, 12);
  // folder and separator don't have page guids
  Assert.equal(root.getChild(3).pageGuid, "");
  Assert.equal(root.getChild(4).pageGuid, "");

  Assert.notEqual(pageGuidZero, root.getChild(1).pageGuid);
  Assert.notEqual(root.getChild(1).pageGuid, root.getChild(2).pageGuid);

  root.containerOpen = false;

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_updateBookmarksAndCheckGuids() {
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      title: "test folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      children: [{
        url: "http://test1.com/",
        title: "1 title",
      }, {
        title: "test folder 2",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      }]
    }]
  });

  let root = PlacesUtils.getFolderContents(bookmarks[0].guid).root;
  Assert.equal(root.childCount, 2);

  // ensure the bookmark and page guids remain the same after modifing other property.
  let bookmarkGuidZero = root.getChild(0).bookmarkGuid;
  let pageGuidZero = root.getChild(0).pageGuid;
  await PlacesUtils.bookmarks.update({
    guid: bookmarks[1].guid,
    title: "1 title mod",
  });
  Assert.equal(root.getChild(0).title, "1 title mod");
  Assert.equal(root.getChild(0).bookmarkGuid, bookmarkGuidZero);
  Assert.equal(root.getChild(0).pageGuid, pageGuidZero);

  let bookmarkGuidOne = root.getChild(1).bookmarkGuid;
  let pageGuidOne = root.getChild(1).pageGuid;

  await PlacesUtils.bookmarks.update({
    guid: bookmarks[2].guid,
    title: "test foolder 234",
  });
  Assert.equal(root.getChild(1).title, "test foolder 234");
  Assert.equal(root.getChild(1).bookmarkGuid, bookmarkGuidOne);
  Assert.equal(root.getChild(1).pageGuid, pageGuidOne);

  root.containerOpen = false;

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_addVisitAndCheckGuid() {
  // add a visit and test page guid and non-existing bookmark guids.
  let sourceURI = uri("http://test4.com/");
  await PlacesTestUtils.addVisits({ uri: sourceURI });
  Assert.equal(await PlacesUtils.bookmarks.fetch({ url: sourceURI }, null));

  let options = histsvc.getNewQueryOptions();
  let query = histsvc.getNewQuery();
  query.uri = sourceURI;
  let root = histsvc.executeQuery(query, options).root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 1);

  do_check_valid_places_guid(root.getChild(0).pageGuid);
  Assert.equal(root.getChild(0).bookmarkGuid, "");
  root.containerOpen = false;

  await PlacesUtils.history.clear();
});

add_task(async function test_addItemsWithInvalidGUIDsFails() {
  const INVALID_GUID = "XYZ";
  try {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      guid: INVALID_GUID,
      title: "XYZ folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
    });
    do_throw("Adding a folder with an invalid guid should fail");
  } catch (ex) { }

  let folder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "test folder",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
  try {
    PlacesUtils.bookmarks.insert({
      parentGuid: folder.guid,
      guid: INVALID_GUID,
      title: "title",
      url: "http://test.tld",
    });
    do_throw("Adding a bookmark with an invalid guid should fail");
  } catch (ex) { }

  try {
    PlacesUtils.bookmarks.insert({
      parentGuid: folder.guid,
      guid: INVALID_GUID,
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR
    });
    do_throw("Adding a separator with an invalid guid should fail");
  } catch (ex) { }

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_addItemsWithGUIDs() {
  const FOLDER_GUID     = "FOLDER--GUID";
  const BOOKMARK_GUID   = "BM------GUID";
  const SEPARATOR_GUID  = "SEP-----GUID";

  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      title: "test folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      guid: FOLDER_GUID,
      children: [{
        url: "http://test1.com",
        title: "1 title",
        guid: BOOKMARK_GUID,
      }, {
        type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
        guid: SEPARATOR_GUID,
      }]
    }]
  });

  let root = PlacesUtils.getFolderContents(bookmarks[0].guid).root;
  Assert.equal(root.childCount, 2);
  Assert.equal(root.bookmarkGuid, FOLDER_GUID);
  Assert.equal(root.getChild(0).bookmarkGuid, BOOKMARK_GUID);
  Assert.equal(root.getChild(1).bookmarkGuid, SEPARATOR_GUID);

  root.containerOpen = false;
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_emptyGUIDFails() {
  try {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      guid: "",
      title: "test folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER
    });
    do_throw("Adding a folder with an empty guid should fail");
  } catch (ex) {
  }
});

add_task(async function test_usingSameGUIDFails() {
  const GUID = "XYZXYZXYZXYZ";
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    guid: GUID,
    title: "test folder",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
  try {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      guid: GUID,
      title: "test folder 2",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
    });
    do_throw("Using the same guid twice should fail");
  } catch (ex) { }

  await PlacesUtils.bookmarks.eraseEverything();
});
