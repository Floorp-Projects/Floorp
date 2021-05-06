/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// Import common head.
{
  /* import-globals-from ../head_common.js */
  let commonFile = do_get_file("../head_common.js", false);
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

function expectNotifications(checkAllArgs) {
  let notifications = [];
  let observer = new Proxy(NavBookmarkObserver, {
    get(target, name) {
      if (name == "check") {
        PlacesUtils.bookmarks.removeObserver(observer);
        return expectedNotifications =>
          Assert.deepEqual(notifications, expectedNotifications);
      }

      if (name.startsWith("onItem")) {
        return (...origArgs) => {
          let args = Array.from(origArgs, arg => {
            if (arg && arg instanceof Ci.nsIURI) {
              return new URL(arg.spec);
            }
            if (arg && typeof arg == "number" && arg >= Date.now() * 1000) {
              return PlacesUtils.toDate(arg);
            }
            return arg;
          });
          if (checkAllArgs) {
            notifications.push({ name, arguments: args });
          } else {
            notifications.push({ name, arguments: { guid: args[5] } });
          }
        };
      }

      if (name in target) {
        return target[name];
      }
      return undefined;
    },
  });
  PlacesUtils.bookmarks.addObserver(observer);
  return observer;
}

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
