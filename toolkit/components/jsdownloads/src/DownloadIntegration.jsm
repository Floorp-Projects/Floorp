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

XPCOMUtils.defineLazyModuleGetter(this, "DownloadStore",
                                  "resource://gre/modules/DownloadStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/commonjs/sdk/core/promise.js");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
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

XPCOMUtils.defineLazyGetter(this, "gStringBundle", function() {
  return Services.strings.
    createBundle("chrome://mozapps/locale/downloads/downloads.properties");
});

////////////////////////////////////////////////////////////////////////////////
//// DownloadIntegration

/**
 * Provides functions to integrate with the host application, handling for
 * example the global prompts on shutdown.
 */
this.DownloadIntegration = {
  // For testing only
  _testMode: false,
  dontLoad: false,
  dontCheckParentalControls: false,
  shouldBlockInTest: false,
  dontOpenFileAndFolder: false,
  _deferTestOpenFile: null,
  _deferTestShowDir: null,

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
   *        to disk during the session lifetime or when the session terminates.
   *
   * @return {Promise}
   * @resolves When the list has been populated.
   * @rejects JavaScript exception.
   */
  loadPersistent: function DI_loadPersistent(aList)
  {
    if (this.dontLoad) {
      return Promise.resolve();
    }

    if (this._store) {
      throw new Error("loadPersistent may be called only once.");
    }

    this._store = new DownloadStore(aList, OS.Path.join(
                                              OS.Constants.Path.profileDir,
                                              "downloads.json"));
    return this._store.load();
  },

  /**
   * Returns the system downloads directory asynchronously.
   *
   * @return {Promise}
   * @resolves The nsIFile of downloads directory.
   */
  getSystemDownloadsDirectory: function DI_getSystemDownloadsDirectory() {
    return Task.spawn(function() {
      if (this._downloadsDirectory) {
        // This explicitly makes this function a generator for Task.jsm. We
        // need this because calls to the "yield" operator below may be
        // preprocessed out on some platforms.
        yield;
        throw new Task.Result(this._downloadsDirectory);
      }

      let directory = null;
#ifdef XP_MACOSX
      directory = this._getDirectory("DfltDwnld");
#elifdef XP_WIN
      // For XP/2K, use My Documents/Downloads. Other version uses
      // the default Downloads directory.
      let version = parseFloat(Services.sysinfo.getProperty("version"));
      if (version < 6) {
        directory = yield this._createDownloadsDirectory("Pers");
      } else {
        directory = this._getDirectory("DfltDwnld");
      }
#elifdef XP_UNIX
#ifdef MOZ_PLATFORM_MAEMO
      // As maemo does not follow the XDG "standard" (as usually desktop
      // Linux distros do) neither has a working $HOME/Desktop folder
      // for us to fallback into, "$HOME/MyDocs/.documents/" is the folder
      // we found most appropriate to be the default target folder for
      // downloads on the platform.
      directory = this._getDirectory("XDGDocs");
#elifdef ANDROID
      // Android doesn't have a $HOME directory, and by default we only have
      // write access to /data/data/org.mozilla.{$APP} and /sdcard
      let directoryPath = gEnvironment.get("DOWNLOADS_DIRECTORY");
      if (!directoryPath) {
        throw new Components.Exception("DOWNLOADS_DIRECTORY is not set.",
                                       Cr.NS_ERROR_FILE_UNRECOGNIZED_PATH);
      }
      directory = new FileUtils.File(directoryPath);
#else
      // For Linux, use XDG download dir, with a fallback to Home/Downloads
      // if the XDG user dirs are disabled.
      try {
        directory = this._getDirectory("DfltDwnld");
      } catch(e) {
        directory = yield this._createDownloadsDirectory("Home");
      }
#endif
#else
      directory = yield this._createDownloadsDirectory("Home");
#endif
      this._downloadsDirectory = directory;
      throw new Task.Result(this._downloadsDirectory);
    }.bind(this));
  },
  _downloadsDirectory: null,

  /**
   * Returns the user downloads directory asynchronously.
   *
   * @return {Promise}
   * @resolves The nsIFile of downloads directory.
   */
  getUserDownloadsDirectory: function DI_getUserDownloadsDirectory() {
    return Task.spawn(function() {
      let directory = null;
      let prefValue = 1;

      try {
        prefValue = Services.prefs.getIntPref("browser.download.folderList");
      } catch(e) {}

      switch(prefValue) {
        case 0: // Desktop
          directory = this._getDirectory("Desk");
          break;
        case 1: // Downloads
          directory = yield this.getSystemDownloadsDirectory();
          break;
        case 2: // Custom
          try {
            directory = Services.prefs.getComplexValue("browser.download.dir",
                                                       Ci.nsIFile);
            yield OS.File.makeDir(directory.path, { ignoreExisting: true });
          } catch(ex) {
            // Either the preference isn't set or the directory cannot be created.
            directory = yield this.getSystemDownloadsDirectory();
          }
          break;
        default:
          directory = yield this.getSystemDownloadsDirectory();
      }
      throw new Task.Result(directory);
    }.bind(this));
  },

  /**
   * Returns the temporary downloads directory asynchronously.
   *
   * @return {Promise}
   * @resolves The nsIFile of downloads directory.
   */
  getTemporaryDownloadsDirectory: function DI_getTemporaryDownloadsDirectory() {
    return Task.spawn(function() {
      let directory = null;
#ifdef XP_MACOSX
      directory = yield this.getUserDownloadsDirectory();
#elifdef ANDROID
      directory = yield this.getSystemDownloadsDirectory();
#else
      // For Metro mode on Windows 8,  we want searchability for documents
      // that the user chose to open with an external application.
      if (this._isImmersiveProcess()) {
        directory = yield this.getSystemDownloadsDirectory();
      } else {
        directory = this._getDirectory("TmpD");
      }
#endif
      throw new Task.Result(directory);
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
   * @resolves The nsIFile directory.
   */
  _createDownloadsDirectory: function DI_createDownloadsDirectory(aName) {
    let directory = this._getDirectory(aName);
    directory.append(gStringBundle.GetStringFromName("downloadsFolder"));

    // Create the Downloads folder and ignore if it already exists.
    return OS.File.makeDir(directory.path, { ignoreExisting: true }).
             then(function() {
               return directory;
             });
  },

  /**
   * Calls the directory service and returns an nsIFile for the requested
   * location name.
   *
   * @return The nsIFile directory.
   */
  _getDirectory: function DI_getDirectory(aName) {
    return Services.dirsvc.get(this.testMode ? "TmpD" : aName, Ci.nsIFile);
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
    DownloadObserver.registerView(aList, aIsPrivate);
    if (!DownloadObserver.observersAdded) {
      DownloadObserver.observersAdded = true;
      Services.obs.addObserver(DownloadObserver, "quit-application-requested", true);
      Services.obs.addObserver(DownloadObserver, "offline-requested", true);
      Services.obs.addObserver(DownloadObserver, "last-pb-context-exiting", true);
    }
    return Promise.resolve();
  }
};

let DownloadObserver = {
  /**
   * Flag to determine if the observers have been added previously.
   */
  observersAdded: false,

  /**
   * Set that contains the in progress publics downloads.
   * It's keep updated when a public download is added, removed or change its
   * properties.
   */
  _publicInProgressDownloads: new Set(),

  /**
   * Set that contains the in progress private downloads.
   * It's keep updated when a private download is added, removed or change its
   * properties.
   */
  _privateInProgressDownloads: new Set(),

  /**
   * Registers a view that updates the corresponding downloads state set, based
   * on the aIsPrivate argument. The set is updated when a download is added,
   * removed or changed its properties.
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
      onDownloadAdded: function DO_V_onDownloadAdded(aDownload) {
        if (!aDownload.stopped) {
          downloadsSet.add(aDownload);
        }
      },
      onDownloadChanged: function DO_V_onDownloadChanged(aDownload) {
        if (aDownload.stopped) {
          downloadsSet.delete(aDownload);
        } else {
          downloadsSet.add(aDownload);
        }
      },
      onDownloadRemoved: function DO_V_onDownloadRemoved(aDownload) {
        downloadsSet.delete(aDownload);
      }
    };

    aList.addView(downloadsView);
  },

  /**
   * Shows the confirm cancel downloads dialog.
   *
   * @param aCancel
   *        The observer notification subject.
   * @param aDownloadsCount
   *        The current downloads count.
   * @param aIdTitle
   *        The string bundle id for the dialog title.
   * @param aIdMessageSingle
   *        The string bundle id for the single download message.
   * @param aIdMessageMultiple
   *        The string bundle id for the multiple downloads message.
   * @param aIdButton
   *        The string bundle id for the don't cancel button text.
   */
  _confirmCancelDownloads: function DO_confirmCancelDownload(
    aCancel, aDownloadsCount, aIdTitle, aIdMessageSingle, aIdMessageMultiple, aIdButton) {
    // If user has already dismissed the request, then do nothing.
    if ((aCancel instanceof Ci.nsISupportsPRBool) && aCancel.data) {
      return;
    }
    // If there are no active downloads, then do nothing.
    if (aDownloadsCount <= 0) {
      return;
    }

    let win = Services.wm.getMostRecentWindow("navigator:browser");
    let buttonFlags = (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_0) +
                      (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_1);
    let title = gStringBundle.GetStringFromName(aIdTitle);
    let dontQuitButton = gStringBundle.GetStringFromName(aIdButton);
    let quitButton;
    let message;

    if (aDownloadsCount > 1) {
      message = gStringBundle.formatStringFromName(aIdMessageMultiple,
                                                   [aDownloadsCount], 1);
      quitButton = gStringBundle.formatStringFromName("cancelDownloadsOKTextMultiple",
                                                      [aDownloadsCount], 1);
    } else {
      message = gStringBundle.GetStringFromName(aIdMessageSingle);
      quitButton = gStringBundle.GetStringFromName("cancelDownloadsOKText");
    }

    let rv = Services.prompt.confirmEx(win, title, message, buttonFlags,
                                       quitButton, dontQuitButton, null, null, {});
    aCancel.data = (rv == 1);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function DO_observe(aSubject, aTopic, aData) {
    let downloadsCount;
    switch (aTopic) {
      case "quit-application-requested":
        downloadsCount = this._publicInProgressDownloads.size +
                         this._privateInProgressDownloads.size;
#ifndef XP_MACOSX
        this._confirmCancelDownloads(aSubject, downloadsCount,
                                     "quitCancelDownloadsAlertTitle",
                                     "quitCancelDownloadsAlertMsg",
                                     "quitCancelDownloadsAlertMsgMultiple",
                                     "dontQuitButtonWin");
#else
        this._confirmCancelDownloads(aSubject, downloadsCount,
                                     "quitCancelDownloadsAlertTitle",
                                     "quitCancelDownloadsAlertMsgMac",
                                     "quitCancelDownloadsAlertMsgMacMultiple",
                                     "dontQuitButtonMac");
#endif
        break;
      case "offline-requested":
        downloadsCount = this._publicInProgressDownloads.size +
                         this._privateInProgressDownloads.size;
        this._confirmCancelDownloads(aSubject, downloadsCount,
                                     "offlineCancelDownloadsAlertTitle",
                                     "offlineCancelDownloadsAlertMsg",
                                     "offlineCancelDownloadsAlertMsgMultiple",
                                     "dontGoOfflineButton");
        break;
      case "last-pb-context-exiting":
        this._confirmCancelDownloads(aSubject,
                                     this._privateInProgressDownloads.size,
                                     "leavePrivateBrowsingCancelDownloadsAlertTitle",
                                     "leavePrivateBrowsingWindowsCancelDownloadsAlertMsg",
                                     "leavePrivateBrowsingWindowsCancelDownloadsAlertMsgMultiple",
                                     "dontLeavePrivateBrowsingButton");
        break;
    }
  },

  ////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};
