/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["PlacesBackups"];

const Ci = Components.interfaces;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/BookmarkJSONUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Deprecated.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS",
  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
  "resource://gre/modules/Sqlite.jsm");

XPCOMUtils.defineLazyGetter(this, "localFileCtor",
  () => Components.Constructor("@mozilla.org/file/local;1",
                               "nsILocalFile", "initWithPath"));

this.PlacesBackups = {
  get _filenamesRegex() {
    delete this._filenamesRegex;
    return this._filenamesRegex =
      new RegExp("^(bookmarks)-([0-9-]+)(_[0-9]+)*\.(json|html)");
  },

  get folder() {
    Deprecated.warning(
      "PlacesBackups.folder is deprecated and will be removed in a future version",
      "https://bugzilla.mozilla.org/show_bug.cgi?id=859695");
    return this._folder;
  },

  /**
   * This exists just to avoid spamming deprecate warnings from internal calls
   * needed to support deprecated methods themselves.
   */
  get _folder() {
    let bookmarksBackupDir = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
    bookmarksBackupDir.append(this.profileRelativeFolderPath);
    if (!bookmarksBackupDir.exists()) {
      bookmarksBackupDir.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0700", 8));
      if (!bookmarksBackupDir.exists())
        throw("Unable to create bookmarks backup folder");
    }
    delete this._folder;
    return this._folder = bookmarksBackupDir;
  },

  /**
   * Gets backup folder asynchronously.
   * @return {Promise}
   * @resolve the folder (the folder string path).
   */
  getBackupFolder: function PB_getBackupFolder() {
    return Task.spawn(function* () {
      if (this._backupFolder) {
        return this._backupFolder;
      }
      let profileDir = OS.Constants.Path.profileDir;
      let backupsDirPath = OS.Path.join(profileDir, this.profileRelativeFolderPath);
      yield OS.File.makeDir(backupsDirPath, { ignoreExisting: true });
      return this._backupFolder = backupsDirPath;
    }.bind(this));
  },

  get profileRelativeFolderPath() "bookmarkbackups",

  /**
   * Cache current backups in a sorted (by date DESC) array.
   */
  get entries() {
    Deprecated.warning(
      "PlacesBackups.entries is deprecated and will be removed in a future version",
      "https://bugzilla.mozilla.org/show_bug.cgi?id=859695");
    return this._entries;
  },

  /**
   * This exists just to avoid spamming deprecate warnings from internal calls
   * needed to support deprecated methods themselves.
   */
  get _entries() {
    delete this._entries;
    this._entries = [];
    let files = this._folder.directoryEntries;
    while (files.hasMoreElements()) {
      let entry = files.getNext().QueryInterface(Ci.nsIFile);
      // A valid backup is any file that matches either the localized or
      // not-localized filename (bug 445704).
      let matches = entry.leafName.match(this._filenamesRegex);
      if (!entry.isHidden() && matches) {
        // Remove bogus backups in future dates.
        if (this.getDateForFile(entry) > new Date()) {
          entry.remove(false);
          continue;
        }
        this._entries.push(entry);
      }
    }
    this._entries.sort((a, b) => {
      let aDate = this.getDateForFile(a);
      let bDate = this.getDateForFile(b);
      return aDate < bDate ? 1 : aDate > bDate ? -1 : 0;
    });
    return this._entries;
  },

  /**
   * Cache current backups in a sorted (by date DESC) array.
   * @return {Promise}
   * @resolve a sorted array of string paths.
   */
  getBackupFiles: function PB_getBackupFiles() {
    return Task.spawn(function* () {
      if (this._backupFiles)
        return this._backupFiles;

      this._backupFiles = [];

      let backupFolderPath = yield this.getBackupFolder();
      let iterator = new OS.File.DirectoryIterator(backupFolderPath);
      yield iterator.forEach(function(aEntry) {
        // Since this is a lazy getter and OS.File I/O is serialized, we can
        // safely remove .tmp files without risking to remove ongoing backups.
        if (aEntry.name.endsWith(".tmp")) {
          OS.File.remove(aEntry.path);
          return;
        }

        let matches = aEntry.name.match(this._filenamesRegex);
        if (matches) {
          // Remove bogus backups in future dates.
          let filePath = aEntry.path;
          if (this.getDateForFile(filePath) > new Date()) {
            return OS.File.remove(filePath);
          } else {
            this._backupFiles.push(filePath);
          }
        }
      }.bind(this));
      iterator.close();

      this._backupFiles.sort((a, b) => {
        let aDate = this.getDateForFile(a);
        let bDate = this.getDateForFile(b);
        return aDate < bDate ? 1 : aDate > bDate ? -1 : 0;
      });

      return this._backupFiles;
    }.bind(this));
  },

  /**
   * Creates a filename for bookmarks backup files.
   *
   * @param [optional] aDateObj
   *                   Date object used to build the filename.
   *                   Will use current date if empty.
   * @return A bookmarks backup filename.
   */
  getFilenameForDate: function PB_getFilenameForDate(aDateObj) {
    let dateObj = aDateObj || new Date();
    // Use YYYY-MM-DD (ISO 8601) as it doesn't contain illegal characters
    // and makes the alphabetical order of multiple backup files more useful.
    return "bookmarks-" + dateObj.toLocaleFormat("%Y-%m-%d") + ".json";
  },

  /**
   * Creates a Date object from a backup file.  The date is the backup
   * creation date.
   *
   * @param aBackupFile
   *        nsIFile or string path of the backup.
   * @return A Date object for the backup's creation time.
   */
  getDateForFile: function PB_getDateForFile(aBackupFile) {
    let filename = (aBackupFile instanceof Ci.nsIFile) ? aBackupFile.leafName
                                                       : OS.Path.basename(aBackupFile);
    let matches = filename.match(this._filenamesRegex);
    if (!matches)
      throw("Invalid backup file name: " + filename);
    return new Date(matches[2].replace(/-/g, "/"));
  },

  /**
   * Get the most recent backup file.
   *
   * @param [optional] aFileExt
   *                   Force file extension.  Either "html" or "json".
   *                   Will check for both if not defined.
   * @returns nsIFile backup file
   */
  getMostRecent: function PB_getMostRecent(aFileExt) {
    Deprecated.warning(
      "PlacesBackups.getMostRecent is deprecated and will be removed in a future version",
      "https://bugzilla.mozilla.org/show_bug.cgi?id=859695");

    let fileExt = aFileExt || "(json|html)";
    for (let i = 0; i < this._entries.length; i++) {
      let rx = new RegExp("\." + fileExt + "$");
      if (this._entries[i].leafName.match(rx))
        return this._entries[i];
    }
    return null;
  },

   /**
    * Get the most recent backup file.
    *
    * @param [optional] aFileExt
    *                   Force file extension.  Either "html" or "json".
    *                   Will check for both if not defined.
    * @return {Promise}
    * @result the path to the file.
    */
   getMostRecentBackup: function PB_getMostRecentBackup(aFileExt) {
     return Task.spawn(function* () {
       let fileExt = aFileExt || "(json|html)";
       let entries = yield this.getBackupFiles();
       for (let entry of entries) {
         let rx = new RegExp("\." + fileExt + "$");
         if (OS.Path.basename(entry).match(rx)) {
           return entry;
         }
       }
       return null;
    }.bind(this));
  },

  /**
   * Serializes bookmarks using JSON, and writes to the supplied file.
   * Note: any item that should not be backed up must be annotated with
   *       "places/excludeFromBackup".
   *
   * @param aFilePath
   *        OS.File path for the "bookmarks.json" file to be created.
   * @return {Promise}
   * @resolves the number of serialized uri nodes.
   * @deprecated passing an nsIFile is deprecated
   */
  saveBookmarksToJSONFile: function PB_saveBookmarksToJSONFile(aFilePath) {
    if (aFilePath instanceof Ci.nsIFile) {
      Deprecated.warning("Passing an nsIFile to PlacesBackups.saveBookmarksToJSONFile " +
                         "is deprecated. Please use an OS.File path instead.",
                         "https://developer.mozilla.org/docs/JavaScript_OS.File");
      aFilePath = aFilePath.path;
    }
    return Task.spawn(function* () {
      let nodeCount = yield BookmarkJSONUtils.exportToFile(aFilePath);

      let backupFolderPath = yield this.getBackupFolder();
      if (OS.Path.dirname(aFilePath) == backupFolderPath) {
        // We are creating a backup in the default backups folder,
        // so just update the internal cache.
        this._entries.unshift(new localFileCtor(aFilePath));
        if (!this._backupFiles) {
          yield this.getBackupFiles();
        }
        this._backupFiles.unshift(aFilePath);
      } else {
        // If we are saving to a folder different than our backups folder, then
        // we also want to copy this new backup to it.
        // This way we ensure the latest valid backup is the same saved by the
        // user.  See bug 424389.
        let name = this.getFilenameForDate();
        let newFilename = this._appendMetaDataToFilename(name,
                                                         { nodeCount: nodeCount });
        let newFilePath = OS.Path.join(backupFolderPath, newFilename);
        let backupFile = yield this._getBackupFileForSameDate(name);
        if (!backupFile) {
          // Update internal cache if we are not replacing an existing
          // backup file.
          this._entries.unshift(new localFileCtor(newFilePath));
          if (!this._backupFiles) {
            yield this.getBackupFiles();
          }
          this._backupFiles.unshift(newFilePath);
        }

        yield OS.File.copy(aFilePath, newFilePath);
      }

      return nodeCount;
    }.bind(this));
  },

  /**
   * Creates a dated backup in <profile>/bookmarkbackups.
   * Stores the bookmarks using JSON.
   * Note: any item that should not be backed up must be annotated with
   *       "places/excludeFromBackup".
   *
   * @param [optional] int aMaxBackups
   *                       The maximum number of backups to keep.
   * @param [optional] bool aForceBackup
   *                        Forces creating a backup even if one was already
   *                        created that day (overwrites).
   * @return {Promise}
   */
  create: function PB_create(aMaxBackups, aForceBackup) {
    return Task.spawn(function* () {
      // Construct the new leafname.
      let newBackupFilename = this.getFilenameForDate();
      let mostRecentBackupFile = yield this.getMostRecentBackup();

      if (!aForceBackup) {
        let backupFiles = yield this.getBackupFiles();
        // If there are backups, limit them to aMaxBackups, if requested.
        if (backupFiles.length > 0 && typeof aMaxBackups == "number" &&
            aMaxBackups > -1 && backupFiles.length >= aMaxBackups) {
          let numberOfBackupsToDelete = backupFiles.length - aMaxBackups;

          // If we don't have today's backup, remove one more so that
          // the total backups after this operation does not exceed the
          // number specified in the pref.
          if (!this._isFilenameWithSameDate(OS.Path.basename(mostRecentBackupFile),
                                            newBackupFilename)) {
            numberOfBackupsToDelete++;
          }

          while (numberOfBackupsToDelete--) {
            this._entries.pop();
            let oldestBackup = this._backupFiles.pop();
            yield OS.File.remove(oldestBackup);
          }
        }

        // Do nothing if we already have this backup or we don't want backups.
        if (aMaxBackups === 0 ||
            (mostRecentBackupFile &&
             this._isFilenameWithSameDate(OS.Path.basename(mostRecentBackupFile),
                                          newBackupFilename)))
          return;
      }

      let backupFile = yield this._getBackupFileForSameDate(newBackupFilename);
      if (backupFile) {
        if (aForceBackup) {
          yield OS.File.remove(backupFile, { ignoreAbsent: true });
        } else {
          return;
        }
      }

      // Save bookmarks to a backup file.
      let backupFolder = yield this.getBackupFolder();
      let newBackupFile = OS.Path.join(backupFolder, newBackupFilename);
      let nodeCount = yield this.saveBookmarksToJSONFile(newBackupFile);
      // Rename the filename with metadata.
      let newFilenameWithMetaData = this._appendMetaDataToFilename(
                                      newBackupFilename,
                                      { nodeCount: nodeCount });
      let newBackupFileWithMetadata = OS.Path.join(backupFolder, newFilenameWithMetaData);
      yield OS.File.move(newBackupFile, newBackupFileWithMetadata);

      // Update internal cache.
      let newFileWithMetaData = new localFileCtor(newBackupFileWithMetadata);
      this._entries.pop();
      this._entries.unshift(newFileWithMetaData);
      this._backupFiles.pop();
      this._backupFiles.unshift(newBackupFileWithMetadata);
    }.bind(this));
  },

  _appendMetaDataToFilename:
  function PB__appendMetaDataToFilename(aFilename, aMetaData) {
    let matches = aFilename.match(this._filenamesRegex);
    let newFilename = matches[1] + "-" + matches[2] + "_" +
                      aMetaData.nodeCount + "." + matches[4];
    return newFilename;
  },

  /**
   * Gets the bookmark count for backup file.
   *
   * @param aFilePath
   *        File path The backup file.
   *
   * @return the bookmark count or null.
   */
  getBookmarkCountForFile: function PB_getBookmarkCountForFile(aFilePath) {
    let count = null;
    let filename = OS.Path.basename(aFilePath);
    let matches = filename.match(this._filenamesRegex);

    if (matches && matches[3])
      count = matches[3].replace(/_/g, "");
    return count;
  },

  _isFilenameWithSameDate:
  function PB__isFilenameWithSameDate(aSourceName, aTargetName) {
    let sourceMatches = aSourceName.match(this._filenamesRegex);
    let targetMatches = aTargetName.match(this._filenamesRegex);

    return (sourceMatches && targetMatches &&
            sourceMatches[1] == targetMatches[1] &&
            sourceMatches[2] == targetMatches[2] &&
            sourceMatches[4] == targetMatches[4]);
    },

  _getBackupFileForSameDate:
  function PB__getBackupFileForSameDate(aFilename) {
    return Task.spawn(function* () {
      let backupFolderPath = yield this.getBackupFolder();
      let iterator = new OS.File.DirectoryIterator(backupFolderPath);
      let backupFile;

      yield iterator.forEach(function(aEntry) {
        if (this._isFilenameWithSameDate(aEntry.name, aFilename)) {
          backupFile = aEntry.path;
          return iterator.close();
        }
      }.bind(this));
      yield iterator.close();

      return backupFile;
    }.bind(this));
  },

  /**
   * Gets a bookmarks tree representation usable to create backups in different
   * file formats.  The root or the tree is PlacesUtils.placesRootId.
   * Items annotated with PlacesUtils.EXCLUDE_FROM_BACKUP_ANNO and all of their
   * descendants are excluded.
   *
   * @return an object representing a tree with the places root as its root.
   *         Each bookmark is represented by an object having these properties:
   *         * id: the item id (make this not enumerable after bug 824502)
   *         * title: the title
   *         * guid: unique id
   *         * parent: item id of the parent folder, not enumerable
   *         * index: the position in the parent
   *         * dateAdded: microseconds from the epoch
   *         * lastModified: microseconds from the epoch
   *         * type: type of the originating node as defined in PlacesUtils 
   *         The following properties exist only for a subset of bookmarks:
   *         * annos: array of annotations
   *         * uri: url
   *         * iconuri: favicon's url
   *         * keyword: associated keyword
   *         * charset: last known charset
   *         * tags: csv string of tags
   *         * root: string describing whether this represents a root
   *         * children: array of child items in a folder
   */
  getBookmarksTree: function () {
    return Task.spawn(function* () {
      let dbFilePath = OS.Path.join(OS.Constants.Path.profileDir, "places.sqlite");
      let conn = yield Sqlite.openConnection({ path: dbFilePath,
                                               sharedMemoryCache: false });
      let rows = [];
      try {
        rows = yield conn.execute(
          "SELECT b.id, h.url, IFNULL(b.title, '') AS title, b.parent, " +
                 "b.position AS [index], b.type, b.dateAdded, b.lastModified, " +
                 "b.guid, f.url AS iconuri, " +
                 "( SELECT GROUP_CONCAT(t.title, ',') " +
                   "FROM moz_bookmarks b2 " +
                   "JOIN moz_bookmarks t ON t.id = +b2.parent AND t.parent = :tags_folder " +
                   "WHERE b2.fk = h.id " +
                 ") AS tags, " +
                 "EXISTS (SELECT 1 FROM moz_items_annos WHERE item_id = b.id LIMIT 1) AS has_annos, " +
                 "( SELECT a.content FROM moz_annos a " +
                   "JOIN moz_anno_attributes n ON a.anno_attribute_id = n.id " +
                   "WHERE place_id = h.id AND n.name = :charset_anno " +
                 ") AS charset " +
          "FROM moz_bookmarks b " +
          "LEFT JOIN moz_bookmarks p ON p.id = b.parent " +
          "LEFT JOIN moz_places h ON h.id = b.fk " +
          "LEFT JOIN moz_favicons f ON f.id = h.favicon_id " +
          "WHERE b.id <> :tags_folder AND b.parent <> :tags_folder AND p.parent <> :tags_folder " +
          "ORDER BY b.parent, b.position",
          { tags_folder: PlacesUtils.tagsFolderId,
            charset_anno: PlacesUtils.CHARSET_ANNO });
      } catch(e) {
        Cu.reportError("Unable to query the database " + e);
      } finally {
        yield conn.close();
      }

      let startTime = Date.now();
      // Create a Map for lookup and recursive building of the tree.
      let itemsMap = new Map();
      for (let row of rows) {
        let id = row.getResultByName("id");
        try {
          let bookmark = sqliteRowToBookmarkObject(row);
          if (itemsMap.has(id)) {
            // Since children may be added before parents, we should merge with
            // the existing object.
            let original = itemsMap.get(id);
            for (prop in bookmark) {
              original[prop] = bookmark[prop];
            }
            bookmark = original;
          }
          else {
            itemsMap.set(id, bookmark);
          }

          // Append bookmark to its parent.
          if (!itemsMap.has(bookmark.parent))
            itemsMap.set(bookmark.parent, {});
          let parent = itemsMap.get(bookmark.parent);
          if (!("children" in parent))
            parent.children = [];
          parent.children.push(bookmark);
        } catch (e) {
          Cu.reportError("Error while reading node " + id + " " + e);
        }
      }

      // Handle excluded items, by removing entire subtrees pointed by them.
      function removeFromMap(id) {
        // Could have been removed by a previous call, since we can't
        // predict order of items in EXCLUDE_FROM_BACKUP_ANNO.
        if (itemsMap.has(id)) {
          let excludedItem = itemsMap.get(id);
          if (excludedItem.children) {
            for (let child of excludedItem.children) {
              removeFromMap(child.id);
            }
          }
          // Remove the excluded item from its parent's children...
          let parentItem = itemsMap.get(excludedItem.parent);
          parentItem.children = parentItem.children.filter(aChild => aChild.id != id);
          // ...then remove it from the map.
          itemsMap.delete(id);
        }
      }

      for (let id of PlacesUtils.annotations.getItemsWithAnnotation(
                       PlacesUtils.EXCLUDE_FROM_BACKUP_ANNO)) {
        removeFromMap(id);
      }

      // Report the time taken to build the tree. This doesn't take into
      // account the time spent in the query since that's off the main-thread.
      try {
        Services.telemetry
                .getHistogramById("PLACES_BACKUPS_BOOKMARKSTREE_MS")
                .add(Date.now() - startTime);
      } catch (ex) {
        Components.utils.reportError("Unable to report telemetry.");
      }

      return [itemsMap.get(PlacesUtils.placesRootId), itemsMap.size];
    });
  }
}

/**
 * Helper function to convert a Sqlite.jsm row to a bookmark object
 * representation.
 *
 * @param aRow The Sqlite.jsm result row.
 */
function sqliteRowToBookmarkObject(aRow) {
  let bookmark = {};
  for (let p of [ "id" ,"guid", "title", "index", "dateAdded", "lastModified" ]) {
    bookmark[p] = aRow.getResultByName(p);
  }
  Object.defineProperty(bookmark, "parent",
                        { value: aRow.getResultByName("parent") });

  let type = aRow.getResultByName("type");

  // Add annotations.
  if (aRow.getResultByName("has_annos")) {
    try {
      bookmark.annos = PlacesUtils.getAnnotationsForItem(bookmark.id);
    } catch (e) {
      Cu.reportError("Unexpected error while reading annotations " + e);
    }
  }

  switch (type) {
    case Ci.nsINavBookmarksService.TYPE_BOOKMARK:
      // TODO: What about shortcuts?
      bookmark.type = PlacesUtils.TYPE_X_MOZ_PLACE;
      // This will throw if we try to serialize an invalid url and the node will
      // just be skipped.
      bookmark.uri = NetUtil.newURI(aRow.getResultByName("url")).spec;
      // Keywords are cached, so this should be decently fast.
      let keyword = PlacesUtils.bookmarks.getKeywordForBookmark(bookmark.id);
      if (keyword)
        bookmark.keyword = keyword;
      let charset = aRow.getResultByName("charset");
      if (charset)
        bookmark.charset = charset;
      let tags = aRow.getResultByName("tags");
      if (tags)
        bookmark.tags = tags;
      let iconuri = aRow.getResultByName("iconuri");
      if (iconuri)
        bookmark.iconuri = iconuri;
      break;
    case Ci.nsINavBookmarksService.TYPE_FOLDER:
      bookmark.type = PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER;

      // Mark root folders.
      if (bookmark.id == PlacesUtils.placesRootId)
        bookmark.root = "placesRoot";
      else if (bookmark.id == PlacesUtils.bookmarksMenuFolderId)
        bookmark.root = "bookmarksMenuFolder";
      else if (bookmark.id == PlacesUtils.unfiledBookmarksFolderId)
        bookmark.root = "unfiledBookmarksFolder";
      else if (bookmark.id == PlacesUtils.toolbarFolderId)
        bookmark.root = "toolbarFolder";
      break;
    case Ci.nsINavBookmarksService.TYPE_SEPARATOR:
      bookmark.type = PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR;
      break;
    default:
      Cu.reportError("Unexpected bookmark type");
      break;
  }
  return bookmark;
}
