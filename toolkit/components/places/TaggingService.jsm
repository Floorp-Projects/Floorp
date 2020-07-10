/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ComponentUtils } = ChromeUtils.import(
  "resource://gre/modules/ComponentUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { PlacesUtils } = ChromeUtils.import(
  "resource://gre/modules/PlacesUtils.jsm"
);

const TOPIC_SHUTDOWN = "places-shutdown";

/**
 * The Places Tagging Service
 */
function TaggingService() {
  this.handlePlacesEvents = this.handlePlacesEvents.bind(this);

  // Observe bookmarks changes.
  PlacesUtils.bookmarks.addObserver(this);
  PlacesUtils.observers.addListener(
    ["bookmark-added", "bookmark-removed"],
    this.handlePlacesEvents
  );

  // Cleanup on shutdown.
  Services.obs.addObserver(this, TOPIC_SHUTDOWN);
}

TaggingService.prototype = {
  /**
   * Creates a tag container under the tags-root with the given name.
   *
   * @param aTagName
   *        the name for the new tag.
   * @param aSource
   *        a change source constant from nsINavBookmarksService::SOURCE_*.
   * @returns the id of the new tag container.
   */
  _createTag: function TS__createTag(aTagName, aSource) {
    var newFolderId = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.tagsFolderId,
      aTagName,
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      /* aGuid */ null,
      aSource
    );
    // Add the folder to our local cache, so we can avoid doing this in the
    // observer that would have to check itemType.
    this._tagFolders[newFolderId] = aTagName;

    return newFolderId;
  },

  /**
   * Checks whether the given uri is tagged with the given tag.
   *
   * @param [in] aURI
   *        url to check for
   * @param [in] aTagName
   *        the tag to check for
   * @returns the item id if the URI is tagged with the given tag, -1
   *          otherwise.
   */
  _getItemIdForTaggedURI: function TS__getItemIdForTaggedURI(aURI, aTagName) {
    var tagId = this._getItemIdForTag(aTagName);
    if (tagId == -1) {
      return -1;
    }
    // Using bookmarks service API for this would be a pain.
    // Until tags implementation becomes sane, go the query way.
    let db = PlacesUtils.history.DBConnection;
    let stmt = db.createStatement(
      `SELECT id FROM moz_bookmarks
       WHERE parent = :tag_id
       AND fk = (SELECT id FROM moz_places WHERE url_hash = hash(:page_url) AND url = :page_url)`
    );
    stmt.params.tag_id = tagId;
    stmt.params.page_url = aURI.spec;
    try {
      if (stmt.executeStep()) {
        return stmt.row.id;
      }
    } finally {
      stmt.finalize();
    }
    return -1;
  },

  /**
   * Returns the folder id for a tag, or -1 if not found.
   * @param [in] aTag
   *        string tag to search for
   * @returns integer id for the bookmark folder for the tag
   */
  _getItemIdForTag: function TS_getItemIdForTag(aTagName) {
    for (var i in this._tagFolders) {
      if (aTagName.toLowerCase() == this._tagFolders[i].toLowerCase()) {
        return parseInt(i);
      }
    }
    return -1;
  },
  /**
   * Makes a proper array of tag objects like  { id: number, name: string }.
   *
   * @param aTags
   *        Array of tags.  Entries can be tag names or concrete item id.
   * @param trim [optional]
   *        Whether to trim passed-in named tags. Defaults to false.
   * @return Array of tag objects like { id: number, name: string }.
   *
   * @throws Cr.NS_ERROR_INVALID_ARG if any element of the input array is not
   *         a valid tag.
   */
  _convertInputMixedTagsArray(aTags, trim = false) {
    // Handle sparse array with a .filter.
    return aTags
      .filter(tag => tag !== undefined)
      .map(idOrName => {
        let tag = {};
        if (typeof idOrName == "number" && this._tagFolders[idOrName]) {
          // This is a tag folder id.
          tag.id = idOrName;
          // We can't know the name at this point, since a previous tag could
          // want to change it.
          tag.__defineGetter__("name", () => this._tagFolders[tag.id]);
        } else if (
          typeof idOrName == "string" &&
          !!idOrName.length &&
          idOrName.length <= PlacesUtils.bookmarks.MAX_TAG_LENGTH
        ) {
          // This is a tag name.
          tag.name = trim ? idOrName.trim() : idOrName;
          // We can't know the id at this point, since a previous tag could
          // have created it.
          tag.__defineGetter__("id", () => this._getItemIdForTag(tag.name));
        } else {
          throw Components.Exception(
            "Invalid tag value",
            Cr.NS_ERROR_INVALID_ARG
          );
        }
        return tag;
      });
  },

  // nsITaggingService
  tagURI: function TS_tagURI(aURI, aTags, aSource) {
    if (!aURI || !aTags || !Array.isArray(aTags) || !aTags.length) {
      throw Components.Exception(
        "Invalid value for tags",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    // This also does some input validation.
    let tags = this._convertInputMixedTagsArray(aTags, true);

    for (let tag of tags) {
      if (tag.id == -1) {
        // Tag does not exist yet, create it.
        this._createTag(tag.name, aSource);
      }

      let itemId = this._getItemIdForTaggedURI(aURI, tag.name);
      if (itemId == -1) {
        // The provided URI is not yet tagged, add a tag for it.
        // Note that bookmarks under tag containers must have null titles.
        PlacesUtils.bookmarks.insertBookmark(
          tag.id,
          aURI,
          PlacesUtils.bookmarks.DEFAULT_INDEX,
          /* aTitle */ null,
          /* aGuid */ null,
          aSource
        );
      } else {
        // Otherwise, bump the tag's timestamp, so that we can increment the
        // sync change counter for all bookmarks with the URI.
        PlacesUtils.bookmarks.setItemLastModified(
          itemId,
          PlacesUtils.toPRTime(Date.now()),
          aSource
        );
      }

      // Try to preserve user's tag name casing.
      // Rename the tag container so the Places view matches the most-recent
      // user-typed value.
      if (PlacesUtils.bookmarks.getItemTitle(tag.id) != tag.name) {
        // this._tagFolders is updated by the bookmarks observer.
        PlacesUtils.bookmarks.setItemTitle(tag.id, tag.name, aSource);
      }
    }
  },

  /**
   * Removes the tag container from the tags root if the given tag is empty.
   *
   * @param aTagId
   *        the itemId of the tag element under the tags root
   * @param aSource
   *        a change source constant from nsINavBookmarksService::SOURCE_*
   */
  _removeTagIfEmpty: function TS__removeTagIfEmpty(aTagId, aSource) {
    let count = 0;
    let db = PlacesUtils.history.DBConnection;
    let stmt = db.createStatement(
      `SELECT count(*) AS count FROM moz_bookmarks
       WHERE parent = :tag_id`
    );
    stmt.params.tag_id = aTagId;
    try {
      if (stmt.executeStep()) {
        count = stmt.row.count;
      }
    } finally {
      stmt.finalize();
    }

    if (count == 0) {
      PlacesUtils.bookmarks.removeItem(aTagId, aSource);
    }
  },

  // nsITaggingService
  untagURI: function TS_untagURI(aURI, aTags, aSource) {
    if (!aURI || (aTags && (!Array.isArray(aTags) || !aTags.length))) {
      throw Components.Exception(
        "Invalid value for tags",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (!aTags) {
      // Passing null should clear all tags for aURI, see the IDL.
      // XXXmano: write a perf-sensitive version of this code path...
      aTags = this.getTagsForURI(aURI);
    }

    // This also does some input validation.
    let tags = this._convertInputMixedTagsArray(aTags);

    let isAnyTagNotTrimmed = tags.some(tag => /^\s|\s$/.test(tag.name));
    if (isAnyTagNotTrimmed) {
      throw Components.Exception(
        "At least one tag passed to untagURI was not trimmed",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    for (let tag of tags) {
      if (tag.id != -1) {
        // A tag could exist.
        let itemId = this._getItemIdForTaggedURI(aURI, tag.name);
        if (itemId != -1) {
          // There is a tagged item.
          PlacesUtils.bookmarks.removeItem(itemId, aSource);
        }
      }
    }
  },

  // nsITaggingService
  getTagsForURI: function TS_getTagsForURI(aURI) {
    if (!aURI) {
      throw Components.Exception("Invalid uri", Cr.NS_ERROR_INVALID_ARG);
    }

    let tags = [];
    let db = PlacesUtils.history.DBConnection;
    let stmt = db.createStatement(
      `SELECT t.id AS folderId
       FROM moz_bookmarks b
       JOIN moz_bookmarks t on t.id = b.parent
       WHERE b.fk = (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url) AND
       t.parent = :tags_root
       ORDER BY b.lastModified DESC, b.id DESC`
    );
    stmt.params.url = aURI.spec;
    stmt.params.tags_root = PlacesUtils.tagsFolderId;
    try {
      while (stmt.executeStep()) {
        try {
          tags.push(this._tagFolders[stmt.row.folderId]);
        } catch (ex) {}
      }
    } finally {
      stmt.finalize();
    }

    // sort the tag list
    tags.sort(function(a, b) {
      return a.toLowerCase().localeCompare(b.toLowerCase());
    });
    return tags;
  },

  __tagFolders: null,
  get _tagFolders() {
    if (!this.__tagFolders) {
      this.__tagFolders = [];

      let db = PlacesUtils.history.DBConnection;
      let stmt = db.createStatement(
        "SELECT id, title FROM moz_bookmarks WHERE parent = :tags_root "
      );
      stmt.params.tags_root = PlacesUtils.tagsFolderId;
      try {
        while (stmt.executeStep()) {
          this.__tagFolders[stmt.row.id] = stmt.row.title;
        }
      } finally {
        stmt.finalize();
      }
    }

    return this.__tagFolders;
  },

  // nsIObserver
  observe: function TS_observe(aSubject, aTopic, aData) {
    if (aTopic == TOPIC_SHUTDOWN) {
      PlacesUtils.bookmarks.removeObserver(this);
      PlacesUtils.observers.removeListener(
        ["bookmark-added", "bookmark-removed"],
        this.handlePlacesEvents
      );
      Services.obs.removeObserver(this, TOPIC_SHUTDOWN);
    }
  },

  /**
   * If the only bookmark items associated with aURI are contained in tag
   * folders, returns the IDs of those items.  This can be the case if
   * the URI was bookmarked and tagged at some point, but the bookmark was
   * removed, leaving only the bookmark items in tag folders.  If the URI is
   * either properly bookmarked or not tagged just returns and empty array.
   *
   * @param   aURI
   *          A URI (string) that may or may not be bookmarked
   * @returns an array of item ids
   */
  _getTaggedItemIdsIfUnbookmarkedURI: function TS__getTaggedItemIdsIfUnbookmarkedURI(
    url
  ) {
    var itemIds = [];
    var isBookmarked = false;

    // Using bookmarks service API for this would be a pain.
    // Until tags implementation becomes sane, go the query way.
    let db = PlacesUtils.history.DBConnection;
    let stmt = db.createStatement(
      `SELECT id, parent
       FROM moz_bookmarks
       WHERE fk = (SELECT id FROM moz_places WHERE url_hash = hash(:page_url) AND url = :page_url)`
    );
    stmt.params.page_url = url;
    try {
      while (stmt.executeStep() && !isBookmarked) {
        if (this._tagFolders[stmt.row.parent]) {
          // This is a tag entry.
          itemIds.push(stmt.row.id);
        } else {
          // This is a real bookmark, so the bookmarked URI is not an orphan.
          isBookmarked = true;
        }
      }
    } finally {
      stmt.finalize();
    }

    return isBookmarked ? [] : itemIds;
  },

  handlePlacesEvents(events) {
    for (let event of events) {
      switch (event.type) {
        case "bookmark-added":
          if (
            !event.isTagging ||
            event.itemType != PlacesUtils.bookmarks.TYPE_FOLDER
          ) {
            continue;
          }

          this._tagFolders[event.id] = event.title;
          break;
        case "bookmark-removed":
          // Item is a tag folder.
          if (
            event.parentId == PlacesUtils.tagsFolderId &&
            this._tagFolders[event.id]
          ) {
            delete this._tagFolders[event.id];
            break;
          }

          Services.tm.dispatchToMainThread(() => {
            if (event.url && !this._tagFolders[event.parentId]) {
              // Item is a bookmark that was removed from a non-tag folder.
              // If the only bookmark items now associated with the bookmark's URI are
              // contained in tag folders, the URI is no longer properly bookmarked, so
              // untag it.
              let itemIds = this._getTaggedItemIdsIfUnbookmarkedURI(event.url);
              for (let i = 0; i < itemIds.length; i++) {
                try {
                  PlacesUtils.bookmarks.removeItem(itemIds[i], event.source);
                } catch (ex) {}
              }
            } else if (event.url && this._tagFolders[event.parentId]) {
              // Item is a tag entry.  If this was the last entry for this tag, remove it.
              this._removeTagIfEmpty(event.parentId, event.source);
            }
          });
          break;
      }
    }
  },

  onItemChanged: function TS_onItemChanged(
    aItemId,
    aProperty,
    aIsAnnotationProperty,
    aNewValue,
    aLastModified,
    aItemType
  ) {
    if (aProperty == "title" && this._tagFolders[aItemId]) {
      this._tagFolders[aItemId] = aNewValue;
    }
  },

  onItemMoved: function TS_onItemMoved(
    aItemId,
    aOldParent,
    aOldIndex,
    aNewParent,
    aNewIndex,
    aItemType
  ) {
    if (
      this._tagFolders[aItemId] &&
      PlacesUtils.tagsFolderId == aOldParent &&
      PlacesUtils.tagsFolderId != aNewParent
    ) {
      delete this._tagFolders[aItemId];
    }
  },

  onItemVisited() {},
  onBeginUpdateBatch() {},
  onEndUpdateBatch() {},

  // nsISupports

  classID: Components.ID("{bbc23860-2553-479d-8b78-94d9038334f7}"),

  _xpcom_factory: ComponentUtils.generateSingletonFactory(TaggingService),

  QueryInterface: ChromeUtils.generateQI([
    "nsITaggingService",
    "nsINavBookmarkObserver",
    "nsIObserver",
  ]),
};

/**
 * Class tracking a single tag autocomplete search.
 */
class TagSearch {
  constructor(searchString, autocompleteSearch, listener) {
    // We need a result regardless of having matches.
    this._result = Cc[
      "@mozilla.org/autocomplete/simple-result;1"
    ].createInstance(Ci.nsIAutoCompleteSimpleResult);
    this._result.setDefaultIndex(0);
    this._result.setSearchString(searchString);

    this._autocompleteSearch = autocompleteSearch;
    this._listener = listener;
  }

  async start() {
    if (this._canceled) {
      throw new Error("Can't restart a canceled search");
    }

    let searchString = this._result.searchString;
    // Only search on characters for the last tag.
    let index = Math.max(
      searchString.lastIndexOf(","),
      searchString.lastIndexOf(";")
    );
    let before = "";
    if (index != -1) {
      before = searchString.slice(0, index + 1);
      searchString = searchString.slice(index + 1);
      // skip past whitespace
      var m = searchString.match(/\s+/);
      if (m) {
        before += m[0];
        searchString = searchString.slice(m[0].length);
      }
    }

    if (searchString.length) {
      let tags = await PlacesUtils.bookmarks.fetchTags();
      if (this._canceled) {
        return;
      }

      let lcSearchString = searchString.toLowerCase();
      let matchingTags = tags
        .filter(t => t.name.toLowerCase().startsWith(lcSearchString))
        .map(t => t.name);

      for (let i = 0; i < matchingTags.length; ++i) {
        let tag = matchingTags[i];
        // For each match, prepend what the user has typed so far.
        this._result.appendMatch(before + tag, tag);
        // In case of many tags, notify once every 10.
        if (i % 10 == 0) {
          this._notifyResult(true);
          // yield to avoid monopolizing the main-thread
          await new Promise(resolve =>
            Services.tm.dispatchToMainThread(resolve)
          );
          if (this._canceled) {
            return;
          }
        }
      }
    }

    // Search is done.
    this._notifyResult(false);
  }

  cancel() {
    this._canceled = true;
  }

  _notifyResult(searchOngoing) {
    let resultCode = this._result.matchCount
      ? "RESULT_SUCCESS"
      : "RESULT_NOMATCH";
    if (searchOngoing) {
      resultCode += "_ONGOING";
    }
    this._result.setSearchResult(Ci.nsIAutoCompleteResult[resultCode]);
    this._listener.onSearchResult(this._autocompleteSearch, this._result);
  }
}

// Implements nsIAutoCompleteSearch
function TagAutoCompleteSearch() {}

TagAutoCompleteSearch.prototype = {
  /*
   * Search for a given string and notify a listener of the result.
   *
   * @param searchString - The string to search for
   * @param searchParam - An extra parameter
   * @param previousResult - A previous result to use for faster searching
   * @param listener - A listener to notify when the search is complete
   */
  startSearch(searchString, searchParam, previousResult, listener) {
    if (this._search) {
      this._search.cancel();
    }
    this._search = new TagSearch(searchString, this, listener);
    this._search.start().catch(Cu.reportError);
  },

  /**
   * Stop an asynchronous search that is in progress
   */
  stopSearch() {
    this._search.cancel();
    this._search = null;
  },

  classID: Components.ID("{1dcc23b0-d4cb-11dc-9ad6-479d56d89593}"),
  QueryInterface: ChromeUtils.generateQI(["nsIAutoCompleteSearch"]),
};

var EXPORTED_SYMBOLS = ["TaggingService", "TagAutoCompleteSearch"];
