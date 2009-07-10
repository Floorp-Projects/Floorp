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

// Annotation to use for shared bookmark folders, incoming and outgoing:
const INCOMING_SHARED_ANNO = "weave/shared-incoming";
const OUTGOING_SHARED_ANNO = "weave/shared-outgoing";
const SERVER_PATH_ANNO = "weave/shared-server-path";
// Standard names for shared files on the server
const KEYRING_FILE_NAME = "keyring";
const SHARED_BOOKMARK_FILE_NAME = "shared_bookmarks";
// Information for the folder that contains all incoming shares
const INCOMING_SHARE_ROOT_ANNO = "weave/mounted-shares-folder";
const INCOMING_SHARE_ROOT_NAME = "Shared Folders";

const SERVICE_NOT_SUPPORTED = "Service not supported on this platform";

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
].forEach(function([weaveId, placeName]) {
  Utils.lazy2(kSpecialIds, weaveId, function() Svc.Bookmark[placeName]);
});

function BookmarksEngine() {
  this._init();
}
BookmarksEngine.prototype = {
  __proto__: SyncEngine.prototype,
  name: "bookmarks",
  displayName: "Bookmarks",
  logName: "Bookmarks",
  _recordObj: PlacesItem,
  _storeObj: BookmarksStore,
  _trackerObj: BookmarksTracker
};

function BookmarksStore() {
  this._init();
}
BookmarksStore.prototype = {
  __proto__: Store.prototype,
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

  __ans: null,
  get _ans() {
    if (!this.__ans)
      this.__ans = Cc["@mozilla.org/browser/annotation-service;1"].
                   getService(Ci.nsIAnnotationService);
    return this.__ans;
  },

  _getItemIdForGUID: function BStore__getItemIdForGUID(GUID) {
    if (GUID in kSpecialIds)
      return kSpecialIds[GUID];

    return this._bms.getItemIdForGUID(GUID);
  },

  _getWeaveIdForItem: function BStore__getWeaveIdForItem(placeId) {
    for (let [weaveId, id] in Iterator(kSpecialIds))
      if (placeId == id)
        return weaveId;

    return this._bms.getItemGUID(placeId);
  },

  _isToplevel: function BStore__isToplevel(placeId) {
    for (let [weaveId, id] in Iterator(kSpecialIds))
      if (placeId == id)
        return true;

    if (this._bms.getFolderIdForItem(placeId) <= 0)
      return true;
    return false;
  },

  itemExists: function BStore_itemExists(id) {
    return this._getItemIdForGUID(id) > 0;
  },

  create: function BStore_create(record) {
    let newId;
    let parentId = this._getItemIdForGUID(record.parentid);

    if (parentId <= 0) {
      this._log.warn("Creating node with unknown parent -> reparenting to root");
      parentId = this._bms.bookmarksMenuFolder;
    }

    switch (record.type) {
    case "bookmark":
    case "microsummary": {
      this._log.debug(" -> creating bookmark \"" + record.title + "\"");
      let uri = Utils.makeURI(record.bmkUri);
      this._log.debug(" -> -> ParentID is " + parentId);
      this._log.debug(" -> -> uri is " + record.bmkUri);
      this._log.debug(" -> -> sortindex is " + record.sortindex);
      this._log.debug(" -> -> title is " + record.title);
      newId = this._bms.insertBookmark(parentId, uri, record.sortindex,
                                       record.title);
      this._ts.untagURI(uri, null);
      this._ts.tagURI(uri, record.tags);
      this._bms.setKeywordForBookmark(newId, record.keyword);
      if (record.description) {
        this._ans.setItemAnnotation(newId, "bookmarkProperties/description",
                                    record.description, 0,
                                   this._ans.EXPIRE_NEVER);
      }

      if (record.loadInSidebar)
        this._ans.setItemAnnotation(newId, "bookmarkProperties/loadInSidebar",
          true, 0, this._ans.EXPIRE_NEVER);

      if (record.type == "microsummary") {
        this._log.debug("   \-> is a microsummary");
        this._ans.setItemAnnotation(newId, "bookmarks/staticTitle",
                                    record.staticTitle || "", 0, this._ans.EXPIRE_NEVER);
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
      this._log.debug(" -> creating folder \"" + record.title + "\"");
      newId = this._bms.createFolder(parentId,
                                     record.title,
                                     record.sortindex);
      // If folder is an outgoing share, put the annotations on it:
      if ( record.outgoingSharedAnno != undefined ) {
	this._ans.setItemAnnotation(newId,
				    OUTGOING_SHARED_ANNO,
                                    record.outgoingSharedAnno,
				    0,
				    this._ans.EXPIRE_NEVER);
	this._ans.setItemAnnotation(newId,
				    SERVER_PATH_ANNO,
                                    record.serverPathAnno,
				    0,
				    this._ans.EXPIRE_NEVER);

      }
      break;
    case "livemark":
      this._log.debug(" -> creating livemark \"" + record.title + "\"");
      newId = this._ls.createLivemark(parentId,
                                      record.title,
                                      Utils.makeURI(record.siteUri),
                                      Utils.makeURI(record.feedUri),
                                      record.sortindex);
      break;
    case "incoming-share":
      /* even though incoming shares are folders according to the
       * bookmarkService, _wrap() wraps them as type=incoming-share, so we
       * handle them separately, like so: */
      this._log.debug(" -> creating incoming-share \"" + record.title + "\"");
      newId = this._bms.createFolder(parentId,
                                     record.title,
                                     record.sortindex);
      this._ans.setItemAnnotation(newId,
				  INCOMING_SHARED_ANNO,
                                  record.incomingSharedAnno,
				  0,
				  this._ans.EXPIRE_NEVER);
      this._ans.setItemAnnotation(newId,
				  SERVER_PATH_ANNO,
                                  record.serverPathAnno,
				  0,
				  this._ans.EXPIRE_NEVER);
      break;
    case "separator":
      this._log.debug(" -> creating separator");
      newId = this._bms.insertSeparator(parentId, record.sortindex);
      break;
    case "item":
      this._log.debug(" -> got a generic places item.. do nothing?");
      break;
    default:
      this._log.error("_create: Unknown item type: " + record.type);
      break;
    }
    if (newId) {
      this._log.trace("Setting GUID of new item " + newId + " to " + record.id);
      let cur = this._bms.getItemGUID(newId);
      if (cur == record.id)
        this._log.warn("Item " + newId + " already has GUID " + record.id);
      else
        this._bms.setItemGUID(newId, record.id);
    }
  },

  remove: function BStore_remove(record) {
    if (record.id in kSpecialIds) {
      this._log.warn("Attempted to remove root node (" + record.id +
                     ").  Skipping record removal.");
      return;
    }

    var itemId = this._bms.getItemIdForGUID(record.id);
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
    let itemId = this._getItemIdForGUID(record.id);

    if (record.id in kSpecialIds) {
      this._log.debug("Skipping update for root node.");
      return;
    }
    if (itemId <= 0) {
      this._log.debug("Skipping update for unknown item: " + record.id);
      return;
    }

    this._log.trace("Updating " + record.id + " (" + itemId + ")");

    if ((this._bms.getItemIndex(itemId) != record.sortindex) ||
        (this._bms.getFolderIdForItem(itemId) !=
         this._getItemIdForGUID(record.parentid))) {
      this._log.trace("Moving item (changing folder/index)");
      let parentid = this._getItemIdForGUID(record.parentid);
      this._bms.moveItem(itemId, parentid, record.sortindex);
    }

    for (let [key, val] in Iterator(record.cleartext)) {
      switch (key) {
      case "title":
        this._bms.setItemTitle(itemId, val);
        break;
      case "bmkUri":
        this._bms.changeBookmarkURI(itemId, Utils.makeURI(val));
        break;
      case "tags": {
        // filter out null/undefined/empty tags
        let tags = val.filter(function(t) t);
        let tagsURI = this._bms.getBookmarkURI(itemId);
        this._ts.untagURI(tagsURI, null);
        this._ts.tagURI(tagsURI, tags);
      } break;
      case "keyword":
        this._bms.setKeywordForBookmark(itemId, val);
        break;
      case "description":
        this._ans.setItemAnnotation(itemId, "bookmarkProperties/description",
                                    val, 0,
                                    this._ans.EXPIRE_NEVER);
        break;
      case "loadInSidebar":
        if (val)
          this._ans.setItemAnnotation(itemId, "bookmarkProperties/loadInSidebar",
            true, 0, this._ans.EXPIRE_NEVER);
        else
          this._ans.removeItemAnnotation(itemId, "bookmarkProperties/loadInSidebar");
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
      case "outgoingSharedAnno":
	this._ans.setItemAnnotation(itemId, OUTGOING_SHARED_ANNO,
				    val, 0,
				    this._ans.EXPIRE_NEVER);
	break;
      case "incomingSharedAnno":
	this._ans.setItemAnnotation(itemId, INCOMING_SHARED_ANNO,
				    val, 0,
				    this._ans.EXPIRE_NEVER);
	break;
      case "serverPathAnno":
	this._ans.setItemAnnotation(itemId, SERVER_PATH_ANNO,
				    val, 0,
				    this._ans.EXPIRE_NEVER);
	break;
      }
    }
  },

  changeItemID: function BStore_changeItemID(oldID, newID) {
    var itemId = this._getItemIdForGUID(oldID);
    if (itemId == null) // toplevel folder
      return;
    if (itemId <= 0) {
      this._log.warn("Can't change GUID " + oldID + " to " +
                      newID + ": Item does not exist");
      return;
    }

    var collision = this._getItemIdForGUID(newID);
    if (collision > 0) {
      this._log.warn("Can't change GUID " + oldID + " to " +
                      newID + ": new ID already in use");
      return;
    }

    this._log.debug("Changing GUID " + oldID + " to " + newID);
    this._bms.setItemGUID(itemId, newID);
  },

  _getNode: function BStore__getNode(folder) {
    let query = this._hsvc.getNewQuery();
    query.setFolders([folder], 1);
    return this._hsvc.executeQuery(query, this._hsvc.getNewQueryOptions()).root;
  },

  // XXX a little inefficient - isToplevel calls getFolderIdForItem too
  _itemDepth: function BStore__itemDepth(id) {
    if (this._isToplevel(id))
      return 0;
    return this._itemDepth(this._bms.getFolderIdForItem(id)) + 1;
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
      return this._ans.getItemAnnotation(id, "bookmarkProperties/description");
    } catch (e) {
      return undefined;
    }
  },

  _isLoadInSidebar: function BStore__isLoadInSidebar(id) {
    return this._ans.itemHasAnnotation(id, "bookmarkProperties/loadInSidebar");
  },

  _getStaticTitle: function BStore__getStaticTitle(id) {
    try {
      return this._ans.getItemAnnotation(id, "bookmarks/staticTitle");
    } catch (e) {
      return "";
    }
  },

  // Create a record starting from the weave id (places guid)
  createRecord: function BStore_createRecord(guid, cryptoMetaURL) {
    let record = this.cache.get(guid);
    if (record)
      return record;

    let placeId = this._bms.getItemIdForGUID(guid);
    if (placeId <= 0) { // deleted item
      record = new PlacesItem();
      record.id = guid;
      record.deleted = true;
      this.cache.put(guid, record);
      return record;
    }

    switch (this._bms.getItemType(placeId)) {
    case this._bms.TYPE_BOOKMARK:
      if (this._ms && this._ms.hasMicrosummary(placeId)) {
        record = new BookmarkMicsum();
        let micsum = this._ms.getMicrosummary(placeId);
        record.generatorUri = micsum.generator.uri.spec; // breaks local generators
        record.staticTitle = this._getStaticTitle(placeId);

      } else {
        record = new Bookmark();
        record.title = this._bms.getItemTitle(placeId);
      }

      record.bmkUri = this._bms.getBookmarkURI(placeId).spec;
      record.tags = this._getTags(record.bmkUri);
      record.keyword = this._bms.getKeywordForBookmark(placeId);
      record.description = this._getDescription(placeId);
      record.loadInSidebar = this._isLoadInSidebar(placeId);
      break;

    case this._bms.TYPE_FOLDER:
      if (this._ls.isLivemark(placeId)) {
        record = new Livemark();
        record.siteUri = this._ls.getSiteURI(placeId).spec;
        record.feedUri = this._ls.getFeedURI(placeId).spec;

      } else {
        record = new BookmarkFolder();
      }

      record.title = this._bms.getItemTitle(placeId);
      break;

    case this._bms.TYPE_SEPARATOR:
      record = new BookmarkSeparator();
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
    record.parentid = this._getWeaveParentIdForItem(placeId);
    record.depth = this._itemDepth(placeId);
    record.sortindex = this._bms.getItemIndex(placeId);
    record.encryption = cryptoMetaURL;

    this.cache.put(guid, record);
    return record;
  },

  _createMiniRecord: function BStore__createMiniRecord(placesId, depthIndex) {
    let foo = {id: this._bms.getItemGUID(placesId)};
    if (depthIndex) {
      foo.depth = this._itemDepth(placesId);
      foo.sortindex = this._bms.getItemIndex(placesId);
    }
    return foo;
  },

  _getWeaveParentIdForItem: function BStore__getWeaveParentIdForItem(itemId) {
    let parentid = this._bms.getFolderIdForItem(itemId);
    if (parentid == -1) {
      this._log.debug("Found orphan bookmark, reparenting to unfiled");
      parentid = this._bms.unfiledBookmarksFolder;
      this._bms.moveItem(itemId, parentid, -1);
    }
    return this._getWeaveIdForItem(parentid);
  },

  _getChildren: function BStore_getChildren(guid, depthIndex, items) {
    if (typeof(items) == "undefined")
      items = {};
    let node = guid; // the recursion case
    if (typeof(node) == "string") // callers will give us the guid as the first arg
      node = this._getNode(this._getItemIdForGUID(guid));

    if (node.type == node.RESULT_TYPE_FOLDER &&
        !this._ls.isLivemark(node.itemId)) {
      node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      node.containerOpen = true;
      for (var i = 0; i < node.childCount; i++) {
        let child = node.getChild(i);
        let foo = this._createMiniRecord(child.itemId, true);
        items[foo.id] = foo;
        this._getChildren(child, depthIndex, items);
      }
    }

    return items;
  },

  _getSiblings: function BStore__getSiblings(guid, depthIndex, items) {
    if (typeof(items) == "undefined")
      items = {};

    let parentid = this._bms.getFolderIdForItem(this._getItemIdForGUID(guid));
    let parent = this._getNode(parentid);
    parent.QueryInterface(Ci.nsINavHistoryQueryResultNode);
    parent.containerOpen = true;

    for (var i = 0; i < parent.childCount; i++) {
      let child = parent.getChild(i);
      let foo = this._createMiniRecord(child.itemId, true);
      items[foo.id] = foo;
    }

    return items;
  },

  getAllIDs: function BStore_getAllIDs() {
    let items = {};
    for (let [weaveId, id] in Iterator(kSpecialIds))
      if (weaveId != "places" && weaveId != "tags")
        this._getChildren(weaveId, true, items);
    return items;
  },

  createMetaRecords: function BStore_createMetaRecords(guid, items) {
    this._getChildren(guid, true, items);
    this._getSiblings(guid, true, items);
    return items;
  },

  wipe: function BStore_wipe() {
    for (let [weaveId, id] in Iterator(kSpecialIds))
      if (weaveId != "places")
        this._bms.removeFolderChildren(id);
  }
};

function BookmarksTracker() {
  this._init();
}
BookmarksTracker.prototype = {
  __proto__: Tracker.prototype,
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
    for (let [weaveId, id] in Iterator(kSpecialIds))
      this.ignoreID(this._bms.getItemGUID(id));

    this._bms.addObserver(this, false);
  },

  /**
   * Add a bookmark (places) id to be uploaded and bump up the sync score
   *
   * @param itemId
   *        Places internal id of the bookmark to upload
   */
  _addId: function BMT__addId(itemId) {
    if (this.addChangedID(this._bms.getItemGUID(itemId)))
      this._upScore();
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

    let tags = this._bms.tagsFolder;
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
  },

  onBeforeItemRemoved: function BMT_onBeforeItemRemoved(itemId) {
    if (this._ignore(itemId))
      return;

    this._log.trace("onBeforeItemRemoved: " + itemId);
    this._addId(itemId);
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
  },

  onBeginUpdateBatch: function BMT_onBeginUpdateBatch() {},
  onEndUpdateBatch: function BMT_onEndUpdateBatch() {},
  onItemRemoved: function BMT_onItemRemoved(itemId, folder, index) {},
  onItemVisited: function BMT_onItemVisited(itemId, aVisitID, time) {}
};
