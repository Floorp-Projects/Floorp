/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* This is a JavaScript module (JSM) to be imported via
  * Components.utils.import() and acts as a singleton. Only the following
  * listed symbols will exposed on import, and only when and where imported.
  */

var EXPORTED_SYMBOLS = ["PlacesItem", "Bookmark", "Separator", "Livemark",
                        "BookmarkFolder", "DumpBookmarks"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/PlacesBackups.jsm");
Cu.import("resource://gre/modules/PlacesSyncUtils.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://tps/logger.jsm");

var DumpBookmarks = function TPS_Bookmarks__DumpBookmarks() {
  let cb = Async.makeSpinningCallback();
  PlacesBackups.getBookmarksTree().then(result => {
    let [bookmarks, count] = result;
    Logger.logInfo("Dumping Bookmarks...\n" + JSON.stringify(bookmarks) + "\n\n");
    cb(null);
  }).then(null, error => {
    cb(error);
  });
  cb.wait();
};

/**
 * extend, causes a child object to inherit from a parent
 */
function extend(child, supertype)
{
   child.prototype.__proto__ = supertype.prototype;
}

/**
 * PlacesItemProps object, holds properties for places items
 */
function PlacesItemProps(props) {
  this.location = null;
  this.uri = null;
  this.loadInSidebar = null;
  this.keyword = null;
  this.title = null;
  this.description = null;
  this.after = null;
  this.before = null;
  this.folder = null;
  this.position = null;
  this.delete = false;
  this.siteUri = null;
  this.feedUri = null;
  this.livemark = null;
  this.tags = null;
  this.last_item_pos = null;
  this.type = null;

  for (var prop in props) {
    if (prop in this)
      this[prop] = props[prop];
  }
}

/**
 * PlacesItem object.  Base class for places items.
 */
function PlacesItem(props) {
  this.props = new PlacesItemProps(props);
  if (this.props.location == null)
    this.props.location = "menu";
  if ("changes" in props)
    this.updateProps = new PlacesItemProps(props.changes);
  else
    this.updateProps = null;
}

/**
 * Instance methods for generic places items.
 */
PlacesItem.prototype = {
  // an array of possible root folders for places items
  _bookmarkFolders: {
    "places": "placesRoot",
    "menu": "bookmarksMenuFolder",
    "tags": "tagFolder",
    "unfiled": "unfiledBookmarksFolder",
    "toolbar": "toolbarFolder",
  },

  toString: function() {
    var that = this;
    var props = ['uri', 'title', 'location', 'folder', 'feedUri', 'siteUri', 'livemark'];
    var string = (this.props.type ? this.props.type + " " : "") +
      "(" +
      (function() {
        var ret = [];
        for (var i in props) {
          if (that.props[props[i]]) {
            ret.push(props[i] + ": " + that.props[props[i]])
          }
        }
        return ret;
      })().join(", ") + ")";
    return string;
  },

  GetSyncId() {
    let guid = Async.promiseSpinningly(PlacesUtils.promiseItemGuid(this.props.item_id));
    return PlacesSyncUtils.bookmarks.guidToSyncId(guid);
  },

  /**
   * GetPlacesNodeId
   *
   * Finds the id of the an item with the specified properties in the places
   * database.
   *
   * @param folder The id of the folder to search
   * @param type The type of the item to find, or null to match any item;
   *        this is one of the values listed at
   *        https://developer.mozilla.org/en/nsINavHistoryResultNode#Constants
   * @param title The title of the item to find, or null to match any title
   * @param uri The uri of the item to find, or null to match any uri
   *
   * @return the node id if the item was found, otherwise -1
   */
  GetPlacesNodeId: function (folder, type, title, uri) {
    let node_id = -1;

    let options = PlacesUtils.history.getNewQueryOptions();
    let query = PlacesUtils.history.getNewQuery();
    query.setFolders([folder], 1);
    let result = PlacesUtils.history.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;

    for (let j = 0; j < rootNode.childCount; j ++) {
      let node = rootNode.getChild(j);
      if (node.title == title) {
        if (type == null || type == undefined || node.type == type)
          if (uri == undefined || uri == null || node.uri.spec == uri.spec)
            node_id = node.itemId;
      }
    }
    rootNode.containerOpen = false;

    return node_id;
  },

  /**
   * IsAdjacentTo
   *
   * Determines if this object is immediately adjacent to another.
   *
   * @param itemName The name of the other object; this may be any kind of
   *        places item
   * @param relativePos The relative position of the other object.  If -1,
   *        it means the other object should precede this one, if +1,
   *        the other object should come after this one
   * @return true if this object is immediately adjacent to the other object,
   *         otherwise false
   */
  IsAdjacentTo: function(itemName, relativePos) {
    Logger.AssertTrue(this.props.folder_id != -1 && this.props.item_id != -1,
      "Either folder_id or item_id was invalid");
    let other_id = this.GetPlacesNodeId(this.props.folder_id, null, itemName);
    Logger.AssertTrue(other_id != -1, "item " + itemName + " not found");
    let other_pos = PlacesUtils.bookmarks.getItemIndex(other_id);
    let this_pos = PlacesUtils.bookmarks.getItemIndex(this.props.item_id);
    if (other_pos + relativePos != this_pos) {
      Logger.logPotentialError("Invalid position - " +
       (this.props.title ? this.props.title : this.props.folder) +
      " not " + (relativePos == 1 ? "after " : "before ") + itemName +
      " for " + this.toString());
      return false;
    }
    return true;
  },

  /**
   * GetItemIndex
   *
   * Gets the item index for this places item.
   *
   * @return the item index, or -1 if there's an error
   */
  GetItemIndex: function() {
    if (this.props.item_id == -1)
      return -1;
    return PlacesUtils.bookmarks.getItemIndex(this.props.item_id);
  },

  /**
   * GetFolder
   *
   * Gets the folder id for the specified bookmark folder
   *
   * @param location The full path of the folder, which must begin
   *        with one of the bookmark root folders
   * @return the folder id if the folder is found, otherwise -1
   */
  GetFolder: function(location) {
    let folder_parts = location.split("/");
    if (!(folder_parts[0] in this._bookmarkFolders)) {
      return -1;
    }
    let folder_id = PlacesUtils.bookmarks[this._bookmarkFolders[folder_parts[0]]];
    for (let i = 1; i < folder_parts.length; i++) {
      let subfolder_id = this.GetPlacesNodeId(
        folder_id,
        Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER,
        folder_parts[i]);
      if (subfolder_id == -1) {
        return -1;
      }
      else {
        folder_id = subfolder_id;
      }
    }
    return folder_id;
  },

  /**
   * CreateFolder
   *
   * Creates a bookmark folder.
   *
   * @param location The full path of the folder, which must begin
   *        with one of the bookmark root folders
   * @return the folder id if the folder was created, otherwise -1
   */
  CreateFolder: function(location) {
    let folder_parts = location.split("/");
    if (!(folder_parts[0] in this._bookmarkFolders)) {
      return -1;
    }
    let folder_id = PlacesUtils.bookmarks[this._bookmarkFolders[folder_parts[0]]];
    for (let i = 1; i < folder_parts.length; i++) {
      let subfolder_id = this.GetPlacesNodeId(
        folder_id,
        Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER,
        folder_parts[i]);
      if (subfolder_id == -1) {
        folder_id = PlacesUtils.bookmarks.createFolder(folder_id,
                                                 folder_parts[i], -1);
      }
      else {
        folder_id = subfolder_id;
      }
    }
    return folder_id;
  },

  /**
   * GetOrCreateFolder
   *
   * Locates the specified folder; if not found it is created.
   *
   * @param location The full path of the folder, which must begin
   *        with one of the bookmark root folders
   * @return the folder id if the folder was found or created, otherwise -1
   */
  GetOrCreateFolder: function(location) {
    folder_id = this.GetFolder(location);
    if (folder_id == -1)
      folder_id = this.CreateFolder(location);
    return folder_id;
  },

  /**
   * CheckDescription
   *
   * Compares the description of this places item with an expected
   * description.
   *
   * @param expectedDescription The description this places item is
   *        expected to have
   * @return true if the actual and expected descriptions match, or if
   *         there is no expected description; otherwise false
   */
  CheckDescription: function(expectedDescription) {
    if (expectedDescription != null) {
      let description = "";
      if (PlacesUtils.annotations.itemHasAnnotation(this.props.item_id,
          "bookmarkProperties/description")) {
        description = PlacesUtils.annotations.getItemAnnotation(
          this.props.item_id, "bookmarkProperties/description");
      }
      if (description != expectedDescription) {
        Logger.logPotentialError("Invalid description, expected: " +
          expectedDescription + ", actual: " + description + " for " +
          this.toString());
        return false;
      }
    }
    return true;
  },

  /**
   * CheckPosition
   *
   * Verifies the position of this places item.
   *
   * @param before The name of the places item that this item should be
            before, or null if this check should be skipped
   * @param after The name of the places item that this item should be
            after, or null if this check should be skipped
   * @param last_item_pos The index of the places item above this one,
   *        or null if this check should be skipped
   * @return true if this item is in the correct position, otherwise false
   */
  CheckPosition: function(before, after, last_item_pos) {
    if (after)
      if (!this.IsAdjacentTo(after, 1)) return false;
    if (before)
      if (!this.IsAdjacentTo(before, -1)) return false;
    if (last_item_pos != null && last_item_pos > -1) {
      if (this.GetItemIndex() != last_item_pos + 1) {
        Logger.logPotentialError("Item not found at the expected index, got " +
          this.GetItemIndex() + ", expected " + (last_item_pos + 1) + " for " +
          this.toString());
        return false;
      }
    }
    return true;
  },

  /**
   * SetLocation
   *
   * Moves this places item to a different folder.
   *
   * @param location The full path of the folder to which to move this
   *        places item, which must begin with one of the bookmark root
   *        folders; if null, no changes are made
   * @return nothing if successful, otherwise an exception is thrown
   */
  SetLocation: function(location) {
    if (location != null) {
      let newfolder_id = this.GetOrCreateFolder(location);
      Logger.AssertTrue(newfolder_id != -1, "Location " + location +
                        " doesn't exist; can't change item's location");
      PlacesUtils.bookmarks.moveItem(this.props.item_id, newfolder_id, -1);
      this.props.folder_id = newfolder_id;
    }
  },

  /**
   * SetDescription
   *
   * Updates the description for this places item.
   *
   * @param description The new description to set; if null, no changes are
   *        made
   * @return nothing
   */
  SetDescription: function(description) {
    if (description != null) {
      if (description != "")
        PlacesUtils.annotations.setItemAnnotation(this.props.item_id,
                                      "bookmarkProperties/description",
                                      description,
                                      0,
                                      PlacesUtils.annotations.EXPIRE_NEVER);
      else
        PlacesUtils.annotations.removeItemAnnotation(this.props.item_id,
                                         "bookmarkProperties/description");
    }
  },

  /**
   * SetPosition
   *
   * Updates the position of this places item within this item's current
   * folder.  Use SetLocation to change folders.
   *
   * @param position The new index this item should be moved to; if null,
   *        no changes are made; if -1, this item is moved to the bottom of
   *        the current folder
   * @return nothing if successful, otherwise an exception is thrown
   */
  SetPosition: function(position) {
    if (position != null) {
      let newposition = -1;
      if (position != -1) {
        newposition = this.GetPlacesNodeId(this.props.folder_id,
                                           null, position);
        Logger.AssertTrue(newposition != -1, "position " + position +
                          " is invalid; unable to change position");
        newposition = PlacesUtils.bookmarks.getItemIndex(newposition);
      }
      PlacesUtils.bookmarks.moveItem(this.props.item_id,
                               this.props.folder_id, newposition);
    }
  },

  /**
   * Update the title of this places item
   *
   * @param title The new title to set for this item; if null, no changes
   *        are made
   * @return nothing
   */
  SetTitle: function(title) {
    if (title != null) {
      PlacesUtils.bookmarks.setItemTitle(this.props.item_id, title);
    }
  },
};

/**
 * Bookmark class constructor.  Initializes instance properties.
 */
function Bookmark(props) {
  PlacesItem.call(this, props);
  if (this.props.title == null)
    this.props.title = this.props.uri;
  this.props.type = "bookmark";
}

/**
 * Bookmark instance methods.
 */
Bookmark.prototype = {
  /**
   * SetKeyword
   *
   * Update this bookmark's keyword.
   *
   * @param keyword The keyword to set for this bookmark; if null, no
   *        changes are made
   * @return nothing
   */
  SetKeyword: function(keyword) {
    if (keyword != null) {
      // Mirror logic from PlacesSyncUtils's updateBookmarkMetadata
      let entry = Async.promiseSpinningly(PlacesUtils.keywords.fetch({
        url: this.props.uri,
      }));
      if (entry) {
        Async.promiseSpinningly(PlacesUtils.keywords.remove(entry));
      }
      Async.promiseSpinningly(PlacesUtils.keywords.insert({
        keyword: keyword,
        url: this.props.uri
      }));
    }
  },

  /**
   * SetLoadInSidebar
   *
   * Updates this bookmark's loadInSidebar property.
   *
   * @param loadInSidebar if true, the loadInSidebar property will be set,
   *        if false, it will be cleared, and any other value will result
   *        in no change
   * @return nothing
   */
  SetLoadInSidebar: function(loadInSidebar) {
    if (loadInSidebar == true)
      PlacesUtils.annotations.setItemAnnotation(this.props.item_id,
                                    "bookmarkProperties/loadInSidebar",
                                    true,
                                    0,
                                    PlacesUtils.annotations.EXPIRE_NEVER);
    else if (loadInSidebar == false)
      PlacesUtils.annotations.removeItemAnnotation(this.props.item_id,
                                       "bookmarkProperties/loadInSidebar");
  },

  /**
   * SetTitle
   *
   * Updates this bookmark's title.
   *
   * @param title The new title to set for this boomark; if null, no changes
   *        are made
   * @return nothing
   */
  SetTitle: function(title) {
    if (title)
      PlacesUtils.bookmarks.setItemTitle(this.props.item_id, title);
  },

  /**
   * SetUri
   *
   * Updates this bookmark's URI.
   *
   * @param uri The new URI to set for this boomark; if null, no changes
   *        are made
   * @return nothing
   */
  SetUri: function(uri) {
    if (uri) {
      let newURI = Services.io.newURI(uri, null, null);
      PlacesUtils.bookmarks.changeBookmarkURI(this.props.item_id, newURI);
    }
  },

  /**
   * SetTags
   *
   * Updates this bookmark's tags.
   *
   * @param tags An array of tags which should be associated with this
   *        bookmark; any previous tags are removed; if this param is null,
   *        no changes are made.  If this param is an empty array, all
   *        tags are removed from this bookmark.
   * @return nothing
   */
  SetTags: function(tags) {
    if (tags != null) {
      let URI = Services.io.newURI(this.props.uri, null, null);
      PlacesUtils.tagging.untagURI(URI, null);
      if (tags.length > 0)
        PlacesUtils.tagging.tagURI(URI, tags);
    }
  },

  /**
   * Create
   *
   * Creates the bookmark described by this object's properties.
   *
   * @return the id of the created bookmark
   */
  Create: function() {
    this.props.folder_id = this.GetOrCreateFolder(this.props.location);
    Logger.AssertTrue(this.props.folder_id != -1, "Unable to create " +
      "bookmark, error creating folder " + this.props.location);
    let bookmarkURI = Services.io.newURI(this.props.uri, null, null);
    this.props.item_id = PlacesUtils.bookmarks.insertBookmark(this.props.folder_id,
                                                        bookmarkURI,
                                                        -1,
                                                        this.props.title);
    this.SetKeyword(this.props.keyword);
    this.SetDescription(this.props.description);
    this.SetLoadInSidebar(this.props.loadInSidebar);
    this.SetTags(this.props.tags);
    return this.props.item_id;
  },

  /**
   * Update
   *
   * Updates this bookmark's properties according the properties on this
   * object's 'updateProps' property.
   *
   * @return nothing
   */
  Update: function() {
    Logger.AssertTrue(this.props.item_id != -1 && this.props.item_id != null,
      "Invalid item_id during Remove");
    this.SetDescription(this.updateProps.description);
    this.SetLoadInSidebar(this.updateProps.loadInSidebar);
    this.SetTitle(this.updateProps.title);
    this.SetUri(this.updateProps.uri);
    this.SetKeyword(this.updateProps.keyword);
    this.SetTags(this.updateProps.tags);
    this.SetLocation(this.updateProps.location);
    this.SetPosition(this.updateProps.position);
  },

  /**
   * Find
   *
   * Locates the bookmark which corresponds to this object's properties.
   *
   * @return the bookmark id if the bookmark was found, otherwise -1
   */
  Find: function() {
    this.props.folder_id = this.GetFolder(this.props.location);
    if (this.props.folder_id == -1) {
      Logger.logError("Unable to find folder " + this.props.location);
      return -1;
    }
    let bookmarkTitle = this.props.title;
    this.props.item_id = this.GetPlacesNodeId(this.props.folder_id,
                                              null,
                                              bookmarkTitle,
                                              this.props.uri);

    if (this.props.item_id == -1) {
      Logger.logPotentialError(this.toString() + " not found");
      return -1;
    }
    if (!this.CheckDescription(this.props.description))
      return -1;
    if (this.props.keyword != null) {
      let { keyword } = Async.promiseSpinningly(
        PlacesSyncUtils.bookmarks.fetch(this.GetSyncId()));
      if (keyword != this.props.keyword) {
        Logger.logPotentialError("Incorrect keyword - expected: " +
          this.props.keyword + ", actual: " + keyword +
          " for " + this.toString());
        return -1;
      }
    }
    let loadInSidebar = PlacesUtils.annotations.itemHasAnnotation(
      this.props.item_id,
      "bookmarkProperties/loadInSidebar");
    if (loadInSidebar)
      loadInSidebar = PlacesUtils.annotations.getItemAnnotation(
        this.props.item_id,
        "bookmarkProperties/loadInSidebar");
    if (this.props.loadInSidebar != null &&
        loadInSidebar != this.props.loadInSidebar) {
      Logger.logPotentialError("Incorrect loadInSidebar setting - expected: " +
        this.props.loadInSidebar + ", actual: " + loadInSidebar +
        " for " + this.toString());
      return -1;
    }
    if (this.props.tags != null) {
      try {
        let URI = Services.io.newURI(this.props.uri, null, null);
        let tags = PlacesUtils.tagging.getTagsForURI(URI, {});
        tags.sort();
        this.props.tags.sort();
        if (JSON.stringify(tags) != JSON.stringify(this.props.tags)) {
          Logger.logPotentialError("Wrong tags - expected: " +
            JSON.stringify(this.props.tags) + ", actual: " +
            JSON.stringify(tags) + " for " + this.toString());
          return -1;
        }
      }
      catch (e) {
        Logger.logPotentialError("error processing tags " + e);
        return -1;
      }
    }
    if (!this.CheckPosition(this.props.before,
                            this.props.after,
                            this.props.last_item_pos))
      return -1;
    return this.props.item_id;
  },

  /**
   * Remove
   *
   * Removes this bookmark.  The bookmark should have been located previously
   * by a call to Find.
   *
   * @return nothing
   */
  Remove: function() {
    Logger.AssertTrue(this.props.item_id != -1 && this.props.item_id != null,
      "Invalid item_id during Remove");
    PlacesUtils.bookmarks.removeItem(this.props.item_id);
  },
};

extend(Bookmark, PlacesItem);

/**
 * BookmarkFolder class constructor. Initializes instance properties.
 */
function BookmarkFolder(props) {
  PlacesItem.call(this, props);
  this.props.type = "folder";
}

/**
 * BookmarkFolder instance methods
 */
BookmarkFolder.prototype = {
  /**
   * Create
   *
   * Creates the bookmark folder described by this object's properties.
   *
   * @return the id of the created bookmark folder
   */
  Create: function() {
    this.props.folder_id = this.GetOrCreateFolder(this.props.location);
    Logger.AssertTrue(this.props.folder_id != -1, "Unable to create " +
      "folder, error creating parent folder " + this.props.location);
    this.props.item_id = PlacesUtils.bookmarks.createFolder(this.props.folder_id,
                                                      this.props.folder,
                                                      -1);
    this.SetDescription(this.props.description);
    return this.props.folder_id;
  },

  /**
   * Find
   *
   * Locates the bookmark folder which corresponds to this object's
   * properties.
   *
   * @return the folder id if the folder was found, otherwise -1
   */
  Find: function() {
    this.props.folder_id = this.GetFolder(this.props.location);
    if (this.props.folder_id == -1) {
      Logger.logError("Unable to find folder " + this.props.location);
      return -1;
    }
    this.props.item_id = this.GetPlacesNodeId(
                              this.props.folder_id,
                              Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER,
                              this.props.folder);
    if (!this.CheckDescription(this.props.description))
      return -1;
    if (!this.CheckPosition(this.props.before,
                            this.props.after,
                            this.props.last_item_pos))
      return -1;
    return this.props.item_id;
  },

  /**
   * Remove
   *
   * Removes this folder.  The folder should have been located previously
   * by a call to Find.
   *
   * @return nothing
   */
  Remove: function() {
    Logger.AssertTrue(this.props.item_id != -1 && this.props.item_id != null,
      "Invalid item_id during Remove");
    PlacesUtils.bookmarks.removeFolderChildren(this.props.item_id);
    PlacesUtils.bookmarks.removeItem(this.props.item_id);
  },

  /**
   * Update
   *
   * Updates this bookmark's properties according the properties on this
   * object's 'updateProps' property.
   *
   * @return nothing
   */
  Update: function() {
    Logger.AssertTrue(this.props.item_id != -1 && this.props.item_id != null,
      "Invalid item_id during Update");
    this.SetLocation(this.updateProps.location);
    this.SetPosition(this.updateProps.position);
    this.SetTitle(this.updateProps.folder);
    this.SetDescription(this.updateProps.description);
  },
};

extend(BookmarkFolder, PlacesItem);

/**
 * Livemark class constructor. Initialzes instance properties.
 */
function Livemark(props) {
  PlacesItem.call(this, props);
  this.props.type = "livemark";
}

/**
 * Livemark instance methods
 */
Livemark.prototype = {
  /**
   * Create
   *
   * Creates the livemark described by this object's properties.
   *
   * @return the id of the created livemark
   */
  Create: function() {
    this.props.folder_id = this.GetOrCreateFolder(this.props.location);
    Logger.AssertTrue(this.props.folder_id != -1, "Unable to create " +
      "folder, error creating parent folder " + this.props.location);
    let siteURI = null;
    if (this.props.siteUri != null)
      siteURI = Services.io.newURI(this.props.siteUri, null, null);
    let livemarkObj = {parentId: this.props.folder_id,
                       title: this.props.livemark,
                       siteURI: siteURI,
                       feedURI: Services.io.newURI(this.props.feedUri, null, null),
                       index: PlacesUtils.bookmarks.DEFAULT_INDEX};

    // Until this can handle asynchronous creation, we need to spin.
    let spinningCb = Async.makeSpinningCallback();

    PlacesUtils.livemarks.addLivemark(livemarkObj).then(
      aLivemark => { spinningCb(null, [Components.results.NS_OK, aLivemark]) },
      () => { spinningCb(null, [Components.results.NS_ERROR_UNEXPECTED, aLivemark]) }
    );

    let [status, livemark] = spinningCb.wait();
    if (!Components.isSuccessCode(status)) {
      throw new Error(status);
    }

    this.props.item_id = livemark.id;
    return this.props.item_id;
  },

  /**
   * Find
   *
   * Locates the livemark which corresponds to this object's
   * properties.
   *
   * @return the item id if the livemark was found, otherwise -1
   */
  Find: function() {
    this.props.folder_id = this.GetFolder(this.props.location);
    if (this.props.folder_id == -1) {
      Logger.logError("Unable to find folder " + this.props.location);
      return -1;
    }
    this.props.item_id = this.GetPlacesNodeId(
                              this.props.folder_id,
                              Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER,
                              this.props.livemark);
    if (!PlacesUtils.annotations
                    .itemHasAnnotation(this.props.item_id, PlacesUtils.LMANNO_FEEDURI)) {
      Logger.logPotentialError("livemark folder found, but it's just a regular folder, for " +
        this.toString());
      this.props.item_id = -1;
      return -1;
    }
    let feedURI = Services.io.newURI(this.props.feedUri, null, null);
    let lmFeedURISpec =
      PlacesUtils.annotations.getItemAnnotation(this.props.item_id,
                                                PlacesUtils.LMANNO_FEEDURI);
    if (feedURI.spec != lmFeedURISpec) {
      Logger.logPotentialError("livemark feed uri not correct, expected: " +
        this.props.feedUri + ", actual: " + lmFeedURISpec +
        " for " + this.toString());
      return -1;
    }
    if (this.props.siteUri != null) {
      let siteURI = Services.io.newURI(this.props.siteUri, null, null);
      let lmSiteURISpec =
        PlacesUtils.annotations.getItemAnnotation(this.props.item_id,
                                                  PlacesUtils.LMANNO_SITEURI);
      if (siteURI.spec != lmSiteURISpec) {
        Logger.logPotentialError("livemark site uri not correct, expected: " +
        this.props.siteUri + ", actual: " + lmSiteURISpec + " for " +
        this.toString());
        return -1;
      }
    }
    if (!this.CheckPosition(this.props.before,
                            this.props.after,
                            this.props.last_item_pos))
      return -1;
    return this.props.item_id;
  },

  /**
   * Update
   *
   * Updates this livemark's properties according the properties on this
   * object's 'updateProps' property.
   *
   * @return nothing
   */
  Update: function() {
    Logger.AssertTrue(this.props.item_id != -1 && this.props.item_id != null,
      "Invalid item_id during Update");
    this.SetLocation(this.updateProps.location);
    this.SetPosition(this.updateProps.position);
    this.SetTitle(this.updateProps.livemark);
    return true;
  },

  /**
   * Remove
   *
   * Removes this livemark.  The livemark should have been located previously
   * by a call to Find.
   *
   * @return nothing
   */
  Remove: function() {
    Logger.AssertTrue(this.props.item_id != -1 && this.props.item_id != null,
      "Invalid item_id during Remove");
    PlacesUtils.bookmarks.removeItem(this.props.item_id);
  },
};

extend(Livemark, PlacesItem);

/**
 * Separator class constructor. Initializes instance properties.
 */
function Separator(props) {
  PlacesItem.call(this, props);
  this.props.type = "separator";
}

/**
 * Separator instance methods.
 */
Separator.prototype = {
  /**
   * Create
   *
   * Creates the bookmark separator described by this object's properties.
   *
   * @return the id of the created separator
   */
  Create: function () {
    this.props.folder_id = this.GetOrCreateFolder(this.props.location);
    Logger.AssertTrue(this.props.folder_id != -1, "Unable to create " +
      "folder, error creating parent folder " + this.props.location);
    this.props.item_id = PlacesUtils.bookmarks.insertSeparator(this.props.folder_id,
                                                         -1);
    return this.props.item_id;
  },

  /**
   * Find
   *
   * Locates the bookmark separator which corresponds to this object's
   * properties.
   *
   * @return the item id if the separator was found, otherwise -1
   */
  Find: function () {
    this.props.folder_id = this.GetFolder(this.props.location);
    if (this.props.folder_id == -1) {
      Logger.logError("Unable to find folder " + this.props.location);
      return -1;
    }
    if (this.props.before == null && this.props.last_item_pos == null) {
      Logger.logPotentialError("Separator requires 'before' attribute if it's the" +
        "first item in the list");
      return -1;
    }
    let expected_pos = -1;
    if (this.props.before) {
      other_id = this.GetPlacesNodeId(this.props.folder_id,
                                      null,
                                      this.props.before);
      if (other_id == -1) {
        Logger.logPotentialError("Can't find places item " + this.props.before +
          " for locating separator");
        return -1;
      }
      expected_pos = PlacesUtils.bookmarks.getItemIndex(other_id) - 1;
    }
    else {
      expected_pos = this.props.last_item_pos + 1;
    }
    this.props.item_id = PlacesUtils.bookmarks.getIdForItemAt(this.props.folder_id,
                                                        expected_pos);
    if (this.props.item_id == -1) {
      Logger.logPotentialError("No separator found at position " + expected_pos);
    }
    else {
      if (PlacesUtils.bookmarks.getItemType(this.props.item_id) !=
          PlacesUtils.bookmarks.TYPE_SEPARATOR) {
        Logger.logPotentialError("Places item at position " + expected_pos +
          " is not a separator");
        return -1;
      }
    }
    return this.props.item_id;
  },

  /**
   * Update
   *
   * Updates this separator's properties according the properties on this
   * object's 'updateProps' property.
   *
   * @return nothing
   */
  Update: function() {
    Logger.AssertTrue(this.props.item_id != -1 && this.props.item_id != null,
      "Invalid item_id during Update");
    this.SetLocation(this.updateProps.location);
    this.SetPosition(this.updateProps.position);
    return true;
  },

  /**
   * Remove
   *
   * Removes this separator.  The separator should have been located
   * previously by a call to Find.
   *
   * @return nothing
   */
  Remove: function() {
    Logger.AssertTrue(this.props.item_id != -1 && this.props.item_id != null,
      "Invalid item_id during Update");
    PlacesUtils.bookmarks.removeItem(this.props.item_id);
  },
};

extend(Separator, PlacesItem);
