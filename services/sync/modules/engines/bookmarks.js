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
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mills <thunder@mozilla.com>
 *   Jono DiCarlo <jdicarlo@mozilla.org>
 *   Anant Narayanan <anant@kix.in>
 *   Philipp von Weitershausen <philipp@weitershausen.de>
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

const GUID_ANNO = "sync/guid";
const PARENT_ANNO = "sync/parent";
const SERVICE_NOT_SUPPORTED = "Service not supported on this platform";
const FOLDER_SORTINDEX = 1000000;

try {
  Cu.import("resource://gre/modules/PlacesUtils.jsm");
}
catch(ex) {
  Cu.import("resource://gre/modules/utils.js");
}
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/stores.js");
Cu.import("resource://services-sync/trackers.js");
Cu.import("resource://services-sync/type_records/bookmark.js");
Cu.import("resource://services-sync/util.js");

function archiveBookmarks() {
  // Some nightly builds of 3.7 don't have this function
  try {
    PlacesUtils.archiveBookmarksFile(null, true);
  }
  catch(ex) {}
}

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
Utils.lazy2(kSpecialIds, "mobile", function() {
  // Use the (one) mobile root if it already exists
  let anno = "mobile/bookmarksRoot";
  let root = Svc.Annos.getItemsWithAnnotation(anno, {});
  if (root.length != 0)
    return root[0];

  // Create the special mobile folder to store mobile bookmarks
  let mobile = Svc.Bookmark.createFolder(Svc.Bookmark.placesRoot, "mobile", -1);
  Utils.anno(mobile, anno, 1);
  return mobile;
});

function BookmarksEngine() {
  SyncEngine.call(this, "Bookmarks");
  this._handleImport();
}
BookmarksEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _recordObj: PlacesItem,
  _storeObj: BookmarksStore,
  _trackerObj: BookmarksTracker,
  version: 2,

  _handleImport: function _handleImport() {
    Svc.Obs.add("bookmarks-restore-begin", function() {
      this._log.debug("Ignoring changes from importing bookmarks");
      this._tracker.ignoreAll = true;
    }, this);

    Svc.Obs.add("bookmarks-restore-success", function() {
      this._log.debug("Tracking all items on successful import");
      this._tracker.ignoreAll = false;

      // Mark all the items as changed so they get uploaded
      for (let id in this._store.getAllIDs())
        this._tracker.addChangedID(id);
    }, this);

    Svc.Obs.add("bookmarks-restore-failed", function() {
      this._tracker.ignoreAll = false;
    }, this);
  },

  _sync: Utils.batchSync("Bookmark", SyncEngine),

  _syncStartup: function _syncStart() {
    SyncEngine.prototype._syncStartup.call(this);

    // For first-syncs, make a backup for the user to restore
    if (this.lastSync == 0)
      archiveBookmarks();

    // Lazily create a mapping of folder titles and separator positions to GUID
    this.__defineGetter__("_lazyMap", function() {
      delete this._lazyMap;

      let lazyMap = {};
      for (let guid in this._store.getAllIDs()) {
        // Figure out what key to store the mapping
        let key;
        let id = this._store.idForGUID(guid);
        switch (Svc.Bookmark.getItemType(id)) {
          case Svc.Bookmark.TYPE_BOOKMARK:
            key = "b" + Svc.Bookmark.getBookmarkURI(id).spec + ":" +
              Svc.Bookmark.getItemTitle(id);
            break;
          case Svc.Bookmark.TYPE_FOLDER:
            key = "f" + Svc.Bookmark.getItemTitle(id);
            break;
          case Svc.Bookmark.TYPE_SEPARATOR:
            key = "s" + Svc.Bookmark.getItemIndex(id);
            break;
          default:
            continue;
        }

        // The mapping is on a per parent-folder-name basis
        let parent = Svc.Bookmark.getFolderIdForItem(id);
        if (parent <= 0)
          continue;

        let parentName = Svc.Bookmark.getItemTitle(parent);
        if (lazyMap[parentName] == null)
          lazyMap[parentName] = {};

        // If the entry already exists, remember that there are explicit dupes
        let entry = new String(guid);
        entry.hasDupe = lazyMap[parentName][key] != null;

        // Remember this item's guid for its parent-name/key pair
        lazyMap[parentName][key] = entry;
        this._log.trace("Mapped: " + [parentName, key, entry, entry.hasDupe]);
      }

      // Expose a helper function to get a dupe guid for an item
      return this._lazyMap = function(item) {
        // Figure out if we have something to key with
        let key;
        switch (item.type) {
          case "bookmark":
          case "query":
          case "microsummary":
            key = "b" + item.bmkUri + ":" + item.title;
            break;
          case "folder":
          case "livemark":
            key = "f" + item.title;
            break;
          case "separator":
            key = "s" + item.pos;
            break;
          default:
            return;
        }

        // Give the guid if we have the matching pair
        this._log.trace("Finding mapping: " + item.parentName + ", " + key);
        let parent = lazyMap[item.parentName];
        let dupe = parent && parent[key];
        this._log.trace("Mapped dupe: " + dupe);
        return dupe;
      };
    });

    this._store._childrenToOrder = {};
  },

  _processIncoming: function _processIncoming() {
    try {
      SyncEngine.prototype._processIncoming.call(this);
    } finally {
      // Reorder children.
      this._tracker.ignoreAll = true;
      this._store._orderChildren();
      this._tracker.ignoreAll = false;
      delete this._store._childrenToOrder;
    }
  },

  _syncFinish: function _syncFinish() {
    SyncEngine.prototype._syncFinish.call(this);
    delete this._lazyMap;
    this._tracker._ensureMobileQuery();
  },

  _createRecord: function _createRecord(id) {
    // Create the record like normal but mark it as having dupes if necessary
    let record = SyncEngine.prototype._createRecord.call(this, id);
    let entry = this._lazyMap(record);
    if (entry != null && entry.hasDupe)
      record.hasDupe = true;
    return record;
  },

  _findDupe: function _findDupe(item) {
    // Don't bother finding a dupe if the incoming item has duplicates
    if (item.hasDupe)
      return;
    return this._lazyMap(item);
  },

  _handleDupe: function _handleDupe(item, dupeId) {
    // Always change the local GUID to the incoming one.
    this._store.changeItemID(dupeId, item.id);
    this._deleteId(dupeId);
    this._tracker.addChangedID(item.id, 0);
    if (item.parentid) {
      this._tracker.addChangedID(item.parentid, 0);
    }
  }
};

function BookmarksStore(name) {
  Store.call(this, name);

  // Explicitly nullify our references to our cached services so we don't leak
  Svc.Obs.add("places-shutdown", function() {
    this.__bms = null;
    this.__hsvc = null;
    this.__ls = null;
    this.__ms = null;
    this.__ts = null;
    for each ([query, stmt] in Iterator(this._stmts))
      stmt.finalize();
  }, this);
}
BookmarksStore.prototype = {
  __proto__: Store.prototype,

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
                    getService(Ci.nsINavHistoryService).
                    QueryInterface(Ci.nsPIPlacesDatabase);
    return this.__hsvc;
  },

  __ls: null,
  get _ls() {
    if (!this.__ls)
      this.__ls = Cc["@mozilla.org/browser/livemark-service;2"].
                  getService(Ci.nsILivemarkService);
    return this.__ls;
  },

  __ms: null,
  get _ms() {
    if (!this.__ms) {
      try {
        this.__ms = Cc["@mozilla.org/microsummary/service;1"].
                    getService(Ci.nsIMicrosummaryService);
      } catch (e) {
        this._log.warn("Could not load microsummary service");
        this._log.debug(e);
        // Redefine our getter so we won't keep trying to get the service
        this.__defineGetter__("_ms", function() null);
      }
    }
    return this.__ms;
  },

  __ts: null,
  get _ts() {
    if (!this.__ts)
      this.__ts = Cc["@mozilla.org/browser/tagging-service;1"].
                  getService(Ci.nsITaggingService);
    return this.__ts;
  },


  itemExists: function BStore_itemExists(id) {
    return this.idForGUID(id) > 0;
  },

  applyIncoming: function BStore_applyIncoming(record) {
    // For special folders we're only interested in child ordering.
    if ((record.id in kSpecialIds) && record.children) {
      this._log.debug("Processing special node: " + record.id);
      // Reorder children later
      this._childrenToOrder[record.id] = record.children;
      return;
    }

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
      let parentId = this.idForGUID(parentGUID);

      // Default to unfiled if we don't have the parent yet
      if (parentId <= 0) {
        this._log.trace("Reparenting to unfiled until parent is synced");
        record._orphan = true;
        parentId = kSpecialIds.unfiled;
      }

      // Save the parent id for modifying the bookmark later
      record._parent = parentId;
    }

    // Do the normal processing of incoming records
    Store.prototype.applyIncoming.apply(this, arguments);

    // Do some post-processing if we have an item
    let itemId = this.idForGUID(record.id);
    if (itemId > 0) {
      // Move any children that are looking for this folder as a parent
      if (record.type == "folder") {
        this._reparentOrphans(itemId);
        // Reorder children later
        if (record.children)
          this._childrenToOrder[record.id] = record.children;
      }

      // Create an annotation to remember that it needs a parent
      if (record._orphan)
        Utils.anno(itemId, PARENT_ANNO, parentGUID);
    }
  },

  /**
   * Find all ids of items that have a given value for an annotation
   */
  _findAnnoItems: function BStore__findAnnoItems(anno, val) {
    return Svc.Annos.getItemsWithAnnotation(anno, {}).filter(function(id)
      Utils.anno(id, anno) == val);
  },

  /**
   * For the provided parent item, attach its children to it
   */
  _reparentOrphans: function _reparentOrphans(parentId) {
    // Find orphans and reunite with this folder parent
    let parentGUID = this.GUIDForId(parentId);
    let orphans = this._findAnnoItems(PARENT_ANNO, parentGUID);

    this._log.debug("Reparenting orphans " + orphans + " to " + parentId);
    orphans.forEach(function(orphan) {
      // Move the orphan to the parent and drop the missing parent annotation
      Svc.Bookmark.moveItem(orphan, parentId, Svc.Bookmark.DEFAULT_INDEX);
      Svc.Annos.removeItemAnnotation(orphan, PARENT_ANNO);
    }, this);
  },

  create: function BStore_create(record) {
    let newId;
    switch (record.type) {
    case "bookmark":
    case "query":
    case "microsummary": {
      let uri = Utils.makeURI(record.bmkUri);
      newId = this._bms.insertBookmark(record._parent, uri,
                                       Svc.Bookmark.DEFAULT_INDEX, record.title);
      this._log.debug(["created bookmark", newId, "under", record._parent,
                       "as", record.title, record.bmkUri].join(" "));

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
        }
        else
          this._log.warn("Can't create microsummary -- not supported.");
      }
    } break;
    case "folder":
      newId = this._bms.createFolder(record._parent, record.title,
                                     Svc.Bookmark.DEFAULT_INDEX);
      this._log.debug(["created folder", newId, "under", record._parent,
                       "as", record.title].join(" "));

      if (record.description)
        Utils.anno(newId, "bookmarkProperties/description", record.description);

      // record.children will be dealt with in _orderChildren.
      break;
    case "livemark":
      let siteURI = null;
      if (record.siteUri != null)
        siteURI = Utils.makeURI(record.siteUri);

      newId = this._ls.createLivemark(record._parent, record.title, siteURI,
                                      Utils.makeURI(record.feedUri),
                                      Svc.Bookmark.DEFAULT_INDEX);
      this._log.debug(["created livemark", newId, "under", record._parent, "as",
                       record.title, record.siteUri, record.feedUri].join(" "));
      break;
    case "separator":
      newId = this._bms.insertSeparator(record._parent,
                                        Svc.Bookmark.DEFAULT_INDEX);
      this._log.debug(["created separator", newId, "under", record._parent]
                      .join(" "));
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
    let itemId = this.idForGUID(record.id);
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
      Svc.Bookmark.removeItem(itemId);
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
    let itemId = this.idForGUID(record.id);

    if (itemId <= 0) {
      this._log.debug("Skipping update for unknown item: " + record.id);
      return;
    }

    this._log.trace("Updating " + record.id + " (" + itemId + ")");

    // Move the bookmark to a new parent or new position if necessary
    if (Svc.Bookmark.getFolderIdForItem(itemId) != record._parent) {
      this._log.trace("Moving item to a new parent.");
      Svc.Bookmark.moveItem(itemId, record._parent, Svc.Bookmark.DEFAULT_INDEX);
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
          if (this._ms == SERVICE_NOT_SUPPORTED)
            this._log.warn("Can't create microsummary -- not supported.");
          else {
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

  _orderChildren: function _orderChildren() {
    for (let [guid, children] in Iterator(this._childrenToOrder)) {
      // Reorder children according to the GUID list. Gracefully deal
      // with missing items, e.g. locally deleted.
      let delta = 0;
      for (let idx = 0; idx < children.length; idx++) {
        let itemid = this.idForGUID(children[idx]);
        if (itemid == -1) {
          delta += 1;
          this._log.trace("Could not locate record " + children[idx]);
          continue;
        }
        try {
          Svc.Bookmark.setItemIndex(itemid, idx - delta);
        } catch (ex) {
          this._log.debug("Could not move item " + children[idx] + ": " + ex);
        }
      }
    }
  },

  changeItemID: function BStore_changeItemID(oldID, newID) {
    this._log.debug("Changing GUID " + oldID + " to " + newID);

    // Make sure there's an item to change GUIDs
    let itemId = this.idForGUID(oldID);
    if (itemId <= 0)
      return;

    this._setGUID(itemId, newID);
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

  _getChildGUIDsForId: function _getChildGUIDsForId(itemid) {
    let node = node = this._getNode(itemid);
    let childids = [];

    if (node.type == node.RESULT_TYPE_FOLDER &&
        !this._ls.isLivemark(node.itemId)) {
      node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      node.containerOpen = true;
      for (var i = 0; i < node.childCount; i++)
        childids.push(node.getChild(i).itemId);
    }
    return childids.map(this.GUIDForId, this);
  },

  // Create a record starting from the weave id (places guid)
  createRecord: function createRecord(id, collection) {
    let placeId = this.idForGUID(id);
    let record;
    if (placeId <= 0) { // deleted item
      record = new PlacesItem(collection, id);
      record.deleted = true;
      return record;
    }

    let parent = Svc.Bookmark.getFolderIdForItem(placeId);
    switch (this._bms.getItemType(placeId)) {
    case this._bms.TYPE_BOOKMARK:
      let bmkUri = this._bms.getBookmarkURI(placeId).spec;
      if (this._ms && this._ms.hasMicrosummary(placeId)) {
        record = new BookmarkMicsum(collection, id);
        let micsum = this._ms.getMicrosummary(placeId);
        record.generatorUri = micsum.generator.uri.spec; // breaks local generators
        record.staticTitle = this._getStaticTitle(placeId);
      }
      else {
        if (bmkUri.search(/^place:/) == 0) {
          record = new BookmarkQuery(collection, id);

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
          record = new Bookmark(collection, id);
        record.title = this._bms.getItemTitle(placeId);
      }

      record.parentName = Svc.Bookmark.getItemTitle(parent);
      record.bmkUri = bmkUri;
      record.tags = this._getTags(record.bmkUri);
      record.keyword = this._bms.getKeywordForBookmark(placeId);
      record.description = this._getDescription(placeId);
      record.loadInSidebar = this._isLoadInSidebar(placeId);
      break;

    case this._bms.TYPE_FOLDER:
      if (this._ls.isLivemark(placeId)) {
        record = new Livemark(collection, id);

        let siteURI = this._ls.getSiteURI(placeId);
        if (siteURI != null)
          record.siteUri = siteURI.spec;
        record.feedUri = this._ls.getFeedURI(placeId).spec;

      } else {
        record = new BookmarkFolder(collection, id);
      }

      if (parent > 0)
        record.parentName = Svc.Bookmark.getItemTitle(parent);
      record.title = this._bms.getItemTitle(placeId);
      record.description = this._getDescription(placeId);
      record.children = this._getChildGUIDsForId(placeId);
      break;

    case this._bms.TYPE_SEPARATOR:
      record = new BookmarkSeparator(collection, id);
      if (parent > 0)
        record.parentName = Svc.Bookmark.getItemTitle(parent);
      // Create a positioning identifier for the separator, used by _lazyMap
      record.pos = Svc.Bookmark.getItemIndex(placeId);
      break;

    case this._bms.TYPE_DYNAMIC_CONTAINER:
      record = new PlacesItem(collection, id);
      this._log.warn("Don't know how to serialize dynamic containers yet");
      break;

    default:
      record = new PlacesItem(collection, id);
      this._log.warn("Unknown item type, cannot serialize: " +
                     this._bms.getItemType(placeId));
    }

    record.parentid = this.GUIDForId(parent);
    record.sortindex = this._calculateIndex(record);

    return record;
  },

  _stmts: {},
  _getStmt: function(query) {
    if (query in this._stmts)
      return this._stmts[query];

    this._log.trace("Creating SQL statement: " + query);
    return this._stmts[query] = Utils.createStatement(this._hsvc.DBConnection,
                                                      query);
  },

  __haveGUIDColumn: null,
  get _haveGUIDColumn() {
    if (this.__haveGUIDColumn !== null) {
      return this.__haveGUIDColumn;
    }
    let stmt;
    try {
      stmt = this._hsvc.DBConnection.createStatement(
        "SELECT guid FROM moz_places");
      stmt.finalize();
      return this.__haveGUIDColumn = true;
    } catch(ex) {
      return this.__haveGUIDColumn = false;
    }
  },

  get _frecencyStm() {
    return this._getStmt(
        "SELECT frecency " +
        "FROM moz_places " +
        "WHERE url = :url " +
        "LIMIT 1");
  },

  get _addGUIDAnnotationNameStm() {
    let stmt = this._getStmt(
      "INSERT OR IGNORE INTO moz_anno_attributes (name) VALUES (:anno_name)");
    stmt.params.anno_name = GUID_ANNO;
    return stmt;
  },

  get _checkGUIDItemAnnotationStm() {
    // Gecko <2.0 only
    let stmt = this._getStmt(
      "SELECT b.id AS item_id, " +
        "(SELECT id FROM moz_anno_attributes WHERE name = :anno_name) AS name_id, " +
        "a.id AS anno_id, a.dateAdded AS anno_date " +
      "FROM moz_bookmarks b " +
      "LEFT JOIN moz_items_annos a ON a.item_id = b.id " +
                                 "AND a.anno_attribute_id = name_id " +
      "WHERE b.id = :item_id");
    stmt.params.anno_name = GUID_ANNO;
    return stmt;
  },

  get _addItemAnnotationStm() {
    return this._getStmt(
    "INSERT OR REPLACE INTO moz_items_annos " +
      "(id, item_id, anno_attribute_id, mime_type, content, flags, " +
       "expiration, type, dateAdded, lastModified) " +
    "VALUES (:id, :item_id, :name_id, :mime_type, :content, :flags, " +
            ":expiration, :type, :date_added, :last_modified)");
  },

  __setGUIDStm: null,
  get _setGUIDStm() {
    if (this.__setGUIDStm !== null) {
      return this.__setGUIDStm;
    }

    // Obtains a statement to set the guid iff the guid column exists.
    let stmt;
    if (this._haveGUIDColumn) {
      stmt = this._getStmt(
        "UPDATE moz_bookmarks " +
        "SET guid = :guid " +
        "WHERE id = :item_id");
    } else {
      stmt = false;
    }
    return this.__setGUIDStm = stmt;
  },

  // Some helper functions to handle GUIDs
  _setGUID: function _setGUID(id, guid) {
    if (arguments.length == 1)
      guid = Utils.makeGUID();

    // If we can, set the GUID on moz_bookmarks and do not do any other work.
    let (stmt = this._setGUIDStm) {
      if (stmt) {
        stmt.params.guid = guid;
        stmt.params.item_id = id;
        Utils.queryAsync(stmt);
        return guid;
      }
    }

    // Ensure annotation name exists
    Utils.queryAsync(this._addGUIDAnnotationNameStm);

    let stmt = this._checkGUIDItemAnnotationStm;
    stmt.params.item_id = id;
    let result = Utils.queryAsync(stmt, ["item_id", "name_id", "anno_id",
                                         "anno_date"])[0];
    if (!result) {
      this._log.warn("Couldn't annotate bookmark id " + id);
      return guid;
    }

    stmt = this._addItemAnnotationStm;
    if (result.anno_id) {
      stmt.params.id = result.anno_id;
      stmt.params.date_added = result.anno_date;
    } else {
      stmt.params.id = null;
      stmt.params.date_added = Date.now() * 1000;
    }
    stmt.params.item_id = result.item_id;
    stmt.params.name_id = result.name_id;
    stmt.params.content = guid;
    stmt.params.flags = 0;
    stmt.params.expiration = Ci.nsIAnnotationService.EXPIRE_NEVER;
    stmt.params.type = Ci.nsIAnnotationService.TYPE_STRING;
    stmt.params.last_modified = Date.now() * 1000;
    Utils.queryAsync(stmt);

    return guid;
  },

  __guidForIdStm: null,
  get _guidForIdStm() {
    if (this.__guidForIdStm) {
      return this.__guidForIdStm;
    }

    // Try to first read from moz_bookmarks.  Creating the statement will
    // fail, however, if the guid column does not exist.  We fallback to just
    // reading the annotation table in this case.
    let stmt;
    if (this._haveGUIDColumn) {
      stmt = this._getStmt(
        "SELECT guid " +
        "FROM moz_bookmarks " +
        "WHERE id = :item_id");
    } else {
      stmt = this._getStmt(
        "SELECT a.content AS guid " +
        "FROM moz_items_annos a " +
        "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id " +
        "JOIN moz_bookmarks b ON b.id = a.item_id " +
        "WHERE n.name = '" + GUID_ANNO + "' " +
        "AND b.id = :item_id");
    }

    return this.__guidForIdStm = stmt;
  },

  GUIDForId: function GUIDForId(id) {
    for (let [guid, specialId] in Iterator(kSpecialIds))
      if (id == specialId)
         return guid;

    let stmt = this._guidForIdStm;
    stmt.params.item_id = id;

    // Use the existing GUID if it exists
    let result = Utils.queryAsync(stmt, ["guid"])[0];
    if (result && result.guid)
      return result.guid;

    // Give the uri a GUID if it doesn't have one
    return this._setGUID(id);
  },

  __idForGUIDStm: null,
  get _idForGUIDStm() {
    if (this.__idForGUIDStm) {
      return this.__idForGUIDStm;
    }


    // Try to first read from moz_bookmarks.  Creating the statement will
    // fail, however, if the guid column does not exist.  We fallback to just
    // reading the annotation table in this case.
    let stmt;
    if (this._haveGUIDColumn) {
      stmt = this._getStmt(
        "SELECT id AS item_id " +
        "FROM moz_bookmarks " +
        "WHERE guid = :guid");
    } else {
      stmt = this._getStmt(
        "SELECT a.item_id AS item_id " +
        "FROM moz_items_annos a " +
        "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id " +
        "WHERE n.name = '" + GUID_ANNO + "' " +
        "AND a.content = :guid");
    }

    return this.__idForGUIDStm = stmt;
  },

  idForGUID: function idForGUID(guid) {
    if (guid in kSpecialIds)
      return kSpecialIds[guid];

    let stmt = this._idForGUIDStm;
    // guid might be a String object rather than a string.
    stmt.params.guid = guid.toString();

    let result = Utils.queryAsync(stmt, ["item_id"])[0];
    if (result)
      return result.item_id;
    return -1;
  },

  _calculateIndex: function _calculateIndex(record) {
    // Ensure folders have a very high sort index so they're not synced last.
    if (record.type == "folder")
      return FOLDER_SORTINDEX;

    // For anything directly under the toolbar, give it a boost of more than an
    // unvisited bookmark
    let index = 0;
    if (record.parentid == "toolbar")
      index += 150;

    // Add in the bookmark's frecency if we have something
    if (record.bmkUri != null) {
      this._frecencyStm.params.url = record.bmkUri;
      let result = Utils.queryAsync(this._frecencyStm, ["frecency"]);
      if (result.length)
        index += result[0].frecency;
    }

    return index;
  },

  _getChildren: function BStore_getChildren(guid, items) {
    let node = guid; // the recursion case
    if (typeof(node) == "string") // callers will give us the guid as the first arg
      node = this._getNode(this.idForGUID(guid));

    if (node.type == node.RESULT_TYPE_FOLDER &&
        !this._ls.isLivemark(node.itemId)) {
      node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      node.containerOpen = true;

      // Remember all the children GUIDs and recursively get more
      for (var i = 0; i < node.childCount; i++) {
        let child = node.getChild(i);
        items[this.GUIDForId(child.itemId)] = true;
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
    let items = {"menu": true,
                 "toolbar": true};
    for (let [guid, id] in Iterator(kSpecialIds))
      if (guid != "places" && guid != "tags")
        this._getChildren(guid, items);
    return items;
  },

  wipe: function BStore_wipe() {
    // Save a backup before clearing out all bookmarks
    archiveBookmarks();

    for (let [guid, id] in Iterator(kSpecialIds))
      if (guid != "places")
        this._bms.removeFolderChildren(id);
  }
};

function BookmarksTracker(name) {
  Tracker.call(this, name);

  Svc.Obs.add("places-shutdown", this);
  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);
}
BookmarksTracker.prototype = {
  __proto__: Tracker.prototype,

  _enabled: false,
  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "weave:engine:start-tracking":
        if (!this._enabled) {
          Svc.Bookmark.addObserver(this, true);
          this._enabled = true;
        }
        break;
      case "weave:engine:stop-tracking":
        if (this._enabled) {
          Svc.Bookmark.removeObserver(this);
          this._enabled = false;
        }
        // Fall through to clean up.
      case "places-shutdown":
        // Explicitly nullify our references to our cached services so
        // we don't leak
        this.__ls = null;
        this.__bms = null;
        break;
    }
  },

  __bms: null,
  get _bms() {
    if (!this.__bms)
      this.__bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                   getService(Ci.nsINavBookmarksService);
    return this.__bms;
  },

  __ls: null,
  get _ls() {
    if (!this.__ls)
      this.__ls = Cc["@mozilla.org/browser/livemark-service;2"].
                  getService(Ci.nsILivemarkService);
    return this.__ls;
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavBookmarkObserver,
    Ci.nsINavBookmarkObserver_MOZILLA_1_9_1_ADDITIONS,
    Ci.nsISupportsWeakReference
  ]),

  _GUIDForId: function _GUIDForId(item_id) {
    // Isn't indirection fun...
    return Engines.get("bookmarks")._store.GUIDForId(item_id);
  },

  /**
   * Add a bookmark (places) id to be uploaded and bump up the sync score
   *
   * @param itemId
   *        Places internal id of the bookmark to upload
   */
  _addId: function BMT__addId(itemId) {
    if (this.addChangedID(this._GUIDForId(itemId, true)))
      this._upScore();
  },

  /* Every add/remove/change is worth 10 points */
  _upScore: function BMT__upScore() {
    this.score += 10;
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

    // Ensure that the mobile bookmarks query is correct in the UI
    this._ensureMobileQuery();

    // Make sure to remove items that have the exclude annotation
    if (Svc.Annos.itemHasAnnotation(itemId, "places/excludeFromBackup")) {
      this.removeChangedID(this._GUIDForId(itemId, true));
      return true;
    }

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
    this._addId(folder);
  },

  onBeforeItemRemoved: function BMT_onBeforeItemRemoved(itemId) {
    if (this._ignore(itemId))
      return;

    this._log.trace("onBeforeItemRemoved: " + itemId);
    this._addId(itemId);
    let folder = Svc.Bookmark.getFolderIdForItem(itemId);
    this._addId(folder);
  },

  _ensureMobileQuery: function _ensureMobileQuery() {
    let anno = "PlacesOrganizer/OrganizerQuery";
    let find = function(val) Svc.Annos.getItemsWithAnnotation(anno, {}).filter(
      function(id) Utils.anno(id, anno) == val);

    // Don't continue if the Library isn't ready
    let all = find("AllBookmarks");
    if (all.length == 0)
      return;

    // Disable handling of notifications while changing the mobile query
    this.ignoreAll = true;

    let mobile = find("MobileBookmarks");
    let queryURI = Utils.makeURI("place:folder=" + kSpecialIds.mobile);
    let title = Str.sync.get("mobile.label");

    // Don't add OR do remove the mobile bookmarks if there's nothing
    if (Svc.Bookmark.getIdForItemAt(kSpecialIds.mobile, 0) == -1) {
      if (mobile.length != 0)
        Svc.Bookmark.removeItem(mobile[0]);
    }
    // Add the mobile bookmarks query if it doesn't exist
    else if (mobile.length == 0) {
      let query = Svc.Bookmark.insertBookmark(all[0], queryURI, -1, title);
      Utils.anno(query, anno, "MobileBookmarks");
      Utils.anno(query, "places/excludeFromBackup", 1);
    }
    // Make sure the existing title is correct
    else if (Svc.Bookmark.getItemTitle(mobile[0]) != title)
      Svc.Bookmark.setItemTitle(mobile[0], title);

    this.ignoreAll = false;
  },

  onItemChanged: function BMT_onItemChanged(itemId, property, isAnno, value) {
    if (this._ignore(itemId))
      return;

    // ignore annotations except for the ones that we sync
    let annos = ["bookmarkProperties/description",
      "bookmarkProperties/loadInSidebar", "bookmarks/staticTitle",
      "livemark/feedURI", "livemark/siteURI", "microsummary/generatorURI"];
    if (isAnno && annos.indexOf(property) == -1)
      return;

    // Ignore favicon changes to avoid unnecessary churn
    if (property == "favicon")
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
    this._addId(oldParent);
    if (oldParent != newParent) {
      this._addId(itemId);
      this._addId(newParent);
    }

    // Remove any position annotations now that the user moved the item
    Svc.Annos.removeItemAnnotation(itemId, PARENT_ANNO);
  },

  onBeginUpdateBatch: function BMT_onBeginUpdateBatch() {},
  onEndUpdateBatch: function BMT_onEndUpdateBatch() {},
  onItemRemoved: function BMT_onItemRemoved(itemId, folder, index) {},
  onItemVisited: function BMT_onItemVisited(itemId, aVisitID, time) {}
};
