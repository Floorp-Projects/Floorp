/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Import common head.
{
  /* import-globals-from ../head_common.js */
  let commonFile = do_get_file("../head_common.js", false);
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

function expectPlacesObserverNotifications(
  types,
  checkAllArgs = true,
  skipDescendants = false
) {
  let notifications = [];
  let listener = events => {
    for (let event of events) {
      switch (event.type) {
        case "bookmark-added":
          notifications.push({
            type: event.type,
            id: event.id,
            itemType: event.itemType,
            parentId: event.parentId,
            index: event.index,
            url: event.url || undefined,
            title: event.title,
            dateAdded: new Date(event.dateAdded),
            guid: event.guid,
            parentGuid: event.parentGuid,
            source: event.source,
            isTagging: event.isTagging,
          });
          break;
        case "bookmark-removed":
          if (
            !(
              skipDescendants &&
              event.isDescendantRemoval &&
              !PlacesUtils.bookmarks.userContentRoots.includes(event.parentGuid)
            )
          ) {
            if (checkAllArgs) {
              notifications.push({
                type: event.type,
                id: event.id,
                itemType: event.itemType,
                parentId: event.parentId,
                index: event.index,
                url: event.url || null,
                guid: event.guid,
                parentGuid: event.parentGuid,
                source: event.source,
                isTagging: event.isTagging,
              });
            } else {
              notifications.push({
                type: event.type,
                guid: event.guid,
              });
            }
          }
          break;
        case "bookmark-moved":
          notifications.push({
            type: event.type,
            id: event.id,
            itemType: event.itemType,
            url: event.url,
            guid: event.guid,
            parentGuid: event.parentGuid,
            source: event.source,
            index: event.index,
            oldParentGuid: event.oldParentGuid,
            oldIndex: event.oldIndex,
            isTagging: event.isTagging,
            title: event.title,
            tags: event.tags,
            frecency: event.frecency,
            hidden: event.hidden,
            visitCount: event.visitCount,
            dateAdded: event.dateAdded,
            lastVisitDate: event.lastVisitDate,
          });
          break;
        case "bookmark-tags-changed":
          notifications.push({
            type: event.type,
            id: event.id,
            itemType: event.itemType,
            url: event.url,
            guid: event.guid,
            parentGuid: event.parentGuid,
            tags: event.tags,
            lastModified: new Date(event.lastModified),
            source: event.source,
            isTagging: event.isTagging,
          });
          break;
        case "bookmark-time-changed":
          notifications.push({
            type: event.type,
            id: event.id,
            itemType: event.itemType,
            url: event.url,
            guid: event.guid,
            parentGuid: event.parentGuid,
            dateAdded: new Date(event.dateAdded),
            lastModified: new Date(event.lastModified),
            source: event.source,
            isTagging: event.isTagging,
          });
          break;
        case "bookmark-title-changed":
          notifications.push({
            type: event.type,
            id: event.id,
            itemType: event.itemType,
            url: event.url,
            guid: event.guid,
            parentGuid: event.parentGuid,
            title: event.title,
            lastModified: new Date(event.lastModified),
            source: event.source,
            isTagging: event.isTagging,
          });
          break;
        case "bookmark-url-changed":
          notifications.push({
            type: event.type,
            id: event.id,
            itemType: event.itemType,
            url: event.url,
            guid: event.guid,
            parentGuid: event.parentGuid,
            source: event.source,
            isTagging: event.isTagging,
            lastModified: new Date(event.lastModified),
          });
          break;
      }
    }
  };
  PlacesUtils.observers.addListener(types, listener);
  return {
    check(expectedNotifications) {
      PlacesUtils.observers.removeListener(types, listener);
      Assert.deepEqual(notifications, expectedNotifications);
    },
  };
}
