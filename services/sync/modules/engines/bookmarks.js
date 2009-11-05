/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
 *  Jono DiCarlo <jdicarlo@mozilla.org>
 *  Anant Narayanan <anant@kix.in>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const EXPORTED_SYMBOLS = ['BookmarksEngine', 'BookmarksSharingManager'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const PARENT_ANNO = "weave/parent";
const PREDECESSOR_ANNO = "weave/predecessor";
const SERVICE_NOT_SUPPORTED = "Service not supported on this platform";

Cu.import("resource://gre/modules/utils.js");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/identity.js");
Cu.import("resource://weave/notifications.js");
Cu.import("resource://weave/resource.js");
Cu.import("resource://weave/type_records/bookmark.js");

// Lazily initialize the special top level folders
let kSpecialIds = {};
[["menu", "bookmarksMenuFolder"],
 ["places", "placesRoot"],
 ["tags", "tagsFolder"],
 ["toolbar", "toolbarFolder"],
 ["unfiled", "unfiledBookmarksFolder"],
].forEach(function([guid, placeName]) {
  Utils.lazy2(kSpecialIds, guid, function() Svc.Bookmark[placeName]);
});

// Create some helper functions to convert GUID/ids
function idForGUID(guid) {
  if (guid in kSpecialIds)
    return kSpecialIds[guid];
  return Svc.Bookmark.getItemIdForGUID(guid);
}
function GUIDForId(placeId) {
  for (let [guid, id] in Iterator(kSpecialIds))
    if (placeId == id)
      return guid;
  return Svc.Bookmark.getItemGUID(placeId);
}

function BookmarksEngine() {
  this._init();
}
BookmarksEngine.prototype = {
  __proto__: SyncEngine.prototype,
  name: "bookmarks",
  _displayName: "Bookmarks",
  description: "Keep your favorite links always at hand",
  logName: "Bookmarks",
  _recordObj: PlacesItem,
  _storeObj: BookmarksStore,
  _trackerObj: BookmarksTracker,

  _sync: Utils.batchSync("Bookmark", SyncEngine),

  _syncStartup: function _syncStart() {
    SyncEngine.prototype._syncStartup.call(this);

    // Lazily create a mapping of folder titles and separator positions to GUID
    let lazyMap = function() {
      delete this._folderTitles;
      delete this._separatorPos;

      let folderTitles = {};
      let separatorPos = {};
      for (let guid in this._store.getAllIDs()) {
        let id = idForGUID(guid);
        switch (Svc.Bookmark.getItemType(id)) {
          case Svc.Bookmark.TYPE_FOLDER:
            // Map the folder name to GUID
            folderTitles[Svc.Bookmark.getItemTitle(id)] = guid;
            break;
          case Svc.Bookmark.TYPE_SEPARATOR:
            // Map the separator position and parent position to GUID
            let parent = Svc.Bookmark.getFolderIdForItem(id);
            let pos = [id, parent].map(Svc.Bookmark.getItemIndex);
            separatorPos[pos] = guid;
            break;
        }
      }

      this._folderTitles = folderTitles;
      this._separatorPos = separatorPos;
    };

    // Make the getters that lazily build the mapping
    ["_folderTitles", "_separatorPos"].forEach(function(lazy) {
      this.__defineGetter__(lazy, function() {
        lazyMap.call(this);
        return this[lazy];
      });
    }, this);
  },

  _syncFinish: function _syncFinish() {
    SyncEngine.prototype._syncFinish.call(this);
    delete this._folderTitles;
    delete this._separatorPos;
  },

  _findDupe: function _findDupe(item) {
    switch (item.type) {
      case "bookmark":
      case "query":
      case "microsummary":
        // Find a bookmark that has the same uri
        let uri = Utils.makeURI(item.bmkUri);
        let localId = PlacesUtils.getMostRecentBookmarkForURI(uri);
        if (localId == -1)
          return;
        return GUIDForId(localId);
      case "folder":
      case "livemark":
        return this._folderTitles[item.title];
      case "separator":
        return this._separatorPos[item.pos];
    }
  },

  _handleDupe: function _handleDupe(item, dupeId) {
    // The local dupe has the lower id, so make it the winning id
    if (dupeId < item.id)
      [item.id, dupeId] = [dupeId, item.id];

    // Trigger id change from dupe to winning and update the server
    this._store.changeItemID(dupeId, item.id);
    this._deleteId(dupeId);
    this._tracker.changedIDs[item.id] = true;
  }
};

function BookmarksStore() {
  this._init();
}
BookmarksStore.prototype = {
  __proto__: Store.prototype,
  name: "bookmarks",
  _logName: "BStore",

  __bms: null,
  get _bms() {
    if (!this.__bms)
      this.__bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                   getService(Ci.nsINavBookmarksService);
    return this.__bms;
  },

  __hsvc: null,
  get _hsvc() {
    if (!this.__hsvc)
      this.__hsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                    getService(Ci.nsINavHistoryService);
    return this.__hsvc;
  },

  __ls: null,
  get _ls() {
    if (!this.__ls)
      this.__ls = Cc["@mozilla.org/browser/livemark-service;2"].
        getService(Ci.nsILivemarkService);
    return this.__ls;
  },

  get _ms() {
    let ms;
    try {
      ms = Cc["@mozilla.org/microsummary/service;1"].
        getService(Ci.nsIMicrosummaryService);
    } catch (e) {
      ms = null;
      this._log.warn("Could not load microsummary service");
      this._log.debug(e);
    }
    this.__defineGetter__("_ms", function() ms);
    return ms;
  },

  __ts: null,
  get _ts() {
    if (!this.__ts)
      this.__ts = Cc["@mozilla.org/browser/tagging-service;1"].
                  getService(Ci.nsITaggingService);
    return this.__ts;
  },


  itemExists: function BStore_itemExists(id) {
    return idForGUID(id) > 0;
  },

  // Hash of old GUIDs to the new renamed GUIDs
  aliases: {},

  applyIncoming: function BStore_applyIncoming(record) {
    // Ignore (accidental?) root changes
    if (record.id in kSpecialIds) {
      this._log.debug("Skipping change to root node: " + record.id);
      return;
    }

    // Convert GUID fields to the aliased GUID if necessary
    ["id", "parentid", "predecessorid"].forEach(function(field) {
      let alias = this.aliases[record[field]];
      if (alias != null)
        record[field] = alias;
    }, this);

    // Preprocess the record before doing the normal apply
    switch (record.type) {
      case "query": {
        // Convert the query uri if necessary
        if (record.bmkUri == null || record.folderName == null)
          break;

        // Tag something so that the tag exists
        let tag = record.folderName;
        let dummyURI = Utils.makeURI("about:weave#BStore_preprocess");
        this._ts.tagURI(dummyURI, [tag]);

        // Look for the id of the tag (that might have just been added)
        let tags = this._getNode(this._bms.tagsFolder);
        if (!(tags instanceof Ci.nsINavHistoryQueryResultNode))
          break;

        tags.containerOpen = true;
        for (let i = 0; i < tags.childCount; i++) {
          let child = tags.getChild(i);
          // Found the tag, so fix up the query to use the right id
          if (child.title == tag) {
            this._log.debug("query folder: " + tag + " = " + child.itemId);
            record.bmkUri = record.bmkUri.replace(/([:&]folder=)\d+/, "$1" +
              child.itemId);
            break;
          }
        }
        break;
      }
    }

    // Figure out the local id of the parent GUID if available
    let parentGUID = record.parentid;
    record._orphan = false;
    if (parentGUID != null) {
      let parentId = idForGUID(parentGUID);

      // Default to unfiled if we don't have the parent yet
      if (parentId <= 0) {
        this._log.trace("Reparenting to unfiled until parent is synced");
        record._orphan = true;
        parentId = kSpecialIds.unfiled;
      }

      // Save the parent id for modifying the bookmark later
      record._parent = parentId;
    }

    // Default to append unless we're not an orphan with the predecessor
    let predGUID = record.predecessorid;
    record._insertPos = Svc.Bookmark.DEFAULT_INDEX;
    if (!record._orphan) {
      // No predecessor means it's the first item
      if (predGUID == null)
        record._insertPos = 0;
      else {
        // The insert position is one after the predecessor of the same parent
        let predId = idForGUID(predGUID);
        if (predId != -1 && this._getParentGUIDForId(predId) == parentGUID) {
          record._insertPos = Svc.Bookmark.getItemIndex(predId) + 1;
          record._predId = predId;
        }
        else
          this._log.trace("Appending to end until predecessor is synced");
      }
    }

    // Do the normal processing of incoming records
    Store.prototype.applyIncoming.apply(this, arguments);

    // Do some post-processing if we have an item
    let itemId = idForGUID(record.id);
    if (itemId > 0) {
      // Move any children that are looking for this folder as a parent
      if (record.type == "folder")
        this._reparentOrphans(itemId);

      // Create an annotation to remember that it needs a parent
      // XXX Work around Bug 510628 by prepending parenT
      if (record._orphan)
        Utils.anno(itemId, PARENT_ANNO, "T" + parentGUID);
      // It's now in the right folder, so move annotated items behind this
      else
        this._attachFollowers(itemId);

      // Create an annotation if we have a predecessor but no position
      // XXX Work around Bug 510628 by prepending predecessoR
      if (predGUID != null && record._insertPos == Svc.Bookmark.DEFAULT_INDEX)
        Utils.anno(itemId, PREDECESSOR_ANNO, "R" + predGUID);
    }
  },

  /**
   * Find all ids of items that have a given value for an annotation
   */
  _findAnnoItems: function BStore__findAnnoItems(anno, val) {
    // XXX Work around Bug 510628 by prepending parenT
    if (anno == PARENT_ANNO)
      val = "T" + val;
    // XXX Work around Bug 510628 by prepending predecessoR
    else if (anno == PREDECESSOR_ANNO)
      val = "R" + val;

    return Svc.Annos.getItemsWithAnnotation(anno, {}).filter(function(id)
      Utils.anno(id, anno) == val);
  },

  /**
   * For the provided parent item, attach its children to it
   */
  _reparentOrphans: function _reparentOrphans(parentId) {
    // Find orphans and reunite with this folder parent
    let parentGUID = GUIDForId(parentId);
    let orphans = this._findAnnoItems(PARENT_ANNO, parentGUID);

    this._log.debug("Reparenting orphans " + orphans + " to " + parentId);
    orphans.forEach(function(orphan) {
      // Append the orphan under the parent unless it's supposed to be first
      let insertPos = Svc.Bookmark.DEFAULT_INDEX;
      if (!Svc.Annos.itemHasAnnotation(orphan, PREDECESSOR_ANNO))
        insertPos = 0;

      // Move the orphan to the parent and drop the missing parent annotation
      Svc.Bookmark.moveItem(orphan, parentId, insertPos);
      Svc.Annos.removeItemAnnotation(orphan, PARENT_ANNO);
    });
    
    // Fix up the ordering of the now-parented items
    orphans.forEach(this._attachFollowers, this);
  },

  /**
   * Move an item and all of its followers to a new position until reaching an
   * item that shouldn't be moved
   */
  _moveItemChain: function BStore__moveItemChain(itemId, insertPos, stopId) {
    let parentId = Svc.Bookmark.getFolderIdForItem(itemId);

    // Keep processing the item chain until it loops to the stop item
    do {
      // Figure out what's next in the chain
      let itemPos = Svc.Bookmark.getItemIndex(itemId);
      let nextId = Svc.Bookmark.getIdForItemAt(parentId, itemPos + 1);

      Svc.Bookmark.moveItem(itemId, parentId, insertPos);
      this._log.trace("Moved " + itemId + " to " + insertPos);

      // Prepare for the next item in the chain
      insertPos = Svc.Bookmark.getItemIndex(itemId) + 1;
      itemId = nextId;

      // Stop if we ran off the end or the item is looking for its pred.
      if (itemId == -1 || Svc.Annos.itemHasAnnotation(itemId, PREDECESSOR_ANNO))
        break;
    } while (itemId != stopId);
  },

  /**
   * For the provided predecessor item, attach its followers to it
   */
  _attachFollowers: function BStore__attachFollowers(predId) {
    let predGUID = GUIDForId(predId);
    let followers = this._findAnnoItems(PREDECESSOR_ANNO, predGUID);
    if (followers.length > 1)
      this._log.warn(predId + " has more than one followers: " + followers);

    // Start at the first follower and move the chain of followers
    let parent = Svc.Bookmark.getFolderIdForItem(predId);
    followers.forEach(function(follow) {
      this._log.trace("Repositioning " + follow + " behind " + predId);
      if (Svc.Bookmark.getFolderIdForItem(follow) != parent) {
        this._log.warn("Follower doesn't have the same parent: " + parent);
        return;
      }

      // Move the chain of followers to after the predecessor
      let insertPos = Svc.Bookmark.getItemIndex(predId) + 1;
      this._moveItemChain(follow, insertPos, predId);

      // Remove the annotation now that we're putting it in the right spot
      Svc.Annos.removeItemAnnotation(follow, PREDECESSOR_ANNO);
    }, this);
  },

  create: function BStore_create(record) {
    let newId;
    switch (record.type) {
    case "bookmark":
    case "query":
    case "microsummary": {
      let uri = Utils.makeURI(record.bmkUri);
      newId = this._bms.insertBookmark(record._parent, uri, record._insertPos,
        record.title);
      this._log.debug(["created bookmark", newId, "under", record._parent, "at",
        record._insertPos, "as", record.title, record.bmkUri].join(" "));

      this._tagURI(uri, record.tags);
      this._bms.setKeywordForBookmark(newId, record.keyword);
      if (record.description)
        Utils.anno(newId, "bookmarkProperties/description", record.description);

      if (record.loadInSidebar)
        Utils.anno(newId, "bookmarkProperties/loadInSidebar", true);

      if (record.type == "microsummary") {
        this._log.debug("   \-> is a microsummary");
        Utils.anno(newId, "bookmarks/staticTitle", record.staticTitle || "");
        let genURI = Utils.makeURI(record.generatorUri);
	if (this._ms) {
          try {
            let micsum = this._ms.createMicrosummary(uri, genURI);
            this._ms.setMicrosummary(newId, micsum);
          }
          catch(ex) { /* ignore "missing local generator" exceptions */ }
	} else {
	  this._log.warn("Can't create microsummary -- not supported.");
	}
      }
    } break;
    case "folder":
      newId = this._bms.createFolder(record._parent, record.title,
        record._insertPos);
      this._log.debug(["created folder", newId, "under", record._parent, "at",
        record._insertPos, "as", record.title].join(" "));
      break;
    case "livemark":
      let siteURI = null;
      if (record.siteUri != null)
        siteURI = Utils.makeURI(record.siteUri);

      newId = this._ls.createLivemark(record._parent, record.title, siteURI,
        Utils.makeURI(record.feedUri), record._insertPos);
      this._log.debug(["created livemark", newId, "under", record._parent, "at",
        record._insertPos, "as", record.title, record.siteUri, record.feedUri].
        join(" "));
      break;
    case "separator":
      newId = this._bms.insertSeparator(record._parent, record._insertPos);
      this._log.debug(["created separator", newId, "under", record._parent,
        "at", record._insertPos].join(" "));
      break;
    case "item":
      this._log.debug(" -> got a generic places item.. do nothing?");
      return;
    default:
      this._log.error("_create: Unknown item type: " + record.type);
      return;
    }

    this._log.trace("Setting GUID of new item " + newId + " to " + record.id);
    this._setGUID(newId, record.id);
  },

  remove: function BStore_remove(record) {
    let itemId = idForGUID(record.id);
    if (itemId <= 0) {
      this._log.debug("Item " + record.id + " already removed");
      return;
    }
    var type = this._bms.getItemType(itemId);

    switch (type) {
    case this._bms.TYPE_BOOKMARK:
      this._log.debug("  -> removing bookmark " + record.id);
      this._ts.untagURI(this._bms.getBookmarkURI(itemId), null);
      this._bms.removeItem(itemId);
      break;
    case this._bms.TYPE_FOLDER:
      this._log.debug("  -> removing folder " + record.id);
      this._bms.removeFolder(itemId);
      break;
    case this._bms.TYPE_SEPARATOR:
      this._log.debug("  -> removing separator " + record.id);
      this._bms.removeItem(itemId);
      break;
    default:
      this._log.error("remove: Unknown item type: " + type);
      break;
    }
  },

  update: function BStore_update(record) {
    let itemId = idForGUID(record.id);

    if (itemId <= 0) {
      this._log.debug("Skipping update for unknown item: " + record.id);
      return;
    }

    this._log.trace("Updating " + record.id + " (" + itemId + ")");

    // Move the bookmark to a new parent if necessary
    if (Svc.Bookmark.getFolderIdForItem(itemId) != record._parent) {
      this._log.trace("Moving item to a new parent");
      Svc.Bookmark.moveItem(itemId, record._parent, record._insertPos);
    }
    // Move the chain of bookmarks to a new position
    else if (Svc.Bookmark.getItemIndex(itemId) != record._insertPos &&
             !record._orphan) {
      this._log.trace("Moving item and followers to a new position");

      // Stop moving at the predecessor unless we don't have one
      this._moveItemChain(itemId, record._insertPos, record._predId || itemId);
    }

    for (let [key, val] in Iterator(record.cleartext)) {
      switch (key) {
      case "title":
        this._bms.setItemTitle(itemId, val);
        break;
      case "bmkUri":
        this._bms.changeBookmarkURI(itemId, Utils.makeURI(val));
        break;
      case "tags":
        this._tagURI(this._bms.getBookmarkURI(itemId), val);
        break;
      case "keyword":
        this._bms.setKeywordForBookmark(itemId, val);
        break;
      case "description":
        Utils.anno(itemId, "bookmarkProperties/description", val);
        break;
      case "loadInSidebar":
        if (val)
          Utils.anno(itemId, "bookmarkProperties/loadInSidebar", true);
        else
          Svc.Annos.removeItemAnnotation(itemId, "bookmarkProperties/loadInSidebar");
        break;
      case "generatorUri": {
        try {
          let micsumURI = this._bms.getBookmarkURI(itemId);
          let genURI = Utils.makeURI(val);
	  if (this._ms == SERVICE_NOT_SUPPORTED) {
	    this._log.warn("Can't create microsummary -- not supported.");
	  } else {
            let micsum = this._ms.createMicrosummary(micsumURI, genURI);
            this._ms.setMicrosummary(itemId, micsum);
	  }
        } catch (e) {
          this._log.debug("Could not set microsummary generator URI: " + e);
        }
      } break;
      case "siteUri":
        this._ls.setSiteURI(itemId, Utils.makeURI(val));
        break;
      case "feedUri":
        this._ls.setFeedURI(itemId, Utils.makeURI(val));
        break;
      }
    }
  },

  changeItemID: function BStore_changeItemID(oldID, newID) {
    // Remember the GUID change for incoming records and avoid invalid refs
    this.aliases[oldID] = newID;
    this.cache.clear();

    // Update any existing annotation references
    this._findAnnoItems(PARENT_ANNO, oldID).forEach(function(itemId) {
      Utils.anno(itemId, PARENT_ANNO, "T" + newID);
    }, this);
    this._findAnnoItems(PREDECESSOR_ANNO, oldID).forEach(function(itemId) {
      Utils.anno(itemId, PREDECESSOR_ANNO, "R" + newID);
    }, this);

    // Make sure there's an item to change GUIDs
    let itemId = idForGUID(oldID);
    if (itemId <= 0)
      return;

    this._log.debug("Changing GUID " + oldID + " to " + newID);
    this._setGUID(itemId, newID);
  },

  _setGUID: function BStore__setGUID(itemId, guid) {
    let collision = idForGUID(guid);
    if (collision != -1) {
      this._log.warn("Freeing up GUID " + guid  + " used by " + collision);
      Svc.Annos.removeItemAnnotation(collision, "placesInternal/GUID");
    }
    Svc.Bookmark.setItemGUID(itemId, guid);
  },

  _getNode: function BStore__getNode(folder) {
    let query = this._hsvc.getNewQuery();
    query.setFolders([folder], 1);
    return this._hsvc.executeQuery(query, this._hsvc.getNewQueryOptions()).root;
  },

  _getTags: function BStore__getTags(uri) {
    try {
      if (typeof(uri) == "string")
        uri = Utils.makeURI(uri);
    } catch(e) {
      this._log.warn("Could not parse URI \"" + uri + "\": " + e);
    }
    return this._ts.getTagsForURI(uri, {});
  },

  _getDescription: function BStore__getDescription(id) {
    try {
      return Utils.anno(id, "bookmarkProperties/description");
    } catch (e) {
      return undefined;
    }
  },

  _isLoadInSidebar: function BStore__isLoadInSidebar(id) {
    return Svc.Annos.itemHasAnnotation(id, "bookmarkProperties/loadInSidebar");
  },

  _getStaticTitle: function BStore__getStaticTitle(id) {
    try {
      return Utils.anno(id, "bookmarks/staticTitle");
    } catch (e) {
      return "";
    }
  },

  // Create a record starting from the weave id (places guid)
  createRecord: function BStore_createRecord(guid, cryptoMetaURL) {
    let record = this.cache.get(guid);
    if (record)
      return record;

    let placeId = idForGUID(guid);
    if (placeId <= 0) { // deleted item
      record = new PlacesItem();
      record.id = guid;
      record.deleted = true;
      this.cache.put(guid, record);
      return record;
    }

    switch (this._bms.getItemType(placeId)) {
    case this._bms.TYPE_BOOKMARK:
      let bmkUri = this._bms.getBookmarkURI(placeId).spec;
      if (this._ms && this._ms.hasMicrosummary(placeId)) {
        record = new BookmarkMicsum();
        let micsum = this._ms.getMicrosummary(placeId);
        record.generatorUri = micsum.generator.uri.spec; // breaks local generators
        record.staticTitle = this._getStaticTitle(placeId);
      }
      else {
        if (bmkUri.search(/^place:/) == 0) {
          record = new BookmarkQuery();

          // Get the actual tag name instead of the local itemId
          let folder = bmkUri.match(/[:&]folder=(\d+)/);
          try {
            // There might not be the tag yet when creating on a new client
            if (folder != null) {
              folder = folder[1];
              record.folderName = this._bms.getItemTitle(folder);
              this._log.debug("query id: " + folder + " = " + record.folderName);
            }
          }
          catch(ex) {}
        }
        else
          record = new Bookmark();
        record.title = this._bms.getItemTitle(placeId);
      }

      record.bmkUri = bmkUri;
      record.tags = this._getTags(record.bmkUri);
      record.keyword = this._bms.getKeywordForBookmark(placeId);
      record.description = this._getDescription(placeId);
      record.loadInSidebar = this._isLoadInSidebar(placeId);
      break;

    case this._bms.TYPE_FOLDER:
      if (this._ls.isLivemark(placeId)) {
        record = new Livemark();

        let siteURI = this._ls.getSiteURI(placeId);
        if (siteURI != null)
          record.siteUri = siteURI.spec;
        record.feedUri = this._ls.getFeedURI(placeId).spec;

      } else {
        record = new BookmarkFolder();
      }

      record.title = this._bms.getItemTitle(placeId);
      break;

    case this._bms.TYPE_SEPARATOR:
      record = new BookmarkSeparator();
      // Create a positioning identifier for the separator
      let parent = Svc.Bookmark.getFolderIdForItem(placeId);
      record.pos = [placeId, parent].map(Svc.Bookmark.getItemIndex);
      break;

    case this._bms.TYPE_DYNAMIC_CONTAINER:
      record = new PlacesItem();
      this._log.warn("Don't know how to serialize dynamic containers yet");
      break;

    default:
      record = new PlacesItem();
      this._log.warn("Unknown item type, cannot serialize: " +
                     this._bms.getItemType(placeId));
    }

    record.id = guid;
    record.parentid = this._getParentGUIDForId(placeId);
    record.predecessorid = this._getPredecessorGUIDForId(placeId);
    record.encryption = cryptoMetaURL;
    record.sortindex = this._calculateIndex(record);

    this.cache.put(guid, record);
    return record;
  },

  get _frecencyStm() {
    this._log.trace("Creating SQL statement: _frecencyStm");
    let stm = Svc.History.DBConnection.createStatement(
      "SELECT frecency " +
      "FROM moz_places_view " +
      "WHERE url = :url");
    this.__defineGetter__("_frecencyStm", function() stm);
    return stm;
  },

  _calculateIndex: function _calculateIndex(record) {
    // For anything directly under the toolbar, give it a boost of more than an
    // unvisited bookmark
    let index = 0;
    if (record.parentid == "toolbar")
      index += 150;

    // Add in the bookmark's frecency if we have something
    if (record.bmkUri != null) {
      try {
        this._frecencyStm.params.url = record.bmkUri;
        if (this._frecencyStm.step())
          index += this._frecencyStm.row.frecency;
      }
      finally {
        this._frecencyStm.reset();
      }
    }

    this._log.info("calculating index for: " + record.title + " = " + index);
    return index;
  },

  _getParentGUIDForId: function BStore__getParentGUIDForId(itemId) {
    // Give the parent annotation if it exists
    try {
      // XXX Work around Bug 510628 by removing prepended parenT
      return Utils.anno(itemId, PARENT_ANNO).slice(1);
    }
    catch(ex) {}

    let parentid = this._bms.getFolderIdForItem(itemId);
    if (parentid == -1) {
      this._log.debug("Found orphan bookmark, reparenting to unfiled");
      parentid = this._bms.unfiledBookmarksFolder;
      this._bms.moveItem(itemId, parentid, -1);
    }
    return GUIDForId(parentid);
  },

  _getPredecessorGUIDForId: function BStore__getPredecessorGUIDForId(itemId) {
    // Give the predecessor annotation if it exists
    try {
      // XXX Work around Bug 510628 by removing prepended predecessoR
      return Utils.anno(itemId, PREDECESSOR_ANNO).slice(1);
    }
    catch(ex) {}

    // Figure out the predecessor, unless it's the first item
    let itemPos = Svc.Bookmark.getItemIndex(itemId);
    if (itemPos == 0)
      return;

    // For items directly under unfiled/unsorted, give no predecessor
    let parentId = Svc.Bookmark.getFolderIdForItem(itemId);
    if (parentId == Svc.Bookmark.unfiledBookmarksFolder)
      return;

    let predecessorId = Svc.Bookmark.getIdForItemAt(parentId, itemPos - 1);
    if (predecessorId == -1) {
      this._log.debug("No predecessor directly before " + itemId + " under " +
        parentId + " at " + itemPos);

      // Find the predecessor before the item
      do {
        // No more items to check, it must be the first one
        if (--itemPos < 0)
          break;
        predecessorId = Svc.Bookmark.getIdForItemAt(parentId, itemPos);
      } while (predecessorId == -1);

      // Fix up the item to be at the right position for next time
      itemPos++;
      this._log.debug("Fixing " + itemId + " to be at position " + itemPos);
      Svc.Bookmark.moveItem(itemId, parentId, itemPos);

      // There must be no predecessor for this item!
      if (itemPos == 0)
        return;
    }

    return GUIDForId(predecessorId);
  },

  _getChildren: function BStore_getChildren(guid, items) {
    let node = guid; // the recursion case
    if (typeof(node) == "string") // callers will give us the guid as the first arg
      node = this._getNode(idForGUID(guid));

    if (node.type == node.RESULT_TYPE_FOLDER &&
        !this._ls.isLivemark(node.itemId)) {
      node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      node.containerOpen = true;

      // Remember all the children GUIDs and recursively get more
      for (var i = 0; i < node.childCount; i++) {
        let child = node.getChild(i);
        items[GUIDForId(child.itemId)] = true;
        this._getChildren(child, items);
      }
    }

    return items;
  },

  _tagURI: function BStore_tagURI(bmkURI, tags) {
    // Filter out any null/undefined/empty tags
    tags = tags.filter(function(t) t);

    // Temporarily tag a dummy uri to preserve tag ids when untagging
    let dummyURI = Utils.makeURI("about:weave#BStore_tagURI");
    this._ts.tagURI(dummyURI, tags);
    this._ts.untagURI(bmkURI, null);
    this._ts.tagURI(bmkURI, tags);
    this._ts.untagURI(dummyURI, null);
  },

  getAllIDs: function BStore_getAllIDs() {
    let items = {};
    for (let [guid, id] in Iterator(kSpecialIds))
      if (guid != "places" && guid != "tags")
        this._getChildren(guid, items);
    return items;
  },

  wipe: function BStore_wipe() {
    for (let [guid, id] in Iterator(kSpecialIds))
      if (guid != "places")
        this._bms.removeFolderChildren(id);
  }
};

function BookmarksTracker() {
  this._init();
}
BookmarksTracker.prototype = {
  __proto__: Tracker.prototype,
  name: "bookmarks",
  _logName: "BmkTracker",
  file: "bookmarks",

  get _bms() {
    let bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
      getService(Ci.nsINavBookmarksService);
    this.__defineGetter__("_bms", function() bms);
    return bms;
  },

  get _ls() {
    let ls = Cc["@mozilla.org/browser/livemark-service;2"].
      getService(Ci.nsILivemarkService);
    this.__defineGetter__("_ls", function() ls);
    return ls;
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavBookmarkObserver,
    Ci.nsINavBookmarkObserver_MOZILLA_1_9_1_ADDITIONS
  ]),

  _init: function BMT__init() {
    this.__proto__.__proto__._init.call(this);

    // Ignore changes to the special roots
    for (let guid in kSpecialIds)
      this.ignoreID(guid);

    this._bms.addObserver(this, false);
  },

  /**
   * Add a bookmark (places) id to be uploaded and bump up the sync score
   *
   * @param itemId
   *        Places internal id of the bookmark to upload
   */
  _addId: function BMT__addId(itemId) {
    if (this.addChangedID(GUIDForId(itemId)))
      this._upScore();
  },

  /**
   * Add the successor id for the item that follows the given item
   */
  _addSuccessor: function BMT__addSuccessor(itemId) {
    let parentId = Svc.Bookmark.getFolderIdForItem(itemId);
    let itemPos = Svc.Bookmark.getItemIndex(itemId);
    let succId = Svc.Bookmark.getIdForItemAt(parentId, itemPos + 1);
    if (succId != -1)
      this._addId(succId);
  },

  /* Every add/remove/change is worth 10 points */
  _upScore: function BMT__upScore() {
    this._score += 10;
  },

  /**
   * Determine if a change should be ignored: we're ignoring everything or the
   * folder is for livemarks
   *
   * @param itemId
   *        Item under consideration to ignore
   * @param folder (optional)
   *        Folder of the item being changed
   */
  _ignore: function BMT__ignore(itemId, folder) {
    // Ignore unconditionally if the engine tells us to
    if (this.ignoreAll)
      return true;

    // Get the folder id if we weren't given one
    if (folder == null)
      folder = this._bms.getFolderIdForItem(itemId);

    let tags = kSpecialIds.tags;
    // Ignore changes to tags (folders under the tags folder)
    if (folder == tags)
      return true;

    // Ignore tag items (the actual instance of a tag for a bookmark)
    if (this._bms.getFolderIdForItem(folder) == tags)
      return true;

    // Ignore livemark children
    return this._ls.isLivemark(folder);
  },

  onItemAdded: function BMT_onEndUpdateBatch(itemId, folder, index) {
    if (this._ignore(itemId, folder))
      return;

    this._log.trace("onItemAdded: " + itemId);
    this._addId(itemId);
    this._addSuccessor(itemId);
  },

  onBeforeItemRemoved: function BMT_onBeforeItemRemoved(itemId) {
    if (this._ignore(itemId))
      return;

    this._log.trace("onBeforeItemRemoved: " + itemId);
    this._addId(itemId);
    this._addSuccessor(itemId);
  },

  onItemChanged: function BMT_onItemChanged(itemId, property, isAnno, value) {
    if (this._ignore(itemId))
      return;

    // Make sure to remove items that now have the exclude annotation
    if (property == "places/excludeFromBackup") {
      this.removeChangedID(GUIDForId(itemId));
      return;
    }

    // ignore annotations except for the ones that we sync
    let annos = ["bookmarkProperties/description",
      "bookmarkProperties/loadInSidebar", "bookmarks/staticTitle",
      "livemark/feedURI", "livemark/siteURI", "microsummary/generatorURI"];
    if (isAnno && annos.indexOf(property) == -1)
      return;

    this._log.trace("onItemChanged: " + itemId +
                    (", " + property + (isAnno? " (anno)" : "")) +
                    (value? (" = \"" + value + "\"") : ""));
    this._addId(itemId);
  },

  onItemMoved: function BMT_onItemMoved(itemId, oldParent, oldIndex, newParent, newIndex) {
    if (this._ignore(itemId))
      return;

    this._log.trace("onItemMoved: " + itemId);
    this._addId(itemId);
    this._addSuccessor(itemId);

    // Get the thing that's now at the old place
    let oldSucc = Svc.Bookmark.getIdForItemAt(oldParent, oldIndex);
    if (oldSucc != -1)
      this._addId(oldSucc);
  },

  onBeginUpdateBatch: function BMT_onBeginUpdateBatch() {},
  onEndUpdateBatch: function BMT_onEndUpdateBatch() {},
  onItemRemoved: function BMT_onItemRemoved(itemId, folder, index) {},
  onItemVisited: function BMT_onItemVisited(itemId, aVisitID, time) {}
};
