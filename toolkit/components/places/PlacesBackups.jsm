/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["PlacesBackups"];

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/BookmarkJSONUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");

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
      new RegExp("^(bookmarks|" + localizedFilenamePrefix + ")-([0-9-]+)\.(json|html)");
  },

  get folder() {
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

  get profileRelativeFolderPath() "bookmarkbackups",

  /**
   * Cache current backups in a sorted (by date DESC) array.
   */
  get entries() {
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
   *        nsIFile of the backup.
   * @return A Date object for the backup's creation time.
   */
  getDateForFile: function PB_getDateForFile(aBackupFile) {
    let filename = aBackupFile.leafName;
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
    let fileExt = aFileExt || "(json|html)";
    for (let i = 0; i < this.entries.length; i++) {
      let rx = new RegExp("\." + fileExt + "$");
      if (this.entries[i].leafName.match(rx))
        return this.entries[i];
    }
    return null;
  },

  /**
   * Serializes bookmarks using JSON, and writes to the supplied file.
   * Note: any item that should not be backed up must be annotated with
   *       "places/excludeFromBackup".
   *
   * @param aFile
   *        nsIFile where to save JSON backup.
   * @return {Promise}
   */
  saveBookmarksToJSONFile: function PB_saveBookmarksToJSONFile(aFile) {
    return Task.spawn(function() {
      if (!aFile.exists())
        aFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0600", 8));
      if (!aFile.exists() || !aFile.isWritable()) {
        throw new Error("Unable to create bookmarks backup file: " + aFile.leafName);
      }

      yield BookmarkJSONUtils.exportToFile(aFile);

      if (aFile.parent.equals(this.folder)) {
        // Update internal cache.
        this.entries.push(aFile);
      } else {
        // If we are saving to a folder different than our backups folder, then
        // we also want to copy this new backup to it.
        // This way we ensure the latest valid backup is the same saved by the
        // user.  See bug 424389.
        let latestBackup = this.getMostRecent("json");
        if (!latestBackup || latestBackup != aFile) {
          let name = this.getFilenameForDate();
          let file = this.folder.clone();
          file.append(name);
          if (file.exists()) {
            file.remove(false);
          } else {
            // Update internal cache if we are not replacing an existing
            // backup file.
            this.entries.push(file);
          }
          aFile.copyTo(this.folder, name);
        }
      }
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
      let mostRecentBackupFile = this.getMostRecent();

      if (!aForceBackup) {
        let numberOfBackupsToDelete = 0;
        if (aMaxBackups !== undefined && aMaxBackups > -1)
          numberOfBackupsToDelete = this.entries.length - aMaxBackups;

        if (numberOfBackupsToDelete > 0) {
          // If we don't have today's backup, remove one more so that
          // the total backups after this operation does not exceed the
          // number specified in the pref.
          if (!mostRecentBackupFile ||
              mostRecentBackupFile.leafName != newBackupFilename)
            numberOfBackupsToDelete++;

          while (numberOfBackupsToDelete--) {
            let oldestBackup = this.entries.pop();
            oldestBackup.remove(false);
          }
        }

        // Do nothing if we already have this backup or we don't want backups.
        if (aMaxBackups === 0 ||
            (mostRecentBackupFile &&
             mostRecentBackupFile.leafName == newBackupFilename))
          return;
      }

      let newBackupFile = this.folder.clone();
      newBackupFile.append(newBackupFilename);

      if (aForceBackup && newBackupFile.exists())
        newBackupFile.remove(false);

      if (newBackupFile.exists())
        return;

      yield this.saveBookmarksToJSONFile(newBackupFile);
    }.bind(this));
  }
}
