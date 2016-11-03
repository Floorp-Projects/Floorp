/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "DownloadImport",
];

// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm")
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
                                  "resource://gre/modules/Sqlite.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

/**
 * These values come from the previous interface
 * nsIDownloadManager, which has now been deprecated.
 * These are the only types of download states that
 * we will import.
 */
const DOWNLOAD_NOTSTARTED = -1;
const DOWNLOAD_DOWNLOADING = 0;
const DOWNLOAD_PAUSED = 4;
const DOWNLOAD_QUEUED = 5;

// DownloadImport

/**
 * Provides an object that has a method to import downloads
 * from the previous SQLite storage format.
 *
 * @param aList   A DownloadList where each successfully
 *                imported download will be added.
 * @param aPath   The path to the database file.
 */
this.DownloadImport = function (aList, aPath)
{
  this.list = aList;
  this.path = aPath;
}

this.DownloadImport.prototype = {
  /**
   * Imports unfinished downloads from the previous SQLite storage
   * format (supporting schemas 7 and up), to the new Download object
   * format. Each imported download will be added to the DownloadList
   *
   * @return {Promise}
   * @resolves When the operation has completed (i.e., every download
   *           from the previous database has been read and added to
   *           the DownloadList)
   */
  import: function () {
    return Task.spawn(function* task_DI_import() {
      let connection = yield Sqlite.openConnection({ path: this.path });

      try {
        let schemaVersion = yield connection.getSchemaVersion();
        // We don't support schemas older than version 7 (from 2007)
        // - Version 7 added the columns mimeType, preferredApplication
        //   and preferredAction in 2007
        // - Version 8 added the column autoResume in 2007
        //   (if we encounter version 7 we will treat autoResume = false)
        // - Version 9 is the last known version, which added a unique
        //   GUID text column that is not used here
        if (schemaVersion < 7) {
          throw new Error("Unable to import in-progress downloads because "
                          + "the existing profile is too old.");
        }

        let rows = yield connection.execute("SELECT * FROM moz_downloads");

        for (let row of rows) {
          try {
            // Get the DB row data
            let source = row.getResultByName("source");
            let target = row.getResultByName("target");
            let tempPath = row.getResultByName("tempPath");
            let startTime = row.getResultByName("startTime");
            let state = row.getResultByName("state");
            let referrer = row.getResultByName("referrer");
            let maxBytes = row.getResultByName("maxBytes");
            let mimeType = row.getResultByName("mimeType");
            let preferredApplication = row.getResultByName("preferredApplication");
            let preferredAction = row.getResultByName("preferredAction");
            let entityID = row.getResultByName("entityID");

            let autoResume = false;
            try {
              autoResume = (row.getResultByName("autoResume") == 1);
            } catch (ex) {
              // autoResume wasn't present in schema version 7
            }

            if (!source) {
              throw new Error("Attempted to import a row with an empty " +
                              "source column.");
            }

            let resumeDownload = false;

            switch (state) {
              case DOWNLOAD_NOTSTARTED:
              case DOWNLOAD_QUEUED:
              case DOWNLOAD_DOWNLOADING:
                resumeDownload = true;
                break;

              case DOWNLOAD_PAUSED:
                resumeDownload = autoResume;
                break;

              default:
                // We won't import downloads in other states
                continue;
            }

            // Transform the data
            let targetPath = NetUtil.newURI(target)
                                    .QueryInterface(Ci.nsIFileURL).file.path;

            let launchWhenSucceeded = (preferredAction != Ci.nsIMIMEInfo.saveToDisk);

            let downloadOptions = {
              source: {
                url: source,
                referrer: referrer
              },
              target: {
                path: targetPath,
                partFilePath: tempPath,
              },
              saver: {
                type: "copy",
                entityID: entityID
              },
              startTime: new Date(startTime / 1000),
              totalBytes: maxBytes,
              hasPartialData: !!tempPath,
              tryToKeepPartialData: true,
              launchWhenSucceeded: launchWhenSucceeded,
              contentType: mimeType,
              launcherPath: preferredApplication
            };

            // Paused downloads that should not be auto-resumed are considered
            // in a "canceled" state.
            if (!resumeDownload) {
              downloadOptions.canceled = true;
            }

            let download = yield Downloads.createDownload(downloadOptions);

            yield this.list.add(download);

            if (resumeDownload) {
              download.start().catch(() => {});
            } else {
              yield download.refresh();
            }

          } catch (ex) {
            Cu.reportError("Error importing download: " + ex);
          }
        }

      } catch (ex) {
        Cu.reportError(ex);
      } finally {
        yield connection.close();
      }
    }.bind(this));
  }
}

