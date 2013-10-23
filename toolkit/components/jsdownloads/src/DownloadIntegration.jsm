/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides functions to integrate with the host application, handling for
 * example the global prompts on shutdown.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "DownloadIntegration",
];

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadStore",
                                  "resource://gre/modules/DownloadStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadImport",
                                  "resource://gre/modules/DownloadImport.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadUIHelper",
                                  "resource://gre/modules/DownloadUIHelper.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/commonjs/sdk/core/promise.js");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gDownloadPlatform",
                                   "@mozilla.org/toolkit/download-platform;1",
                                   "mozIDownloadPlatform");
XPCOMUtils.defineLazyServiceGetter(this, "gEnvironment",
                                   "@mozilla.org/process/environment;1",
                                   "nsIEnvironment");
XPCOMUtils.defineLazyServiceGetter(this, "gMIMEService",
                                   "@mozilla.org/mime;1",
                                   "nsIMIMEService");
XPCOMUtils.defineLazyServiceGetter(this, "gExternalProtocolService",
                                   "@mozilla.org/uriloader/external-protocol-service;1",
                                   "nsIExternalProtocolService");

XPCOMUtils.defineLazyGetter(this, "gParentalControlsService", function() {
  if ("@mozilla.org/parental-controls-service;1" in Cc) {
    return Cc["@mozilla.org/parental-controls-service;1"]
      .createInstance(Ci.nsIParentalControlsService);
  }
  return null;
});

/**
 * ArrayBufferView representing the bytes to be written to the "Zone.Identifier"
 * Alternate Data Stream to mark a file as coming from the Internet zone.
 */
XPCOMUtils.defineLazyGetter(this, "gInternetZoneIdentifier", function() {
  return new TextEncoder().encode("[ZoneTransfer]\r\nZoneId=3\r\n");
});

const Timer = Components.Constructor("@mozilla.org/timer;1", "nsITimer",
                                     "initWithCallback");

/**
 * Indicates the delay between a change to the downloads data and the related
 * save operation.  This value is the result of a delicate trade-off, assuming
 * the host application uses the browser history instead of the download store
 * to save completed downloads.
 *
 * If a download takes less than this interval to complete (for example, saving
 * a page that is already displayed), then no input/output is triggered by the
 * download store except for an existence check, resulting in the best possible
 * efficiency.
 *
 * Conversely, if the browser is closed before this interval has passed, the
 * download will not be saved.  This prevents it from being restored in the next
 * session, and if there is partial data associated with it, then the ".part"
 * file will not be deleted when the browser starts again.
 *
 * In all cases, for best efficiency, this value should be high enough that the
 * input/output for opening or closing the target file does not overlap with the
 * one for saving the list of downloads.
 */
const kSaveDelayMs = 1500;

/**
 * This pref indicates if we have already imported (or attempted to import)
 * the downloads database from the previous SQLite storage.
 */
const kPrefImportedFromSqlite = "browser.download.importedFromSqlite";

////////////////////////////////////////////////////////////////////////////////
//// DownloadIntegration

/**
 * Provides functions to integrate with the host application, handling for
 * example the global prompts on shutdown.
 */
this.DownloadIntegration = {
  // For testing only
  _testMode: false,
  testPromptDownloads: 0,
  dontLoadList: false,
  dontLoadObservers: false,
  dontCheckParentalControls: false,
  shouldBlockInTest: false,
  dontOpenFileAndFolder: false,
  downloadDoneCalled: false,
  _deferTestOpenFile: null,
  _deferTestShowDir: null,
  _deferTestClearPrivateList: null,

  /**
   * Main DownloadStore object for loading and saving the list of persistent
   * downloads, or null if the download list was never requested and thus it
   * doesn't need to be persisted.
   */
  _store: null,

  /**
   * Gets and sets test mode
   */
  get testMode() this._testMode,
  set testMode(mode) {
    this._downloadsDirectory = null;
    return (this._testMode = mode);
  },

  /**
   * Performs initialization of the list of persistent downloads, before its
   * first use by the host application.  This function may be called only once
   * during the entire lifetime of the application.
   *
   * @param aList
   *        DownloadList object to be populated with the download objects
   *        serialized from the previous session.  This list will be persisted
   *        to disk during the session lifetime.
   *
   * @return {Promise}
   * @resolves When the list has been populated.
   * @rejects JavaScript exception.
   */
  initializePublicDownloadList: function(aList) {
    return Task.spawn(function task_DI_initializePublicDownloadList() {
      if (this.dontLoadList) {
        // In tests, only register the history observer.  This object is kept
        // alive by the history service, so we don't keep a reference to it.
        new DownloadHistoryObserver(aList);
        return;
      }

      if (this._store) {
        throw new Error("initializePublicDownloadList may be called only once.");
      }

      this._store = new DownloadStore(aList, OS.Path.join(
                                                OS.Constants.Path.profileDir,
                                                "downloads.json"));
      this._store.onsaveitem = this.shouldPersistDownload.bind(this);

      if (this._importedFromSqlite) {
        try {
          yield this._store.load();
        } catch (ex) {
          Cu.reportError(ex);
        }
      } else {
        let sqliteDBpath = OS.Path.join(OS.Constants.Path.profileDir,
                                        "downloads.sqlite");

        if (yield OS.File.exists(sqliteDBpath)) {
          let sqliteImport = new DownloadImport(aList, sqliteDBpath);
          yield sqliteImport.import();

          let importCount = (yield aList.getAll()).length;
          if (importCount > 0) {
            try {
              yield this._store.save();
            } catch (ex) { }
          }

          // No need to wait for the file removal.
          OS.File.remove(sqliteDBpath).then(null, Cu.reportError);
        }

        Services.prefs.setBoolPref(kPrefImportedFromSqlite, true);

        // Don't even report error here because this file is pre Firefox 3
        // and most likely doesn't exist.
        OS.File.remove(OS.Path.join(OS.Constants.Path.profileDir,
                                    "downloads.rdf"));

      }

      // After the list of persistent downloads has been loaded, add the
      // DownloadAutoSaveView and the DownloadHistoryObserver (even if the load
      // operation failed).  These objects are kept alive by the underlying
      // DownloadList and by the history service respectively.  We wait for a
      // complete initialization of the view used for detecting changes to
      // downloads to be persisted, before other callers get a chance to modify
      // the list without being detected.
      yield new DownloadAutoSaveView(aList, this._store).initialize();
      new DownloadHistoryObserver(aList);
    }.bind(this));
  },

  /**
   * Determines if a Download object from the list of persistent downloads
   * should be saved into a file, so that it can be restored across sessions.
   *
   * This function allows filtering out downloads that the host application is
   * not interested in persisting across sessions, for example downloads that
   * finished successfully.
   *
   * @param aDownload
   *        The Download object to be inspected.  This is originally taken from
   *        the global DownloadList object for downloads that were not started
   *        from a private browsing window.  The item may have been removed
   *        from the list since the save operation started, though in this case
   *        the save operation will be repeated later.
   *
   * @return True to save the download, false otherwise.
   */
  shouldPersistDownload: function (aDownload)
  {
    // In the default implementation, we save all the downloads currently in
    // progress, as well as stopped downloads for which we retained partially
    // downloaded data.  Stopped downloads for which we don't need to track the
    // presence of a ".part" file are only retained in the browser history.
    return aDownload.hasPartialData || !aDownload.stopped;
  },

  /**
   * Returns the system downloads directory asynchronously.
   *
   * @return {Promise}
   * @resolves The downloads directory string path.
   */
  getSystemDownloadsDirectory: function DI_getSystemDownloadsDirectory() {
    return Task.spawn(function() {
      if (this._downloadsDirectory) {
        // This explicitly makes this function a generator for Task.jsm. We
        // need this because calls to the "yield" operator below may be
        // preprocessed out on some platforms.
        yield undefined;
        throw new Task.Result(this._downloadsDirectory);
      }

      let directoryPath = null;
#ifdef XP_MACOSX
      directoryPath = this._getDirectory("DfltDwnld");
#elifdef XP_WIN
      // For XP/2K, use My Documents/Downloads. Other version uses
      // the default Downloads directory.
      let version = parseFloat(Services.sysinfo.getProperty("version"));
      if (version < 6) {
        directoryPath = yield this._createDownloadsDirectory("Pers");
      } else {
        directoryPath = this._getDirectory("DfltDwnld");
      }
#elifdef XP_UNIX
#ifdef ANDROID
      // Android doesn't have a $HOME directory, and by default we only have
      // write access to /data/data/org.mozilla.{$APP} and /sdcard
      directoryPath = gEnvironment.get("DOWNLOADS_DIRECTORY");
      if (!directoryPath) {
        throw new Components.Exception("DOWNLOADS_DIRECTORY is not set.",
                                       Cr.NS_ERROR_FILE_UNRECOGNIZED_PATH);
      }
#else
      // For Linux, use XDG download dir, with a fallback to Home/Downloads
      // if the XDG user dirs are disabled.
      try {
        directoryPath = this._getDirectory("DfltDwnld");
      } catch(e) {
        directoryPath = yield this._createDownloadsDirectory("Home");
      }
#endif
#else
      directoryPath = yield this._createDownloadsDirectory("Home");
#endif
      this._downloadsDirectory = directoryPath;
      throw new Task.Result(this._downloadsDirectory);
    }.bind(this));
  },
  _downloadsDirectory: null,

  /**
   * Returns the user downloads directory asynchronously.
   *
   * @return {Promise}
   * @resolves The downloads directory string path.
   */
  getPreferredDownloadsDirectory: function DI_getPreferredDownloadsDirectory() {
    return Task.spawn(function() {
      let directoryPath = null;
      let prefValue = 1;

      try {
        prefValue = Services.prefs.getIntPref("browser.download.folderList");
      } catch(e) {}

      switch(prefValue) {
        case 0: // Desktop
          directoryPath = this._getDirectory("Desk");
          break;
        case 1: // Downloads
          directoryPath = yield this.getSystemDownloadsDirectory();
          break;
        case 2: // Custom
          try {
            let directory = Services.prefs.getComplexValue("browser.download.dir",
                                                           Ci.nsIFile);
            directoryPath = directory.path;
            yield OS.File.makeDir(directoryPath, { ignoreExisting: true });
          } catch(ex) {
            // Either the preference isn't set or the directory cannot be created.
            directoryPath = yield this.getSystemDownloadsDirectory();
          }
          break;
        default:
          directoryPath = yield this.getSystemDownloadsDirectory();
      }
      throw new Task.Result(directoryPath);
    }.bind(this));
  },

  /**
   * Returns the temporary downloads directory asynchronously.
   *
   * @return {Promise}
   * @resolves The downloads directory string path.
   */
  getTemporaryDownloadsDirectory: function DI_getTemporaryDownloadsDirectory() {
    return Task.spawn(function() {
      let directoryPath = null;
#ifdef XP_MACOSX
      directoryPath = yield this.getPreferredDownloadsDirectory();
#elifdef ANDROID
      directoryPath = yield this.getSystemDownloadsDirectory();
#else
      // For Metro mode on Windows 8,  we want searchability for documents
      // that the user chose to open with an external application.
      if (this._isImmersiveProcess()) {
        directoryPath = yield this.getSystemDownloadsDirectory();
      } else {
        directoryPath = this._getDirectory("TmpD");
      }
#endif
      throw new Task.Result(directoryPath);
    }.bind(this));
  },

  /**
   * Checks to determine whether to block downloads for parental controls.
   *
   * aParam aDownload
   *        The download object.
   *
   * @return {Promise}
   * @resolves The boolean indicates to block downloads or not.
   */
  shouldBlockForParentalControls: function DI_shouldBlockForParentalControls(aDownload) {
    if (this.dontCheckParentalControls) {
      return Promise.resolve(this.shouldBlockInTest);
    }

    let isEnabled = gParentalControlsService &&
                    gParentalControlsService.parentalControlsEnabled;
    let shouldBlock = isEnabled &&
                      gParentalControlsService.blockFileDownloadsEnabled;

    // Log the event if required by parental controls settings.
    if (isEnabled && gParentalControlsService.loggingEnabled) {
      gParentalControlsService.log(gParentalControlsService.ePCLog_FileDownload,
                                   shouldBlock,
                                   NetUtil.newURI(aDownload.source.url), null);
    }

    return Promise.resolve(shouldBlock);
  },

  /**
   * Performs platform-specific operations when a download is done.
   *
   * aParam aDownload
   *        The Download object.
   *
   * @return {Promise}
   * @resolves When all the operations completed successfully.
   * @rejects JavaScript exception if any of the operations failed.
   */
  downloadDone: function(aDownload) {
    return Task.spawn(function () {
#ifdef XP_WIN
      // On Windows, we mark any executable file saved to the NTFS file system
      // as coming from the Internet security zone.  We do this by writing to
      // the "Zone.Identifier" Alternate Data Stream directly, because the Save
      // method of the IAttachmentExecute interface would trigger operations
      // that may cause the application to hang, or other performance issues.
      // The stream created in this way is forward-compatible with all the
      // current and future versions of Windows.
      if (Services.prefs.getBoolPref("browser.download.saveZoneInformation")) {
        let file = new FileUtils.File(aDownload.target.path);
        if (file.isExecutable()) {
          try {
            let streamPath = aDownload.target.path + ":Zone.Identifier";
            let stream = yield OS.File.open(streamPath, { create: true });
            try {
              yield stream.write(gInternetZoneIdentifier);
            } finally {
              yield stream.close();
            }
          } catch (ex) {
            // If writing to the stream fails, we ignore the error and continue.
            // The Windows API error 123 (ERROR_INVALID_NAME) is expected to
            // occur when working on a file system that does not support
            // Alternate Data Streams, like FAT32, thus we don't report this
            // specific error.
            if (!(ex instanceof OS.File.Error) || ex.winLastError != 123) {
              Cu.reportError(ex);
            }
          }
        }
      }
#endif

      gDownloadPlatform.downloadDone(NetUtil.newURI(aDownload.source.url),
                                     new FileUtils.File(aDownload.target.path),
                                     aDownload.contentType,
                                     aDownload.source.isPrivate);
      this.downloadDoneCalled = true;
    }.bind(this));
  },

  /**
   * Determines whether it's a Windows Metro app.
   */
  _isImmersiveProcess: function() {
    // TODO: to be implemented
    return false;
  },

  /*
   * Launches a file represented by the target of a download. This can
   * open the file with the default application for the target MIME type
   * or file extension, or with a custom application if
   * aDownload.launcherPath is set.
   *
   * @param    aDownload
   *           A Download object that contains the necessary information
   *           to launch the file. The relevant properties are: the target
   *           file, the contentType and the custom application chosen
   *           to launch it.
   *
   * @return {Promise}
   * @resolves When the instruction to launch the file has been
   *           successfully given to the operating system. Note that
   *           the OS might still take a while until the file is actually
   *           launched.
   * @rejects  JavaScript exception if there was an error trying to launch
   *           the file.
   */
  launchDownload: function (aDownload) {
    let deferred = Task.spawn(function DI_launchDownload_task() {
      let file = new FileUtils.File(aDownload.target.path);

#ifndef XP_WIN
      // Ask for confirmation if the file is executable, except on Windows where
      // the operating system will show the prompt based on the security zone.
      // We do this here, instead of letting the caller handle the prompt
      // separately in the user interface layer, for two reasons.  The first is
      // because of its security nature, so that add-ons cannot forget to do
      // this check.  The second is that the system-level security prompt would
      // be displayed at launch time in any case.
      if (file.isExecutable() && !this.dontOpenFileAndFolder) {
        // We don't anchor the prompt to a specific window intentionally, not
        // only because this is the same behavior as the system-level prompt,
        // but also because the most recently active window is the right choice
        // in basically all cases.
        let shouldLaunch = yield DownloadUIHelper.getPrompter()
                                   .confirmLaunchExecutable(file.path);
        if (!shouldLaunch) {
          return;
        }
      }
#endif

      // In case of a double extension, like ".tar.gz", we only
      // consider the last one, because the MIME service cannot
      // handle multiple extensions.
      let fileExtension = null, mimeInfo = null;
      let match = file.leafName.match(/\.([^.]+)$/);
      if (match) {
        fileExtension = match[1];
      }

      try {
        // The MIME service might throw if contentType == "" and it can't find
        // a MIME type for the given extension, so we'll treat this case as
        // an unknown mimetype.
        mimeInfo = gMIMEService.getFromTypeAndExtension(aDownload.contentType,
                                                        fileExtension);
      } catch (e) { }

      if (aDownload.launcherPath) {
        if (!mimeInfo) {
          // This should not happen on normal circumstances because launcherPath
          // is only set when we had an instance of nsIMIMEInfo to retrieve
          // the custom application chosen by the user.
          throw new Error(
            "Unable to create nsIMIMEInfo to launch a custom application");
        }

        // Custom application chosen
        let localHandlerApp = Cc["@mozilla.org/uriloader/local-handler-app;1"]
                                .createInstance(Ci.nsILocalHandlerApp);
        localHandlerApp.executable = new FileUtils.File(aDownload.launcherPath);

        mimeInfo.preferredApplicationHandler = localHandlerApp;
        mimeInfo.preferredAction = Ci.nsIMIMEInfo.useHelperApp;

        // In test mode, allow the test to verify the nsIMIMEInfo instance.
        if (this.dontOpenFileAndFolder) {
          throw new Task.Result(mimeInfo);
        }

        mimeInfo.launchWithFile(file);
        return;
      }

      // No custom application chosen, let's launch the file with the default
      // handler.  In test mode, we indicate this with a null value.
      if (this.dontOpenFileAndFolder) {
        throw new Task.Result(null);
      }

      // First let's try to launch it through the MIME service application
      // handler
      if (mimeInfo) {
        mimeInfo.preferredAction = Ci.nsIMIMEInfo.useSystemDefault;

        try {
          mimeInfo.launchWithFile(file);
          return;
        } catch (ex) { }
      }

      // If it didn't work or if there was no MIME info available,
      // let's try to directly launch the file.
      try {
        file.launch();
        return;
      } catch (ex) { }

      // If our previous attempts failed, try sending it through
      // the system's external "file:" URL handler.
      gExternalProtocolService.loadUrl(NetUtil.newURI(file));
      yield undefined;
    }.bind(this));

    if (this.dontOpenFileAndFolder) {
      deferred.then((value) => { this._deferTestOpenFile.resolve(value); },
                    (error) => { this._deferTestOpenFile.reject(error); });
    }

    return deferred;
  },

  /*
   * Shows the containing folder of a file.
   *
   * @param    aFilePath
   *           The path to the file.
   *
   * @return {Promise}
   * @resolves When the instruction to open the containing folder has been
   *           successfully given to the operating system. Note that
   *           the OS might still take a while until the folder is actually
   *           opened.
   * @rejects  JavaScript exception if there was an error trying to open
   *           the containing folder.
   */
  showContainingDirectory: function (aFilePath) {
    let deferred = Task.spawn(function DI_showContainingDirectory_task() {
      let file = new FileUtils.File(aFilePath);

      if (this.dontOpenFileAndFolder) {
        return;
      }

      try {
        // Show the directory containing the file and select the file.
        file.reveal();
        return;
      } catch (ex) { }

      // If reveal fails for some reason (e.g., it's not implemented on unix
      // or the file doesn't exist), try using the parent if we have it.
      let parent = file.parent;
      if (!parent) {
        throw new Error(
          "Unexpected reference to a top-level directory instead of a file");
      }

      try {
        // Open the parent directory to show where the file should be.
        parent.launch();
        return;
      } catch (ex) { }

      // If launch also fails (probably because it's not implemented), let
      // the OS handler try to open the parent.
      gExternalProtocolService.loadUrl(NetUtil.newURI(parent));
      yield undefined;
    }.bind(this));

    if (this.dontOpenFileAndFolder) {
      deferred.then((value) => { this._deferTestShowDir.resolve("success"); },
                    (error) => { this._deferTestShowDir.reject(error); });
    }

    return deferred;
  },

  /**
   * Calls the directory service, create a downloads directory and returns an
   * nsIFile for the downloads directory.
   *
   * @return {Promise}
   * @resolves The directory string path.
   */
  _createDownloadsDirectory: function DI_createDownloadsDirectory(aName) {
    // We read the name of the directory from the list of translated strings
    // that is kept by the UI helper module, even if this string is not strictly
    // displayed in the user interface.
    let directoryPath = OS.Path.join(this._getDirectory(aName),
                                     DownloadUIHelper.strings.downloadsFolder);

    // Create the Downloads folder and ignore if it already exists.
    return OS.File.makeDir(directoryPath, { ignoreExisting: true }).
             then(function() {
               return directoryPath;
             });
  },

  /**
   * Calls the directory service and returns an nsIFile for the requested
   * location name.
   *
   * @return The directory string path.
   */
  _getDirectory: function DI_getDirectory(aName) {
    return Services.dirsvc.get(this.testMode ? "TmpD" : aName, Ci.nsIFile).path;
  },

  /**
   * Register the downloads interruption observers.
   *
   * @param aList
   *        The public or private downloads list.
   * @param aIsPrivate
   *        True if the list is private, false otherwise.
   *
   * @return {Promise}
   * @resolves When the views and observers are added.
   */
  addListObservers: function DI_addListObservers(aList, aIsPrivate) {
    if (this.dontLoadObservers) {
      return Promise.resolve();
    }

    DownloadObserver.registerView(aList, aIsPrivate);
    if (!DownloadObserver.observersAdded) {
      DownloadObserver.observersAdded = true;
      Services.obs.addObserver(DownloadObserver, "quit-application-requested", true);
      Services.obs.addObserver(DownloadObserver, "offline-requested", true);
      Services.obs.addObserver(DownloadObserver, "last-pb-context-exiting", true);
      Services.obs.addObserver(DownloadObserver, "last-pb-context-exited", true);

      Services.obs.addObserver(DownloadObserver, "sleep_notification", true);
      Services.obs.addObserver(DownloadObserver, "suspend_process_notification", true);
      Services.obs.addObserver(DownloadObserver, "wake_notification", true);
      Services.obs.addObserver(DownloadObserver, "resume_process_notification", true);
      Services.obs.addObserver(DownloadObserver, "network:offline-about-to-go-offline", true);
      Services.obs.addObserver(DownloadObserver, "network:offline-status-changed", true);
    }
    return Promise.resolve();
  },

  /**
   * Checks if we have already imported (or attempted to import)
   * the downloads database from the previous SQLite storage.
   *
   * @return boolean True if we the previous DB was imported.
   */
  get _importedFromSqlite() {
    try {
      return Services.prefs.getBoolPref(kPrefImportedFromSqlite);
    } catch (ex) {
      return false;
    }
  },
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadObserver

this.DownloadObserver = {
  /**
   * Flag to determine if the observers have been added previously.
   */
  observersAdded: false,

  /**
   * Timer used to delay restarting canceled downloads upon waking and returning
   * online.
   */
  _wakeTimer: null,

  /**
   * Set that contains the in progress publics downloads.
   * It's kept updated when a public download is added, removed or changes its
   * properties.
   */
  _publicInProgressDownloads: new Set(),

  /**
   * Set that contains the in progress private downloads.
   * It's kept updated when a private download is added, removed or changes its
   * properties.
   */
  _privateInProgressDownloads: new Set(),

  /**
   * Set that contains the downloads that have been canceled when going offline
   * or to sleep. These are started again when returning online or waking. This
   * list is not persisted so when exiting and restarting, the downloads will not
   * be started again.
   */
  _canceledOfflineDownloads: new Set(),

  /**
   * Registers a view that updates the corresponding downloads state set, based
   * on the aIsPrivate argument. The set is updated when a download is added,
   * removed or changes its properties.
   *
   * @param aList
   *        The public or private downloads list.
   * @param aIsPrivate
   *        True if the list is private, false otherwise.
   */
  registerView: function DO_registerView(aList, aIsPrivate) {
    let downloadsSet = aIsPrivate ? this._privateInProgressDownloads
                                  : this._publicInProgressDownloads;
    let downloadsView = {
      onDownloadAdded: aDownload => {
        if (!aDownload.stopped) {
          downloadsSet.add(aDownload);
        }
      },
      onDownloadChanged: aDownload => {
        if (aDownload.stopped) {
          downloadsSet.delete(aDownload);
        } else {
          downloadsSet.add(aDownload);
        }
      },
      onDownloadRemoved: aDownload => {
        downloadsSet.delete(aDownload);
        // The download must also be removed from the canceled when offline set.
        this._canceledOfflineDownloads.delete(aDownload);
      }
    };

    // We register the view asynchronously.
    aList.addView(downloadsView).then(null, Cu.reportError);
  },

  /**
   * Wrapper that handles the test mode before calling the prompt that display
   * a warning message box that informs that there are active downloads,
   * and asks whether the user wants to cancel them or not.
   *
   * @param aCancel
   *        The observer notification subject.
   * @param aDownloadsCount
   *        The current downloads count.
   * @param aPrompter
   *        The prompter object that shows the confirm dialog.
   * @param aPromptType
   *        The type of prompt notification depending on the observer.
   */
  _confirmCancelDownloads: function DO_confirmCancelDownload(
    aCancel, aDownloadsCount, aPrompter, aPromptType) {
    // If user has already dismissed the request, then do nothing.
    if ((aCancel instanceof Ci.nsISupportsPRBool) && aCancel.data) {
      return;
    }
    // Handle test mode
    if (DownloadIntegration.testMode) {
      DownloadIntegration.testPromptDownloads = aDownloadsCount;
      return;
    }

    aCancel.data = aPrompter.confirmCancelDownloads(aDownloadsCount, aPromptType);
  },

  /**
   * Resume all downloads that were paused when going offline, used when waking
   * from sleep or returning from being offline.
   */
  _resumeOfflineDownloads: function DO_resumeOfflineDownloads() {
    this._wakeTimer = null;

    for (let download of this._canceledOfflineDownloads) {
      download.start();
    }
  },

  ////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function DO_observe(aSubject, aTopic, aData) {
    let downloadsCount;
    let p = DownloadUIHelper.getPrompter();
    switch (aTopic) {
      case "quit-application-requested":
        downloadsCount = this._publicInProgressDownloads.size +
                         this._privateInProgressDownloads.size;
        this._confirmCancelDownloads(aSubject, downloadsCount, p, p.ON_QUIT);
        break;
      case "offline-requested":
        downloadsCount = this._publicInProgressDownloads.size +
                         this._privateInProgressDownloads.size;
        this._confirmCancelDownloads(aSubject, downloadsCount, p, p.ON_OFFLINE);
        break;
      case "last-pb-context-exiting":
        downloadsCount = this._privateInProgressDownloads.size;
        this._confirmCancelDownloads(aSubject, downloadsCount, p,
                                     p.ON_LEAVE_PRIVATE_BROWSING);
        break;
      case "last-pb-context-exited":
        let deferred = Task.spawn(function() {
          let list = yield Downloads.getList(Downloads.PRIVATE);
          let downloads = yield list.getAll();

          // We can remove the downloads and finalize them in parallel.
          for (let download of downloads) {
            list.remove(download).then(null, Cu.reportError);
            download.finalize(true).then(null, Cu.reportError);
          }
        });
        // Handle test mode
        if (DownloadIntegration.testMode) {
          deferred.then((value) => { DownloadIntegration._deferTestClearPrivateList.resolve("success"); },
                        (error) => { DownloadIntegration._deferTestClearPrivateList.reject(error); });
        }
        break;
      case "sleep_notification":
      case "suspend_process_notification":
      case "network:offline-about-to-go-offline":
        for (let download of this._publicInProgressDownloads) {
          download.cancel();
          this._canceledOfflineDownloads.add(download);
        }
        for (let download of this._privateInProgressDownloads) {
          download.cancel();
          this._canceledOfflineDownloads.add(download);
        }
        break;
      case "wake_notification":
      case "resume_process_notification":
        let wakeDelay = 10000;
        try {
          wakeDelay = Services.prefs.getIntPref("browser.download.manager.resumeOnWakeDelay");
        } catch(e) {}

        if (wakeDelay >= 0) {
          this._wakeTimer = new Timer(this._resumeOfflineDownloads.bind(this), wakeDelay,
                                      Ci.nsITimer.TYPE_ONE_SHOT);
        }
        break;
      case "network:offline-status-changed":
        if (aData == "online") {
          this._resumeOfflineDownloads();
        }
        break;
    }
  },

  ////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadHistoryObserver

/**
 * Registers a Places observer so that operations on download history are
 * reflected on the provided list of downloads.
 *
 * You do not need to keep a reference to this object in order to keep it alive,
 * because the history service already keeps a strong reference to it.
 *
 * @param aList
 *        DownloadList object linked to this observer.
 */
function DownloadHistoryObserver(aList)
{
  this._list = aList;
  PlacesUtils.history.addObserver(this, false);
}

DownloadHistoryObserver.prototype = {
  /**
   * DownloadList object linked to this observer.
   */
  _list: null,

  ////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver]),

  ////////////////////////////////////////////////////////////////////////////
  //// nsINavHistoryObserver

  onDeleteURI: function DL_onDeleteURI(aURI, aGUID) {
    this._list.removeFinished(download => aURI.equals(NetUtil.newURI(
                                                      download.source.url)));
  },

  onClearHistory: function DL_onClearHistory() {
    this._list.removeFinished();
  },

  onTitleChanged: function () {},
  onBeginUpdateBatch: function () {},
  onEndUpdateBatch: function () {},
  onVisit: function () {},
  onPageChanged: function () {},
  onDeleteVisits: function () {},
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadAutoSaveView

/**
 * This view can be added to a DownloadList object to trigger a save operation
 * in the given DownloadStore object when a relevant change occurs.  You should
 * call the "initialize" method in order to register the view and load the
 * current state from disk.
 *
 * You do not need to keep a reference to this object in order to keep it alive,
 * because the DownloadList object already keeps a strong reference to it.
 *
 * @param aList
 *        The DownloadList object on which the view should be registered.
 * @param aStore
 *        The DownloadStore object used for saving.
 */
function DownloadAutoSaveView(aList, aStore) {
  this._list = aList;
  this._store = aStore;
  this._downloadsMap = new Map();
}

DownloadAutoSaveView.prototype = {
  /**
   * DownloadList object linked to this view.
   */
  _list: null,

  /**
   * The DownloadStore object used for saving.
   */
  _store: null,

  /**
   * True when the initial state of the downloads has been loaded.
   */
  _initialized: false,

  /**
   * Registers the view and loads the current state from disk.
   *
   * @return {Promise}
   * @resolves When the view has been registered.
   * @rejects JavaScript exception.
   */
  initialize: function ()
  {
    // We set _initialized to true after adding the view, so that
    // onDownloadAdded doesn't cause a save to occur.
    return this._list.addView(this).then(() => this._initialized = true);
  },

  /**
   * This map contains only Download objects that should be saved to disk, and
   * associates them with the result of their getSerializationHash function, for
   * the purpose of detecting changes to the relevant properties.
   */
  _downloadsMap: null,

  /**
   * This is set to true when the save operation should be triggered.  This is
   * required so that a new operation can be scheduled while the current one is
   * in progress, without re-entering the save method.
   */
  _shouldSave: false,

  /**
   * nsITimer used for triggering the save operation after a delay, or null if
   * saving has finished and there is no operation scheduled for execution.
   *
   * The logic here is different from the DeferredTask module in that multiple
   * requests will never delay the operation for longer than the expected time
   * (no grace delay), and the operation is never re-entered during execution.
   */
  _timer: null,

  /**
   * Timer callback used to serialize the list of downloads.
   */
  _save: function ()
  {
    Task.spawn(function () {
      // Any save request received during execution will be handled later.
      this._shouldSave = false;

      // Execute the asynchronous save operation.
      try {
        yield this._store.save();
      } catch (ex) {
        Cu.reportError(ex);
      }

      // Handle requests received during the operation.
      this._timer = null;
      if (this._shouldSave) {
        this.saveSoon();
      }
    }.bind(this)).then(null, Cu.reportError);
  },

  /**
   * Called when the list of downloads changed, this triggers the asynchronous
   * serialization of the list of downloads.
   */
  saveSoon: function ()
  {
    this._shouldSave = true;
    if (!this._timer) {
      this._timer = new Timer(this._save.bind(this), kSaveDelayMs,
                              Ci.nsITimer.TYPE_ONE_SHOT);
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// DownloadList view

  onDownloadAdded: function (aDownload)
  {
    if (DownloadIntegration.shouldPersistDownload(aDownload)) {
      this._downloadsMap.set(aDownload, aDownload.getSerializationHash());
      if (this._initialized) {
        this.saveSoon();
      }
    }
  },

  onDownloadChanged: function (aDownload)
  {
    if (!DownloadIntegration.shouldPersistDownload(aDownload)) {
      if (this._downloadsMap.has(aDownload)) {
        this._downloadsMap.delete(aDownload);
        this.saveSoon();
      }
      return;
    }

    let hash = aDownload.getSerializationHash();
    if (this._downloadsMap.get(aDownload) != hash) {
      this._downloadsMap.set(aDownload, hash);
      this.saveSoon();
    }
  },

  onDownloadRemoved: function (aDownload)
  {
    if (this._downloadsMap.has(aDownload)) {
      this._downloadsMap.delete(aDownload);
      this.saveSoon();
    }
  },
};
