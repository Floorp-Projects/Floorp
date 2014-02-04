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

XPCOMUtils.defineLazyModuleGetter(this, "OS",
  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
  "resource://gre/modules/FileUtils.jsm");

this.PlacesBackups = {
  get _filenamesRegex() {
    // Get the localized backup filename, will be used to clear out
    // old backups with a localized name (bug 445704).
    let localizedFilename =
      PlacesUtils.getFormattedString("bookmarksArchiveFilename", [new Date()]);
    let localizedFilenamePrefix =
      localizedFilename.substr(0, localizedFilename.indexOf("-"));
    delete this._filenamesRegex;
    return this._filenamesRegex =
      new RegExp("^(bookmarks|" + localizedFilenamePrefix + ")-([0-9-]+)(_[0-9]+)*\.(json|html)");
  },

  get folder() {
    Deprecated.warning(
      "PlacesBackups.folder is deprecated and will be removed in a future version",
      "https://bugzilla.mozilla.org/show_bug.cgi?id=859695");

    let bookmarksBackupDir = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
    bookmarksBackupDir.append(this.profileRelativeFolderPath);
    if (!bookmarksBackupDir.exists()) {
      bookmarksBackupDir.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0700", 8));
      if (!bookmarksBackupDir.exists())
        throw("Unable to create bookmarks backup folder");
    }
    delete this.folder;
    return this.folder = bookmarksBackupDir;
  },

  /**
   * Gets backup folder asynchronously.
   * @return {Promise}
   * @resolve the folder (the folder string path).
   */
  getBackupFolder: function PB_getBackupFolder() {
    return Task.spawn(function() {
      if (this._folder) {
        throw new Task.Result(this._folder);
      }
      let profileDir = OS.Constants.Path.profileDir;
      let backupsDirPath = OS.Path.join(profileDir, this.profileRelativeFolderPath);
      yield OS.File.makeDir(backupsDirPath, { ignoreExisting: true }).then(
        function onSuccess() {
          this._folder = backupsDirPath;
         }.bind(this),
         function onError() {
           throw("Unable to create bookmarks backup folder");
         });
       throw new Task.Result(this._folder);
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

    delete this.entries;
    this.entries = [];
    let files = this.folder.directoryEntries;
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
        this.entries.push(entry);
      }
    }
    this.entries.sort((a, b) => {
      let aDate = this.getDateForFile(a);
      let bDate = this.getDateForFile(b);
      return aDate < bDate ? 1 : aDate > bDate ? -1 : 0;
    });
    return this.entries;
  },

  /**
   * Cache current backups in a sorted (by date DESC) array.
   * @return {Promise}
   * @resolve a sorted array of string paths.
   */
  getBackupFiles: function PB_getBackupFiles() {
    return Task.spawn(function() {
      if (this._backupFiles) {
        throw new Task.Result(this._backupFiles);
      }
      this._backupFiles = [];

      let backupFolderPath = yield this.getBackupFolder();
      let iterator = new OS.File.DirectoryIterator(backupFolderPath);
      yield iterator.forEach(function(aEntry) {
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

      this._backupFiles.sort(function(a, b) {
        let aDate = this.getDateForFile(a);
        let bDate = this.getDateForFile(b);
        return aDate < bDate ? 1 : aDate > bDate ? -1 : 0;
      }.bind(this));

      throw new Task.Result(this._backupFiles);
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
    for (let i = 0; i < this.entries.length; i++) {
      let rx = new RegExp("\." + fileExt + "$");
      if (this.entries[i].leafName.match(rx))
        return this.entries[i];
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
     return Task.spawn(function() {
       let fileExt = aFileExt || "(json|html)";
       let entries = yield this.getBackupFiles();
       for (let entry of entries) {
         let rx = new RegExp("\." + fileExt + "$");
         if (OS.Path.basename(entry).match(rx)) {
           throw new Task.Result(entry);
         }
       }
       throw new Task.Result(null);
    }.bind(this));
  },

  /**
   * Serializes bookmarks using JSON, and writes to the supplied file.
   * Note: any item that should not be backed up must be annotated with
   *       "places/excludeFromBackup".
   *
   * @param aFile
   *        nsIFile where to save JSON backup.
   * @return {Promise}
   * @resolves the number of serialized uri nodes.
   */
  saveBookmarksToJSONFile: function PB_saveBookmarksToJSONFile(aFile) {
    return Task.spawn(function() {
      let nodeCount = yield BookmarkJSONUtils.exportToFile(aFile);

      let backupFolderPath = yield this.getBackupFolder();
      if (aFile.parent.path == backupFolderPath) {
        // Update internal cache.
        this.entries.push(aFile);
        if (!this._backupFiles) {
          yield this.getBackupFiles();
        }
        this._backupFiles.push(aFile.path);
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

        if (backupFile) {
          yield OS.File.remove(backupFile, { ignoreAbsent: true });
        } else {
          let file = new FileUtils.File(newFilePath);

          // Update internal cache if we are not replacing an existing
          // backup file.
          this.entries.push(file);
          if (!this._backupFiles) {
            yield this.getBackupFiles();
          }
          this._backupFiles.push(file.path);
        }
        yield OS.File.copy(aFile.path, newFilePath);
      }

      throw new Task.Result(nodeCount);
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
    return Task.spawn(function() {
      // Construct the new leafname.
      let newBackupFilename = this.getFilenameForDate();
      let mostRecentBackupFile = yield this.getMostRecentBackup();

      if (!aForceBackup) {
        let numberOfBackupsToDelete = 0;
        if (aMaxBackups !== undefined && aMaxBackups > -1) {
          let backupFiles = yield this.getBackupFiles();
          numberOfBackupsToDelete = backupFiles.length - aMaxBackups;
        }

        if (numberOfBackupsToDelete > 0) {
          // If we don't have today's backup, remove one more so that
          // the total backups after this operation does not exceed the
          // number specified in the pref.
          if (!mostRecentBackupFile ||
              !this._isFilenameWithSameDate(OS.Path.basename(mostRecentBackupFile),
                                            newBackupFilename))
            numberOfBackupsToDelete++;

          while (numberOfBackupsToDelete--) {
            this.entries.pop();
            if (!this._backupFiles) {
              yield this.getBackupFiles();
            }
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
      let backupFolderPath = yield this.getBackupFolder();
      let backupFolder = new FileUtils.File(backupFolderPath);
      let newBackupFile = backupFolder.clone();
      newBackupFile.append(newBackupFilename);

      let nodeCount = yield this.saveBookmarksToJSONFile(newBackupFile);
      // Rename the filename with metadata.
      let newFilenameWithMetaData = this._appendMetaDataToFilename(
                                      newBackupFilename,
                                      { nodeCount: nodeCount });
      newBackupFile.moveTo(backupFolder, newFilenameWithMetaData);

      // Update internal cache.
      let newFileWithMetaData = backupFolder.clone();
      newFileWithMetaData.append(newFilenameWithMetaData);
      this.entries.pop();
      this.entries.push(newFileWithMetaData);
      this._backupFiles.pop();
      this._backupFiles.push(newFileWithMetaData.path);
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
     return Task.spawn(function() {
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

       throw new Task.Result(backupFile);
     }.bind(this));
   }
}
