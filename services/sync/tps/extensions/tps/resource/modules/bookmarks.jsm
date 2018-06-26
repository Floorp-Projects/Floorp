/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* This is a JavaScript module (JSM) to be imported via
  * Components.utils.import() and acts as a singleton. Only the following
  * listed symbols will exposed on import, and only when and where imported.
  */

var EXPORTED_SYMBOLS = ["PlacesItem", "Bookmark", "Separator", "Livemark",
                        "BookmarkFolder", "DumpBookmarks"];

ChromeUtils.import("resource://gre/modules/PlacesBackups.jsm");
ChromeUtils.import("resource://gre/modules/PlacesSyncUtils.jsm");
ChromeUtils.import("resource://gre/modules/PlacesUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://tps/logger.jsm");

async function DumpBookmarks() {
  let [bookmarks, ] = await PlacesBackups.getBookmarksTree();
  Logger.logInfo("Dumping Bookmarks...\n" + JSON.stringify(bookmarks, undefined, 2) + "\n\n");
}

/**
 * extend, causes a child object to inherit from a parent
 */
function extend(child, supertype) {
   child.prototype.__proto__ = supertype.prototype;
}

/**
 * PlacesItemProps object, holds properties for places items
 */
function PlacesItemProps(props) {
  this.location = null;
  this.uri = null;
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
    "places": PlacesUtils.bookmarks.rootGuid,
    "menu": PlacesUtils.bookmarks.menuGuid,
    "tags": PlacesUtils.bookmarks.tagsGuid,
    "unfiled": PlacesUtils.bookmarks.unfiledGuid,
    "toolbar": PlacesUtils.bookmarks.toolbarGuid,
    "mobile": PlacesUtils.bookmarks.mobileGuid,
  },

  _typeMap: new Map([
    [PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER, PlacesUtils.bookmarks.TYPE_FOLDER],
    [PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR, PlacesUtils.bookmarks.TYPE_SEPARATOR],
    [PlacesUtils.TYPE_X_MOZ_PLACE, PlacesUtils.bookmarks.TYPE_BOOKMARK],
  ]),

  toString() {
    var that = this;
    var props = ["uri", "title", "location", "folder", "feedUri", "siteUri", "livemark"];
    var string = (this.props.type ? this.props.type + " " : "") +
      "(" +
      (function() {
        var ret = [];
        for (var i in props) {
          if (that.props[props[i]]) {
            ret.push(props[i] + ": " + that.props[props[i]]);
          }
        }
        return ret;
      })().join(", ") + ")";
    return string;
  },

  /**
   * GetPlacesChildGuid
   *
   * Finds the guid of the an item with the specified properties in the places
   * database under the specified parent.
   *
   * @param folder The guid of the folder to search
   * @param type The type of the item to find, or null to match any item;
   *        this is one of the PlacesUtils.bookmarks.TYPE_* values
   * @param title The title of the item to find, or null to match any title
   * @param uri The uri of the item to find, or null to match any uri
   *
   * @return the node id if the item was found, otherwise null
   */
  async GetPlacesChildGuid(folder, type, title, uri) {
    let children = (await PlacesUtils.promiseBookmarksTree(folder)).children;
    if (!children) {
      return null;
    }
    let guid = null;
    for (let node of children) {
      if (node.title == title) {
        let nodeType = this._typeMap.get(node.type);
        if (type == null || type == undefined || nodeType == type)
          if (uri == undefined || uri == null || node.uri.spec == uri.spec) {
            // Note that this is suspect as we return the *last* matching
            // child, which some tests rely on (ie, an early-return here causes
            // at least 1 test to fail). But that's a yak for another day.
            guid = node.guid;
          }
      }
    }
    return guid;
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
  async IsAdjacentTo(itemName, relativePos) {
    Logger.AssertTrue(this.props.folder_id != -1 && this.props.guid != null,
      "Either folder_id or guid was invalid");
    let otherGuid = await this.GetPlacesChildGuid(this.props.parentGuid, null, itemName);
    Logger.AssertTrue(otherGuid, "item " + itemName + " not found");
    let other_pos = (await PlacesUtils.bookmarks.fetch(otherGuid)).index;
    let this_pos = (await PlacesUtils.bookmarks.fetch(this.props.guid)).index;
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
  async GetItemIndex() {
    if (this.props.guid == null)
      return -1;
    return (await PlacesUtils.bookmarks.fetch(this.props.guid)).index;
  },

  /**
   * GetFolder
   *
   * Gets the folder guid for the specified bookmark folder
   *
   * @param location The full path of the folder, which must begin
   *        with one of the bookmark root folders
   * @return the folder guid if the folder is found, otherwise null
   */
  async GetFolder(location) {
    let folder_parts = location.split("/");
    if (!(folder_parts[0] in this._bookmarkFolders)) {
      return null;
    }
    let folderGuid = this._bookmarkFolders[folder_parts[0]];
    for (let i = 1; i < folder_parts.length; i++) {
      let guid = await this.GetPlacesChildGuid(
        folderGuid,
        PlacesUtils.bookmarks.TYPE_FOLDER,
        folder_parts[i]);
      if (guid == null) {
        return null;
      }
      folderGuid = guid;
    }
    return folderGuid;
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
  async CreateFolder(location) {
    let folder_parts = location.split("/");
    if (!(folder_parts[0] in this._bookmarkFolders)) {
      return -1;
    }
    let folderGuid = this._bookmarkFolders[folder_parts[0]];
    for (let i = 1; i < folder_parts.length; i++) {
      let subfolderGuid = await this.GetPlacesChildGuid(
        folderGuid,
        PlacesUtils.bookmarks.TYPE_FOLDER,
        folder_parts[i]
      );
      if (subfolderGuid == null) {
        let {guid} = await PlacesUtils.bookmarks.insert({parentGuid: folderGuid,
                                                         name: folder_parts[i],
                                                         type: PlacesUtils.bookmarks.TYPE_FOLDER});
        folderGuid = guid;
      } else {
        folderGuid = subfolderGuid;
      }
    }
    return folderGuid;
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
  async GetOrCreateFolder(location) {
    let parentGuid = await this.GetFolder(location);
    if (parentGuid == null)
      parentGuid = await this.CreateFolder(location);
    return parentGuid;
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
  async CheckDescription(expectedDescription) {
    if (expectedDescription != null) {
      // Use PlacesSyncUtils as it gives us the description.
      let info = await PlacesSyncUtils.bookmarks.fetch(this.props.guid);
      if (info.description != expectedDescription) {
        Logger.logPotentialError("Invalid description, expected: " +
          expectedDescription + ", actual: " + info.description + " for " +
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
  async CheckPosition(before, after, last_item_pos) {
    if (after)
      if (!(await this.IsAdjacentTo(after, 1))) return false;
    if (before)
      if (!(await this.IsAdjacentTo(before, -1))) return false;
    if (last_item_pos != null && last_item_pos > -1) {
      let index = await this.GetItemIndex();
      if (index != last_item_pos + 1) {
        Logger.logPotentialError("Item not found at the expected index, got " +
          index + ", expected " + (last_item_pos + 1) + " for " +
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
  async SetLocation(location) {
    if (location != null) {
      let newfolderGuid = await this.GetOrCreateFolder(location);
      Logger.AssertTrue(newfolderGuid, "Location " + location +
                        " doesn't exist; can't change item's location");
      await PlacesUtils.bookmarks.update({
        guid: this.props.guid,
        parentGuid: newfolderGuid,
        index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      });
      this.props.parentGuid = newfolderGuid;
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
  async SetDescription(description) {
    let itemId = await PlacesUtils.promiseItemId(this.props.guid);

    if (description != null) {
      if (description != "")
        PlacesUtils.annotations.setItemAnnotation(itemId,
                                      "bookmarkProperties/description",
                                      description,
                                      0,
                                      PlacesUtils.annotations.EXPIRE_NEVER);
      else
        PlacesUtils.annotations.removeItemAnnotation(itemId,
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
   *        the current folder. Otherwise, must be a string which is the
   *        title of an existing item in the folder, who's current position
   *        is used as the index.
   * @return nothing if successful, otherwise an exception is thrown
   */
  async SetPosition(position) {
    if (position == null) {
      return;
    }
    let index = -1;
    if (position != -1) {
      let existingGuid = await this.GetPlacesChildGuid(this.props.parentGuid,
                                                       null, position);
      if (existingGuid) {
        index = (await PlacesUtils.bookmarks.fetch(existingGuid)).index;
      }
      Logger.AssertTrue(index != -1, "position " + position +
                        " is invalid; unable to change position");
    }
    await PlacesUtils.bookmarks.update({guid: this.props.guid, index});
  },

  /**
   * Update the title of this places item
   *
   * @param title The new title to set for this item; if null, no changes
   *        are made
   * @return nothing
   */
  async SetTitle(title) {
    if (title != null) {
      await PlacesUtils.bookmarks.update({guid: this.props.guid, title});
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
  async SetKeyword(keyword) {
    if (keyword != null) {
      // Mirror logic from PlacesSyncUtils's updateBookmarkMetadata
      let entry = await PlacesUtils.keywords.fetch({url: this.props.uri});
      if (entry) {
        await PlacesUtils.keywords.remove(entry);
      }
      await PlacesUtils.keywords.insert({keyword, url: this.props.uri});
    }
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
  async SetUri(uri) {
    if (uri) {
      let url = Services.io.newURI(uri);
      await PlacesUtils.bookmarks.update({guid: this.props.guid, url});
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
  SetTags(tags) {
    if (tags != null) {
      let URI = Services.io.newURI(this.props.uri);
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
  async Create() {
    this.props.parentGuid = await this.GetOrCreateFolder(this.props.location);
    Logger.AssertTrue(this.props.parentGuid, "Unable to create " +
      "bookmark, error creating folder " + this.props.location);
    let bookmarkURI = Services.io.newURI(this.props.uri);
    let {guid} = await PlacesUtils.bookmarks.insert({parentGuid: this.props.parentGuid,
                                                     url: bookmarkURI,
                                                     title: this.props.title});
    this.props.guid = guid;
    await this.SetKeyword(this.props.keyword);
    await this.SetDescription(this.props.description);
    await this.SetTags(this.props.tags);
    return this.props.guid;
  },

  /**
   * Update
   *
   * Updates this bookmark's properties according the properties on this
   * object's 'updateProps' property.
   *
   * @return nothing
   */
  async Update() {
    Logger.AssertTrue(this.props.guid, "Invalid guid during Update");
    await this.SetDescription(this.updateProps.description);
    await this.SetTitle(this.updateProps.title);
    await this.SetUri(this.updateProps.uri);
    await this.SetKeyword(this.updateProps.keyword);
    await this.SetTags(this.updateProps.tags);
    await this.SetLocation(this.updateProps.location);
    await this.SetPosition(this.updateProps.position);
  },

  /**
   * Find
   *
   * Locates the bookmark which corresponds to this object's properties.
   *
   * @return the bookmark guid if the bookmark was found, otherwise null
   */
  async Find() {
    this.props.parentGuid = await this.GetFolder(this.props.location);

    if (this.props.parentGuid == null) {
      Logger.logError("Unable to find folder " + this.props.location);
      return null;
    }
    let bookmarkTitle = this.props.title;
    this.props.guid = await this.GetPlacesChildGuid(this.props.parentGuid,
                                                    null,
                                                    bookmarkTitle,
                                                    this.props.uri);

    if (!this.props.guid) {
      Logger.logPotentialError(this.toString() + " not found");
      return null;
    }
    if (!(await this.CheckDescription(this.props.description))) {
      return null;
    }
    if (this.props.keyword != null) {
      let {keyword} = await PlacesSyncUtils.bookmarks.fetch(this.props.guid);
      if (keyword != this.props.keyword) {
        Logger.logPotentialError("Incorrect keyword - expected: " +
          this.props.keyword + ", actual: " + keyword +
          " for " + this.toString());
        return null;
      }
    }
    if (this.props.tags != null) {
      try {
        let URI = Services.io.newURI(this.props.uri);
        let tags = PlacesUtils.tagging.getTagsForURI(URI, {});
        tags.sort();
        this.props.tags.sort();
        if (JSON.stringify(tags) != JSON.stringify(this.props.tags)) {
          Logger.logPotentialError("Wrong tags - expected: " +
            JSON.stringify(this.props.tags) + ", actual: " +
            JSON.stringify(tags) + " for " + this.toString());
          return null;
        }
      } catch (e) {
        Logger.logPotentialError("error processing tags " + e);
        return null;
      }
    }
    if (!(await this.CheckPosition(this.props.before,
                                 this.props.after,
                                 this.props.last_item_pos)))
      return null;
    return this.props.guid;
  },

  /**
   * Remove
   *
   * Removes this bookmark.  The bookmark should have been located previously
   * by a call to Find.
   *
   * @return nothing
   */
  async Remove() {
    Logger.AssertTrue(this.props.guid, "Invalid guid during Remove");
    await PlacesUtils.bookmarks.remove(this.props.guid);
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
  async Create() {
    this.props.parentGuid = await this.GetOrCreateFolder(this.props.location);
    Logger.AssertTrue(this.props.parentGuid, "Unable to create " +
      "folder, error creating parent folder " + this.props.location);
    let {guid} = await PlacesUtils.bookmarks.insert({parentGuid: this.props.parentGuid,
                                                     title: this.props.folder,
                                                     index: PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                     type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     });
    this.props.guid = guid;
    await this.SetDescription(this.props.description);
    return this.props.parentGuid;
  },

  /**
   * Find
   *
   * Locates the bookmark folder which corresponds to this object's
   * properties.
   *
   * @return the folder guid if the folder was found, otherwise null
   */
  async Find() {
    this.props.parentGuid = await this.GetFolder(this.props.location);
    if (this.props.parentGuid == null) {
      Logger.logError("Unable to find folder " + this.props.location);
      return null;
    }
    this.props.guid = await this.GetPlacesChildGuid(
                              this.props.parentGuid,
                              PlacesUtils.bookmarks.TYPE_FOLDER,
                              this.props.folder);
    if (this.props.guid == null) {
      return null;
    }
    if (!(await this.CheckDescription(this.props.description))) {
      return null;
    }
    if (!(await this.CheckPosition(this.props.before,
                                   this.props.after,
                                   this.props.last_item_pos))) {
      return null;
    }
    return this.props.guid;
  },

  /**
   * Remove
   *
   * Removes this folder.  The folder should have been located previously
   * by a call to Find.
   *
   * @return nothing
   */
  async Remove() {
    Logger.AssertTrue(this.props.guid, "Invalid guid during Remove");
    await PlacesUtils.bookmarks.remove(this.props.guid);
  },

  /**
   * Update
   *
   * Updates this bookmark's properties according the properties on this
   * object's 'updateProps' property.
   *
   * @return nothing
   */
  async Update() {
    Logger.AssertTrue(this.props.guid, "Invalid guid during Update");
    await this.SetLocation(this.updateProps.location);
    await this.SetPosition(this.updateProps.position);
    await this.SetTitle(this.updateProps.folder);
    await this.SetDescription(this.updateProps.description);
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
  async Create() {
    this.props.parentGuid = await this.GetOrCreateFolder(this.props.location);
    Logger.AssertTrue(this.props.parentGuid, "Unable to create " +
      "folder, error creating parent folder " + this.props.location);
    let siteURI = null;
    if (this.props.siteUri != null)
      siteURI = Services.io.newURI(this.props.siteUri);
    let livemarkObj = {parentGuid: this.props.parentGuid,
                       title: this.props.livemark,
                       siteURI,
                       feedURI: Services.io.newURI(this.props.feedUri),
                       index: PlacesUtils.bookmarks.DEFAULT_INDEX};

    let livemark = await PlacesUtils.livemarks.addLivemark(livemarkObj);
    this.props.guid = livemark.guid;
    return this.props.guid;
  },

  /**
   * Find
   *
   * Locates the livemark which corresponds to this object's
   * properties.
   *
   * @return the item guid if the livemark was found, otherwise null
   */
  async Find() {
    this.props.parentGuid = await this.GetFolder(this.props.location);
    if (this.props.parentGuid == null) {
      Logger.logError("Unable to find folder " + this.props.location);
      return null;
    }
    this.props.guid = await this.GetPlacesChildGuid(
                              this.props.parentGuid,
                              PlacesUtils.bookmarks.TYPE_FOLDER,
                              this.props.livemark);
    if (!this.props.guid) {
      Logger.logPotentialError("can't find livemark for " + this.toString());
      return null;
    }
    let itemId = await PlacesUtils.promiseItemId(this.props.guid);
    if (!PlacesUtils.annotations
                    .itemHasAnnotation(itemId, PlacesUtils.LMANNO_FEEDURI)) {
      Logger.logPotentialError("livemark folder found, but it's just a regular folder, for " +
        this.toString());
      this.props.guid = null;
      return null;
    }
    let feedURI = Services.io.newURI(this.props.feedUri);
    let lmFeedURISpec =
      PlacesUtils.annotations.getItemAnnotation(itemId,
                                                PlacesUtils.LMANNO_FEEDURI);
    if (feedURI.spec != lmFeedURISpec) {
      Logger.logPotentialError("livemark feed uri not correct, expected: " +
        this.props.feedUri + ", actual: " + lmFeedURISpec +
        " for " + this.toString());
      return null;
    }
    if (this.props.siteUri != null) {
      let siteURI = Services.io.newURI(this.props.siteUri);
      let lmSiteURISpec =
        PlacesUtils.annotations.getItemAnnotation(itemId,
                                                  PlacesUtils.LMANNO_SITEURI);
      if (siteURI.spec != lmSiteURISpec) {
        Logger.logPotentialError("livemark site uri not correct, expected: " +
        this.props.siteUri + ", actual: " + lmSiteURISpec + " for " +
        this.toString());
        return null;
      }
    }
    if (!(await this.CheckPosition(this.props.before,
                                   this.props.after,
                                   this.props.last_item_pos)))
      return null;
    return this.props.guid;
  },

  /**
   * Update
   *
   * Updates this livemark's properties according the properties on this
   * object's 'updateProps' property.
   *
   * @return nothing
   */
  async Update() {
    Logger.AssertTrue(this.props.guid, "Invalid guid during Update");
    await this.SetLocation(this.updateProps.location);
    await this.SetPosition(this.updateProps.position);
    await this.SetTitle(this.updateProps.livemark);
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
  async Remove() {
    Logger.AssertTrue(this.props.guid, "Invalid guid during Remove");
    await PlacesUtils.bookmarks.remove(this.props.guid);
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
  async Create() {
    this.props.parentGuid = await this.GetOrCreateFolder(this.props.location);
    Logger.AssertTrue(this.props.parentGuid, "Unable to create " +
      "folder, error creating parent folder " + this.props.location);
    let {guid} = await PlacesUtils.bookmarks.insert({
      parentGuid: this.props.parentGuid,
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR
    });
    this.props.guid = guid;
    return guid;
  },

  /**
   * Find
   *
   * Locates the bookmark separator which corresponds to this object's
   * properties.
   *
   * @return the item guid if the separator was found, otherwise null
   */
  async Find() {
    this.props.parentGuid = await this.GetFolder(this.props.location);
    if (this.props.parentGuid == null) {
      Logger.logError("Unable to find folder " + this.props.location);
      return null;
    }
    if (this.props.before == null && this.props.last_item_pos == null) {
      Logger.logPotentialError("Separator requires 'before' attribute if it's the" +
        "first item in the list");
      return null;
    }
    let expected_pos = -1;
    if (this.props.before) {
      let otherGuid = this.GetPlacesChildGuid(this.props.parentGuid,
                                              null,
                                              this.props.before);
      if (otherGuid == null) {
        Logger.logPotentialError("Can't find places item " + this.props.before +
          " for locating separator");
        return null;
      }
      expected_pos = (await PlacesUtils.bookmarks.fetch(otherGuid)).index - 1;
    } else {
      expected_pos = this.props.last_item_pos + 1;
    }
    // Note these are IDs instead of GUIDs.
    let children = await PlacesSyncUtils.bookmarks.fetchChildRecordIds(this.props.parentGuid);
    this.props.guid = children[expected_pos];
    if (this.props.guid == null) {
      Logger.logPotentialError("No separator found at position " + expected_pos);
      return null;
    }
    let info = await PlacesUtils.bookmarks.fetch(this.props.guid);
    if (info.type != PlacesUtils.bookmarks.TYPE_SEPARATOR) {
      Logger.logPotentialError("Places item at position " + expected_pos +
        " is not a separator");
      return null;
    }
    return this.props.guid;
  },

  /**
   * Update
   *
   * Updates this separator's properties according the properties on this
   * object's 'updateProps' property.
   *
   * @return nothing
   */
  async Update() {
    Logger.AssertTrue(this.props.guid, "Invalid guid during Update");
    await this.SetLocation(this.updateProps.location);
    await this.SetPosition(this.updateProps.position);
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
  async Remove() {
    Logger.AssertTrue(this.props.guid, "Invalid guid during Update");
    await PlacesUtils.bookmarks.remove(this.props.guid);
  },
};

extend(Separator, PlacesItem);
