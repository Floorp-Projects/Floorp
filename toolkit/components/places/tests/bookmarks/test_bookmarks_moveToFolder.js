/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BOOKMARK_DATE_ADDED = new Date();

function ensurePosition(info, parentGuid, index) {
  print(`Checking ${info.guid}`);
  checkBookmarkObject(info);
  Assert.equal(
    info.parentGuid,
    parentGuid,
    "Should be in the correct parent folder"
  );
  Assert.equal(info.index, index, "Should have the correct index");
}

function insertChildren(folder, items) {
  if (!items.length) {
    return [];
  }

  let children = [];
  for (let i = 0; i < items.length; i++) {
    if (items[i].type === TYPE_BOOKMARK) {
      children.push({
        title: `${i}`,
        url: "http://example.com",
        dateAdded: BOOKMARK_DATE_ADDED,
      });
    } else {
      throw new Error(`Type ${items[i].type} is not supported.`);
    }
  }
  return PlacesUtils.bookmarks.insertTree({
    guid: folder.guid,
    children,
  });
}

async function dumpFolderChildren(
  folder,
  details,
  folderA,
  folderB,
  originalAChildren,
  originalBChildren
) {
  info(`${folder} Details:`);
  info(`Input: ${JSON.stringify(details.initial[folder])}`);
  info(`Expected: ${JSON.stringify(details.expected[folder])}`);
  info("Index\tOriginal\tExpected\tResult");

  let originalChildren;
  let folderGuid;
  if (folder == "folderA") {
    originalChildren = originalAChildren;
    folderGuid = folderA.guid;
  } else {
    originalChildren = originalBChildren;
    folderGuid = folderB.guid;
  }

  let tree = await PlacesUtils.promiseBookmarksTree(folderGuid);
  let childrenCount = tree.children ? tree.children.length : 0;
  for (let i = 0; i < originalChildren.length || i < childrenCount; i++) {
    let originalGuid =
      i < originalChildren.length ? originalChildren[i].guid : "            ";
    let resultGuid = i < childrenCount ? tree.children[i].guid : "            ";
    let expectedGuid = "            ";
    if (i < details.expected[folder].length) {
      let expected = details.expected[folder][i];
      expectedGuid =
        expected.folder == "a"
          ? originalAChildren[expected.originalIndex].guid
          : originalBChildren[expected.originalIndex].guid;
    }
    info(`${i}\t${originalGuid}\t${expectedGuid}\t${resultGuid}\n`);
  }
}

async function checkExpectedResults(
  details,
  folder,
  folderGuid,
  lastModified,
  movedItems,
  folderAChildren,
  folderBChildren
) {
  let expectedResults = details.expected[folder];
  for (let i = 0; i < expectedResults.length; i++) {
    let expectedDetails = expectedResults[i];
    let originalItem =
      expectedDetails.folder == "a"
        ? folderAChildren[expectedDetails.originalIndex]
        : folderBChildren[expectedDetails.originalIndex];

    // Check the item got updated correctly in the database.
    let updatedItem = await PlacesUtils.bookmarks.fetch(originalItem.guid);

    ensurePosition(updatedItem, folderGuid, i);
    Assert.greaterOrEqual(
      updatedItem.lastModified.getTime(),
      lastModified.getTime(),
      "Last modified should be later or equal to before"
    );
  }

  if (details.expected.skipResultIndexChecks) {
    return;
  }

  // Check the items returned from the actual move() call are correct.
  let index = 0;
  for (let item of details.initial[folder]) {
    if (!("targetFolder" in item)) {
      // We weren't moving this item, so skip it and continue.
      continue;
    }

    let movedItem = movedItems[index];
    let updatedItem = await PlacesUtils.bookmarks.fetch(movedItem.guid);

    ensurePosition(movedItem, updatedItem.parentGuid, updatedItem.index);

    index++;
  }
}

async function checkLastModifiedForFolders(details, folder, movedItems) {
  // For the tests, the moves always come from folderA.
  if (
    details.initial.folderA.some(
      item => "targetFolder" in item && item.targetFolder == folder.title
    )
  ) {
    let updatedFolder = await PlacesUtils.bookmarks.fetch(folder.guid);

    Assert.greaterOrEqual(
      updatedFolder.lastModified.getTime(),
      folder.lastModified.getTime(),
      "Should have updated the folder's last modified time."
    );
    print(JSON.stringify(movedItems[0]));
    Assert.deepEqual(
      updatedFolder.lastModified,
      movedItems[0].lastModified,
      "Should have the same last modified as the moved items."
    );
  }
}

async function testMoveToFolder(details) {
  await PlacesUtils.bookmarks.eraseEverything();

  // Always create two folders by default.
  let [folderA, folderB] = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      {
        title: "a",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      },
      {
        title: "b",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      },
    ],
  });

  checkBookmarkObject(folderA);
  let folderAChildren = await insertChildren(folderA, details.initial.folderA);
  checkBookmarkObject(folderB);
  let folderBChildren = await insertChildren(folderB, details.initial.folderB);

  const originalAChildren = folderAChildren.map(child => {
    return { ...child };
  });
  const originalBChildren = folderBChildren.map(child => {
    return { ...child };
  });

  let lastModified;
  if (folderAChildren.length) {
    lastModified = folderAChildren[0].lastModified;
  } else if (folderBChildren.length) {
    lastModified = folderBChildren[0].lastModified;
  } else {
    throw new Error("No children added, can't determine lastModified");
  }

  // Work out which children to move and to where.
  let childrenToUpdate = [];
  for (let i = 0; i < details.initial.folderA.length; i++) {
    if ("move" in details.initial.folderA[i]) {
      childrenToUpdate.push(folderAChildren[i].guid);
    }
  }

  let observer;
  if (details.notifications) {
    observer = expectPlacesObserverNotifications(["bookmark-moved"]);
  }

  let movedItems = await PlacesUtils.bookmarks.moveToFolder(
    childrenToUpdate,
    details.targetFolder == "a" ? folderA.guid : folderB.guid,
    details.targetIndex
  );

  await dumpFolderChildren(
    "folderA",
    details,
    folderA,
    folderB,
    originalAChildren,
    originalBChildren
  );
  await dumpFolderChildren(
    "folderB",
    details,
    folderA,
    folderB,
    originalAChildren,
    originalBChildren
  );

  Assert.equal(movedItems.length, childrenToUpdate.length);
  await checkExpectedResults(
    details,
    "folderA",
    folderA.guid,
    lastModified,
    movedItems,
    folderAChildren,
    folderBChildren
  );
  await checkExpectedResults(
    details,
    "folderB",
    folderB.guid,
    lastModified,
    movedItems,
    folderAChildren,
    folderBChildren
  );

  if (details.notifications) {
    let expectedNotifications = [];

    for (let notification of details.notifications) {
      let origItem =
        notification.originalFolder == "folderA"
          ? originalAChildren[notification.originalIndex]
          : originalBChildren[notification.originalIndex];
      let newFolder = notification.newFolder == "folderA" ? folderA : folderB;

      expectedNotifications.push({
        type: "bookmark-moved",
        id: await PlacesTestUtils.promiseItemId(origItem.guid),
        itemType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        url: origItem.url,
        guid: origItem.guid,
        parentGuid: newFolder.guid,
        source: PlacesUtils.bookmarks.SOURCES.DEFAULT,
        index: notification.newIndex,
        oldParentGuid: origItem.parentGuid,
        oldIndex: notification.originalIndex,
        isTagging: false,
        title: origItem.title,
        tags: "",
        frecency: 1,
        hidden: false,
        visitCount: 0,
        dateAdded: BOOKMARK_DATE_ADDED.getTime(),
        lastVisitDate: null,
      });
    }
    observer.check(expectedNotifications);
  }

  await checkLastModifiedForFolders(details, folderA, movedItems);
  await checkLastModifiedForFolders(details, folderB, movedItems);
}

const TYPE_BOOKMARK = PlacesUtils.bookmarks.TYPE_BOOKMARK;

add_task(async function invalid_input_throws() {
  Assert.throws(
    () => PlacesUtils.bookmarks.moveToFolder(),
    /guids should be an array/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.moveToFolder({}),
    /guids should be an array/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.moveToFolder([]),
    /guids should be an array of at least one item/
  );

  Assert.throws(
    () => PlacesUtils.bookmarks.moveToFolder(["test"]),
    /Expected only valid GUIDs to be passed/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.moveToFolder([null]),
    /Expected only valid GUIDs to be passed/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.moveToFolder([123]),
    /Expected only valid GUIDs to be passed/
  );

  Assert.throws(
    () => PlacesUtils.bookmarks.moveToFolder(["123456789012"], 123),
    /Error: parentGuid should be a valid GUID/
  );

  Assert.throws(
    () =>
      PlacesUtils.bookmarks.moveToFolder(
        ["123456789012"],
        PlacesUtils.bookmarks.rootGuid
      ),
    /Cannot move bookmarks into root/
  );

  Assert.throws(
    () =>
      PlacesUtils.bookmarks.moveToFolder(["123456789012"], "123456789012", -2),
    /index should be a number greater than/
  );
  Assert.throws(
    () =>
      PlacesUtils.bookmarks.moveToFolder(
        ["123456789012"],
        "123456789012",
        "sdffd"
      ),
    /index should be a number greater than/
  );
});

add_task(async function test_move_nonexisting_bookmark_rejects() {
  await Assert.rejects(
    PlacesUtils.bookmarks.moveToFolder(["123456789012"], "123456789012", -1),
    /No bookmarks found for the provided GUID/,
    "Should reject when moving a non-existing bookmark"
  );
});

add_task(async function test_move_folder_into_descendant_rejects() {
  let parent = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
  let descendant = await PlacesUtils.bookmarks.insert({
    parentGuid: parent.guid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });

  await Assert.rejects(
    PlacesUtils.bookmarks.moveToFolder([parent.guid], parent.guid, 0),
    /Cannot insert a folder into itself or one of its descendants/,
    "Should reject when moving a folder into itself"
  );

  await Assert.rejects(
    PlacesUtils.bookmarks.moveToFolder([parent.guid], descendant.guid, 0),
    /Cannot insert a folder into itself or one of its descendants/,
    "Should reject when moving a folder into a descendant"
  );
});

add_task(async function test_move_from_differnt_with_no_target_rejects() {
  let bm1 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
  let bm2 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });

  await Assert.rejects(
    PlacesUtils.bookmarks.moveToFolder([bm1.guid, bm2.guid], null, -1),
    /All bookmarks should be in the same folder if no parent is specified/,
    "Should reject when moving bookmarks from different folders with no target folder"
  );
});

add_task(async function test_move_append_same_folder() {
  await testMoveToFolder({
    initial: {
      folderA: [
        { type: TYPE_BOOKMARK },
        { type: TYPE_BOOKMARK, move: true },
        { type: TYPE_BOOKMARK },
      ],
      folderB: [],
    },
    targetFolder: "a",
    targetIndex: -1,
    expected: {
      folderA: [
        { folder: "a", originalIndex: 0 },
        { folder: "a", originalIndex: 2 },
        { folder: "a", originalIndex: 1 },
      ],
      folderB: [],
    },
    notifications: [
      {
        originalFolder: "folderA",
        originalIndex: 1,
        newFolder: "folderA",
        newIndex: 2,
      },
    ],
  });
});

add_task(async function test_move_append_multiple_same_folder() {
  await testMoveToFolder({
    initial: {
      folderA: [
        { type: TYPE_BOOKMARK, move: true },
        { type: TYPE_BOOKMARK, move: true },
        { type: TYPE_BOOKMARK, move: true },
        { type: TYPE_BOOKMARK },
      ],
      folderB: [],
    },
    targetFolder: "a",
    targetIndex: -1,
    expected: {
      folderA: [
        { folder: "a", originalIndex: 3 },
        { folder: "a", originalIndex: 0 },
        { folder: "a", originalIndex: 1 },
        { folder: "a", originalIndex: 2 },
      ],
      folderB: [],
    },
    // These are all inserted at position 3 as that's what the views require
    // to be notified, to ensure the new items are displayed in their correct
    // positions.
    notifications: [
      {
        originalFolder: "folderA",
        originalIndex: 0,
        newFolder: "folderA",
        newIndex: 3,
      },
      {
        originalFolder: "folderA",
        originalIndex: 1,
        newFolder: "folderA",
        newIndex: 3,
      },
      {
        originalFolder: "folderA",
        originalIndex: 2,
        newFolder: "folderA",
        newIndex: 3,
      },
    ],
  });
});

add_task(async function test_move_append_multiple_new_folder() {
  await testMoveToFolder({
    initial: {
      folderA: [
        { type: TYPE_BOOKMARK, move: true },
        { type: TYPE_BOOKMARK, move: true },
        { type: TYPE_BOOKMARK, move: true },
      ],
      folderB: [],
    },
    targetFolder: "b",
    targetIndex: -1,
    expected: {
      folderA: [],
      folderB: [
        { folder: "a", originalIndex: 0 },
        { folder: "a", originalIndex: 1 },
        { folder: "a", originalIndex: 2 },
      ],
    },
    notifications: [
      {
        originalFolder: "folderA",
        originalIndex: 0,
        newFolder: "folderB",
        newIndex: 0,
      },
      {
        originalFolder: "folderA",
        originalIndex: 1,
        newFolder: "folderB",
        newIndex: 1,
      },
      {
        originalFolder: "folderA",
        originalIndex: 2,
        newFolder: "folderB",
        newIndex: 2,
      },
    ],
  });
});

add_task(async function test_move_append_multiple_new_folder_with_existing() {
  await testMoveToFolder({
    initial: {
      folderA: [
        { type: TYPE_BOOKMARK, move: true },
        { type: TYPE_BOOKMARK, move: true },
        { type: TYPE_BOOKMARK, move: true },
      ],
      folderB: [{ type: TYPE_BOOKMARK }, { type: TYPE_BOOKMARK }],
    },
    targetFolder: "b",
    targetIndex: -1,
    expected: {
      folderA: [],
      folderB: [
        { folder: "b", originalIndex: 0 },
        { folder: "b", originalIndex: 1 },
        { folder: "a", originalIndex: 0 },
        { folder: "a", originalIndex: 1 },
        { folder: "a", originalIndex: 2 },
      ],
    },
    notifications: [
      {
        originalFolder: "folderA",
        originalIndex: 0,
        newFolder: "folderB",
        newIndex: 2,
      },
      {
        originalFolder: "folderA",
        originalIndex: 1,
        newFolder: "folderB",
        newIndex: 3,
      },
      {
        originalFolder: "folderA",
        originalIndex: 2,
        newFolder: "folderB",
        newIndex: 4,
      },
    ],
  });
});

add_task(async function test_move_insert_same_folder_up() {
  await testMoveToFolder({
    initial: {
      folderA: [
        { type: TYPE_BOOKMARK },
        { type: TYPE_BOOKMARK },
        { type: TYPE_BOOKMARK, move: true },
      ],
      folderB: [],
    },
    targetFolder: "a",
    targetIndex: 0,
    expected: {
      folderA: [
        { folder: "a", originalIndex: 2 },
        { folder: "a", originalIndex: 0 },
        { folder: "a", originalIndex: 1 },
      ],
      folderB: [],
    },
    notifications: [
      {
        originalFolder: "folderA",
        originalIndex: 2,
        newFolder: "folderA",
        newIndex: 0,
      },
    ],
  });
});

add_task(async function test_move_insert_same_folder_down() {
  await testMoveToFolder({
    initial: {
      folderA: [
        { type: TYPE_BOOKMARK, move: true },
        { type: TYPE_BOOKMARK },
        { type: TYPE_BOOKMARK },
      ],
      folderB: [],
    },
    targetFolder: "a",
    targetIndex: 2,
    expected: {
      folderA: [
        { folder: "a", originalIndex: 1 },
        { folder: "a", originalIndex: 0 },
        { folder: "a", originalIndex: 2 },
      ],
      folderB: [],
    },
    notifications: [
      {
        originalFolder: "folderA",
        originalIndex: 0,
        newFolder: "folderA",
        newIndex: 1,
      },
    ],
  });
});

add_task(
  async function test_move_insert_multiple_same_folder_split_locations() {
    await testMoveToFolder({
      initial: {
        folderA: [
          { type: TYPE_BOOKMARK, move: true },
          { type: TYPE_BOOKMARK },
          { type: TYPE_BOOKMARK },
          { type: TYPE_BOOKMARK, move: true },
          { type: TYPE_BOOKMARK },
          { type: TYPE_BOOKMARK },
          { type: TYPE_BOOKMARK, move: true },
          { type: TYPE_BOOKMARK },
          { type: TYPE_BOOKMARK },
          { type: TYPE_BOOKMARK, move: true },
        ],
        folderB: [],
      },
      targetFolder: "a",
      targetIndex: 2,
      expected: {
        folderA: [
          { folder: "a", originalIndex: 1 },
          { folder: "a", originalIndex: 0 },
          { folder: "a", originalIndex: 3 },
          { folder: "a", originalIndex: 6 },
          { folder: "a", originalIndex: 9 },
          { folder: "a", originalIndex: 2 },
          { folder: "a", originalIndex: 4 },
          { folder: "a", originalIndex: 5 },
          { folder: "a", originalIndex: 7 },
          { folder: "a", originalIndex: 8 },
        ],
        folderB: [],
      },
      notifications: [
        {
          originalFolder: "folderA",
          originalIndex: 0,
          newFolder: "folderA",
          newIndex: 1,
        },
        {
          originalFolder: "folderA",
          originalIndex: 3,
          newFolder: "folderA",
          newIndex: 2,
        },
        {
          originalFolder: "folderA",
          originalIndex: 6,
          newFolder: "folderA",
          newIndex: 3,
        },
        {
          originalFolder: "folderA",
          originalIndex: 9,
          newFolder: "folderA",
          newIndex: 4,
        },
      ],
    });
  }
);

add_task(async function test_move_folder_with_descendant() {
  let parent = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: parent.guid,
    url: "http://example.com/",
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
  });
  let descendant = await PlacesUtils.bookmarks.insert({
    parentGuid: parent.guid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
  Assert.equal(descendant.index, 1);
  let lastModified = bm.lastModified;

  // This is moving to a nonexisting index by purpose, it will be appended.
  [bm] = await PlacesUtils.bookmarks.moveToFolder(
    [bm.guid],
    descendant.guid,
    1
  );
  checkBookmarkObject(bm);
  Assert.equal(bm.parentGuid, descendant.guid);
  Assert.equal(bm.index, 0);
  Assert.ok(bm.lastModified >= lastModified);

  parent = await PlacesUtils.bookmarks.fetch(parent.guid);
  descendant = await PlacesUtils.bookmarks.fetch(descendant.guid);
  Assert.deepEqual(parent.lastModified, bm.lastModified);
  Assert.deepEqual(descendant.lastModified, bm.lastModified);
  Assert.equal(descendant.index, 0);

  bm = await PlacesUtils.bookmarks.fetch(bm.guid);
  Assert.equal(bm.parentGuid, descendant.guid);
  Assert.equal(bm.index, 0);

  [bm] = await PlacesUtils.bookmarks.moveToFolder([bm.guid], parent.guid, 0);
  Assert.equal(bm.parentGuid, parent.guid);
  Assert.equal(bm.index, 0);

  descendant = await PlacesUtils.bookmarks.fetch(descendant.guid);
  Assert.equal(descendant.index, 1);
});
