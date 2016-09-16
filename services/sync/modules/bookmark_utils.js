/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["BookmarkSpecialIds", "BookmarkAnnos"];

const { utils: Cu, interfaces: Ci, classes: Cc } = Components;

Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/async.js");

let BookmarkAnnos = {
  ALLBOOKMARKS_ANNO:    "AllBookmarks",
  DESCRIPTION_ANNO:     "bookmarkProperties/description",
  SIDEBAR_ANNO:         "bookmarkProperties/loadInSidebar",
  MOBILEROOT_ANNO:      "mobile/bookmarksRoot",
  MOBILE_ANNO:          "MobileBookmarks",
  EXCLUDEBACKUP_ANNO:   "places/excludeFromBackup",
  SMART_BOOKMARKS_ANNO: "Places/SmartBookmark",
  PARENT_ANNO:          "sync/parent",
  ORGANIZERQUERY_ANNO:  "PlacesOrganizer/OrganizerQuery",
};

// Accessing `PlacesUtils.bookmarks` initializes the bookmarks service, which,
// in turn, initializes the database and upgrades the schema as a side effect.
// This causes Sync unit tests that exercise older database formats to fail,
// so we define this map as a lazy getter.
XPCOMUtils.defineLazyGetter(this, "SpecialGUIDToPlacesGUID", () => {
  return {
    "menu": PlacesUtils.bookmarks.menuGuid,
    "places": PlacesUtils.bookmarks.rootGuid,
    "tags": PlacesUtils.bookmarks.tagsGuid,
    "toolbar": PlacesUtils.bookmarks.toolbarGuid,
    "unfiled": PlacesUtils.bookmarks.unfiledGuid,
    "mobile": PlacesUtils.bookmarks.mobileGuid,
  };
});

let BookmarkSpecialIds = {

  // Special IDs.
  guids: ["menu", "places", "tags", "toolbar", "unfiled", "mobile"],

  // Accessors for IDs.
  isSpecialGUID: function isSpecialGUID(g) {
    return this.guids.indexOf(g) != -1;
  },

  specialIdForGUID: function specialIdForGUID(guid) {
    return this[guid];
  },

  specialGUIDForId: function specialGUIDForId(id) {
    for (let guid of this.guids)
      if (this.specialIdForGUID(guid) == id)
        return guid;
    return null;
  },

  syncIDToPlacesGUID(g) {
    return g in SpecialGUIDToPlacesGUID ? SpecialGUIDToPlacesGUID[g] : g;
  },

  get menu() {
    return PlacesUtils.bookmarksMenuFolderId;
  },
  get places() {
    return PlacesUtils.placesRootId;
  },
  get tags() {
    return PlacesUtils.tagsFolderId;
  },
  get toolbar() {
    return PlacesUtils.toolbarFolderId;
  },
  get unfiled() {
    return PlacesUtils.unfiledBookmarksFolderId;
  },
  get mobile() {
    return PlacesUtils.mobileFolderId;
  },
};
