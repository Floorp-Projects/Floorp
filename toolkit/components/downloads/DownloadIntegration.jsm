/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides functions to integrate with the host application, handling for
 * example the global prompts on shutdown.
 */

"use strict";

var EXPORTED_SYMBOLS = ["DownloadIntegration"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Downloads } = ChromeUtils.import(
  "resource://gre/modules/Downloads.jsm"
);
const { Integration } = ChromeUtils.import(
  "resource://gre/modules/Integration.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "AsyncShutdown",
  "resource://gre/modules/AsyncShutdown.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "DownloadStore",
  "resource://gre/modules/DownloadStore.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "DownloadUIHelper",
  "resource://gre/modules/DownloadUIHelper.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gDownloadPlatform",
  "@mozilla.org/toolkit/download-platform;1",
  "mozIDownloadPlatform"
);
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gEnvironment",
  "@mozilla.org/process/environment;1",
  "nsIEnvironment"
);
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gMIMEService",
  "@mozilla.org/mime;1",
  "nsIMIMEService"
);
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gExternalProtocolService",
  "@mozilla.org/uriloader/external-protocol-service;1",
  "nsIExternalProtocolService"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "RuntimePermissions",
  "resource://gre/modules/RuntimePermissions.jsm"
);

XPCOMUtils.defineLazyGetter(lazy, "gParentalControlsService", function() {
  if ("@mozilla.org/parental-controls-service;1" in Cc) {
    return Cc["@mozilla.org/parental-controls-service;1"].createInstance(
      Ci.nsIParentalControlsService
    );
  }
  return null;
});

ChromeUtils.defineModuleGetter(
  lazy,
  "DownloadSpamProtection",
  "resource:///modules/DownloadSpamProtection.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gApplicationReputationService",
  "@mozilla.org/reputationservice/application-reputation-service;1",
  Ci.nsIApplicationReputationService
);

// We have to use the gCombinedDownloadIntegration identifier because, in this
// module only, the DownloadIntegration identifier refers to the base version.
Integration.downloads.defineModuleGetter(
  lazy,
  "gCombinedDownloadIntegration",
  "resource://gre/modules/DownloadIntegration.jsm",
  "DownloadIntegration"
);

const Timer = Components.Constructor(
  "@mozilla.org/timer;1",
  "nsITimer",
  "initWithCallback"
);

/**
 * Indicates the delay between a change to the downloads data and the related
 * save operation.
 *
 * For best efficiency, this value should be high enough that the input/output
 * for opening or closing the target file does not overlap with the one for
 * saving the list of downloads.
 */
const kSaveDelayMs = 1500;

/**
 * List of observers to listen against
 */
const kObserverTopics = [
  "quit-application-requested",
  "offline-requested",
  "last-pb-context-exiting",
  "last-pb-context-exited",
  "sleep_notification",
  "suspend_process_notification",
  "wake_notification",
  "resume_process_notification",
  "network:offline-about-to-go-offline",
  "network:offline-status-changed",
  "xpcom-will-shutdown",
  "blocked-automatic-download",
];

/**
 * Maps nsIApplicationReputationService verdicts with the DownloadError ones.
 */
const kVerdictMap = {
  [Ci.nsIApplicationReputationService.VERDICT_DANGEROUS]:
    Downloads.Error.BLOCK_VERDICT_MALWARE,
  [Ci.nsIApplicationReputationService.VERDICT_UNCOMMON]:
    Downloads.Error.BLOCK_VERDICT_UNCOMMON,
  [Ci.nsIApplicationReputationService.VERDICT_POTENTIALLY_UNWANTED]:
    Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED,
  [Ci.nsIApplicationReputationService.VERDICT_DANGEROUS_HOST]:
    Downloads.Error.BLOCK_VERDICT_MALWARE,
};

/**
 * Provides functions to integrate with the host application, handling for
 * example the global prompts on shutdown.
 */
var DownloadIntegration = {
  /**
   * Main DownloadStore object for loading and saving the list of persistent
   * downloads, or null if the download list was never requested and thus it
   * doesn't need to be persisted.
   */
  _store: null,

  /**
   * Returns whether data for blocked downloads should be kept on disk.
   * Implementations which support unblocking downloads may return true to
   * keep the blocked download on disk until its fate is decided.
   *
   * If a download is blocked and the partial data is kept the Download's
   * 'hasBlockedData' property will be true. In this state Download.unblock()
   * or Download.confirmBlock() may be used to either unblock the download or
   * remove the downloaded data respectively.
   *
   * Even if shouldKeepBlockedData returns true, if the download did not use a
   * partFile the blocked data will be removed - preventing the complete
   * download from existing on disk with its final filename.
   *
   * @return boolean True if data should be kept.
   */
  shouldKeepBlockedData() {
    const FIREFOX_ID = "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}";
    return Services.appinfo.ID == FIREFOX_ID;
  },

  /**
   * Performs initialization of the list of persistent downloads, before its
   * first use by the host application.  This function may be called only once
   * during the entire lifetime of the application.
   *
   * @param list
   *        DownloadList object to be initialized.
   *
   * @return {Promise}
   * @resolves When the list has been initialized.
   * @rejects JavaScript exception.
   */
  async initializePublicDownloadList(list) {
    try {
      await this.loadPublicDownloadListFromStore(list);
    } catch (ex) {
      Cu.reportError(ex);
    }

    if (AppConstants.MOZ_PLACES) {
      // After the list of persistent downloads has been loaded, we can add the
      // history observers, even if the load operation failed. This object is kept
      // alive by the history service.
      new DownloadHistoryObserver(list);
    }
  },

  /**
   * Called by initializePublicDownloadList to load the list of persistent
   * downloads, before its first use by the host application.  This function may
   * be called only once during the entire lifetime of the application.
   *
   * @param list
   *        DownloadList object to be populated with the download objects
   *        serialized from the previous session.  This list will be persisted
   *        to disk during the session lifetime.
   *
   * @return {Promise}
   * @resolves When the list has been populated.
   * @rejects JavaScript exception.
   */
  async loadPublicDownloadListFromStore(list) {
    if (this._store) {
      throw new Error("Initialization may be performed only once.");
    }

    this._store = new lazy.DownloadStore(
      list,
      PathUtils.join(PathUtils.profileDir, "downloads.json")
    );
    this._store.onsaveitem = this.shouldPersistDownload.bind(this);

    try {
      await this._store.load();
    } catch (ex) {
      Cu.reportError(ex);
    }

    // Add the view used for detecting changes to downloads to be persisted.
    // We must do this after the list of persistent downloads has been loaded,
    // even if the load operation failed. We wait for a complete initialization
    // so other callers cannot modify the list without being detected. The
    // DownloadAutoSaveView is kept alive by the underlying DownloadList.
    await new DownloadAutoSaveView(list, this._store).initialize();
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
  shouldPersistDownload(aDownload) {
    // On all platforms, we save all the downloads currently in progress, as
    // well as stopped downloads for which we retained partially downloaded
    // data or we have blocked data.
    // On Android we store all history; on Desktop, stopped downloads for which
    // we don't need to track the presence of a ".part" file are only retained
    // in the browser history.
    return (
      !aDownload.stopped ||
      aDownload.hasPartialData ||
      aDownload.hasBlockedData ||
      AppConstants.platform == "android"
    );
  },

  /**
   * Returns the system downloads directory asynchronously.
   *
   * @return {Promise}
   * @resolves The downloads directory string path.
   */
  async getSystemDownloadsDirectory() {
    if (this._downloadsDirectory) {
      return this._downloadsDirectory;
    }

    if (AppConstants.platform == "android") {
      // Android doesn't have a $HOME directory, and by default we only have
      // write access to /data/data/org.mozilla.{$APP} and /sdcard
      this._downloadsDirectory = lazy.gEnvironment.get("DOWNLOADS_DIRECTORY");
      if (!this._downloadsDirectory) {
        throw new Components.Exception(
          "DOWNLOADS_DIRECTORY is not set.",
          Cr.NS_ERROR_FILE_UNRECOGNIZED_PATH
        );
      }
    } else {
      try {
        this._downloadsDirectory = this._getDirectory("DfltDwnld");
      } catch (e) {
        this._downloadsDirectory = await this._createDownloadsDirectory("Home");
      }
    }

    return this._downloadsDirectory;
  },
  _downloadsDirectory: null,

  /**
   * Returns the user downloads directory asynchronously.
   *
   * @return {Promise}
   * @resolves The downloads directory string path.
   */
  async getPreferredDownloadsDirectory() {
    let directoryPath = null;
    let prefValue = Services.prefs.getIntPref("browser.download.folderList", 1);

    switch (prefValue) {
      case 0: // Desktop
        directoryPath = this._getDirectory("Desk");
        break;
      case 1: // Downloads
        directoryPath = await this.getSystemDownloadsDirectory();
        break;
      case 2: // Custom
        try {
          let directory = Services.prefs.getComplexValue(
            "browser.download.dir",
            Ci.nsIFile
          );
          directoryPath = directory.path;
          await IOUtils.makeDirectory(directoryPath, {
            createAncestors: false,
          });
        } catch (ex) {
          Cu.reportError(ex);
          // Either the preference isn't set or the directory cannot be created.
          directoryPath = await this.getSystemDownloadsDirectory();
        }
        break;
      default:
        directoryPath = await this.getSystemDownloadsDirectory();
    }
    return directoryPath;
  },

  /**
   * Returns the temporary downloads directory asynchronously.
   *
   * @return {Promise}
   * @resolves The downloads directory string path.
   */
  async getTemporaryDownloadsDirectory() {
    let directoryPath = null;
    if (AppConstants.platform == "macosx") {
      directoryPath = await this.getPreferredDownloadsDirectory();
    } else if (AppConstants.platform == "android") {
      directoryPath = await this.getSystemDownloadsDirectory();
    } else {
      directoryPath = this._getDirectory("TmpD");
    }
    return directoryPath;
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
  shouldBlockForParentalControls(aDownload) {
    let isEnabled =
      lazy.gParentalControlsService &&
      lazy.gParentalControlsService.parentalControlsEnabled;
    let shouldBlock =
      isEnabled && lazy.gParentalControlsService.blockFileDownloadsEnabled;

    // Log the event if required by parental controls settings.
    if (isEnabled && lazy.gParentalControlsService.loggingEnabled) {
      lazy.gParentalControlsService.log(
        lazy.gParentalControlsService.ePCLog_FileDownload,
        shouldBlock,
        lazy.NetUtil.newURI(aDownload.source.url),
        null
      );
    }

    return Promise.resolve(shouldBlock);
  },

  /**
   * Checks to determine whether to block downloads for not granted runtime permissions.
   *
   * @return {Promise}
   * @resolves The boolean indicates to block downloads or not.
   */
  async shouldBlockForRuntimePermissions() {
    return (
      AppConstants.platform == "android" &&
      !(await lazy.RuntimePermissions.waitForPermissions(
        lazy.RuntimePermissions.WRITE_EXTERNAL_STORAGE
      ))
    );
  },

  /**
   * Checks to determine whether to block downloads because they might be
   * malware, based on application reputation checks.
   *
   * aParam aDownload
   *        The download object.
   *
   * @return {Promise}
   * @resolves Object with the following properties:
   *           {
   *             shouldBlock: Whether the download should be blocked.
   *             verdict: Detailed reason for the block, according to the
   *                      "Downloads.Error.BLOCK_VERDICT_" constants, or empty
   *                      string if the reason is unknown.
   *           }
   */
  shouldBlockForReputationCheck(aDownload) {
    let hash;
    let sigInfo;
    let channelRedirects;
    try {
      hash = aDownload.saver.getSha256Hash();
      sigInfo = aDownload.saver.getSignatureInfo();
      channelRedirects = aDownload.saver.getRedirects();
    } catch (ex) {
      // Bail if DownloadSaver doesn't have a hash or signature info.
      return Promise.resolve({
        shouldBlock: false,
        verdict: "",
      });
    }
    if (!hash || !sigInfo) {
      return Promise.resolve({
        shouldBlock: false,
        verdict: "",
      });
    }
    return new Promise(resolve => {
      lazy.gApplicationReputationService.queryReputation(
        {
          sourceURI: lazy.NetUtil.newURI(aDownload.source.url),
          referrerInfo: aDownload.source.referrerInfo,
          fileSize: aDownload.currentBytes,
          sha256Hash: hash,
          suggestedFileName: PathUtils.filename(aDownload.target.path),
          signatureInfo: sigInfo,
          redirects: channelRedirects,
        },
        function onComplete(aShouldBlock, aRv, aVerdict) {
          resolve({
            shouldBlock: aShouldBlock,
            verdict: (aShouldBlock && kVerdictMap[aVerdict]) || "",
          });
        }
      );
    });
  },

  /**
   * Checks whether downloaded files should be marked as coming from
   * Internet Zone.
   *
   * @return true if files should be marked
   */
  _shouldSaveZoneInformation() {
    let key = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
      Ci.nsIWindowsRegKey
    );
    try {
      key.open(
        Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Attachments",
        Ci.nsIWindowsRegKey.ACCESS_QUERY_VALUE
      );
      try {
        return key.readIntValue("SaveZoneInformation") != 1;
      } finally {
        key.close();
      }
    } catch (ex) {
      // If the key is not present, files should be marked by default.
      return true;
    }
  },

  /**
   * Builds a key and URL value pair for the "Zone.Identifier" Alternate Data
   * Stream.
   *
   * @param aKey
   *        String to write before the "=" sign. This is not validated.
   * @param aUrl
   *        URL string to write after the "=" sign. Only the "http(s)" and
   *        "ftp" schemes are allowed, and usernames and passwords are
   *        stripped.
   * @param [optional] aFallback
   *        Value to place after the "=" sign in case the URL scheme is not
   *        allowed. If unspecified, an empty string is returned when the
   *        scheme is not allowed.
   *
   * @return Line to add to the stream, including the final CRLF, or an empty
   *         string if the validation failed.
   */
  _zoneIdKey(aKey, aUrl, aFallback) {
    try {
      let url;
      const uri = lazy.NetUtil.newURI(aUrl);
      if (["http", "https", "ftp"].includes(uri.scheme)) {
        url = uri
          .mutate()
          .setUserPass("")
          .finalize().spec;
      } else if (aFallback) {
        url = aFallback;
      } else {
        return "";
      }
      return aKey + "=" + url + "\r\n";
    } catch (e) {
      return "";
    }
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
  async downloadDone(aDownload) {
    // On Windows, we mark any file saved to the NTFS file system as coming
    // from the Internet security zone unless Group Policy disables the
    // feature.  We do this by writing to the "Zone.Identifier" Alternate
    // Data Stream directly, because the Save method of the
    // IAttachmentExecute interface would trigger operations that may cause
    // the application to hang, or other performance issues.
    // The stream created in this way is forward-compatible with all the
    // current and future versions of Windows.
    if (AppConstants.platform == "win" && this._shouldSaveZoneInformation()) {
      let zone;
      try {
        zone = lazy.gDownloadPlatform.mapUrlToZone(aDownload.source.url);
      } catch (e) {
        // Default to Internet Zone if mapUrlToZone failed for
        // whatever reason.
        zone = Ci.mozIDownloadPlatform.ZONE_INTERNET;
      }
      // Don't write zone IDs for Local, Intranet, or Trusted sites
      // to match Windows behavior.
      if (zone >= Ci.mozIDownloadPlatform.ZONE_INTERNET) {
        let path = aDownload.target.path + ":Zone.Identifier";
        try {
          let zoneId = "[ZoneTransfer]\r\nZoneId=" + zone + "\r\n";
          let { url, isPrivate, referrerInfo } = aDownload.source;
          if (!isPrivate) {
            let referrer = referrerInfo
              ? referrerInfo.computedReferrerSpec
              : "";
            zoneId +=
              this._zoneIdKey("ReferrerUrl", referrer) +
              this._zoneIdKey("HostUrl", url, "about:internet");
          }
          await IOUtils.writeUTF8(
            PathUtils.toExtendedWindowsPath(path),
            zoneId
          );
        } catch (ex) {
          // If writing to the file fails, we ignore the error and continue.
          if (!DOMException.isInstance(ex)) {
            Cu.reportError(ex);
          }
        }
      }
    }

    // The file with the partially downloaded data has restrictive permissions
    // that don't allow other users on the system to access it.  Now that the
    // download is completed, we need to adjust permissions based on whether
    // this is a permanently downloaded file or a temporary download to be
    // opened read-only with an external application.
    try {
      // The following logic to determine whether this is a temporary download
      // is due to the fact that "deleteTempFileOnExit" is false on Mac, where
      // downloads to be opened with external applications are preserved in
      // the "Downloads" folder like normal downloads.
      let isTemporaryDownload =
        aDownload.launchWhenSucceeded &&
        (aDownload.source.isPrivate ||
          (Services.prefs.getBoolPref(
            "browser.helperApps.deleteTempFileOnExit"
          ) &&
            !Services.prefs.getBoolPref(
              "browser.download.improvements_to_download_panel"
            )));
      // Permanently downloaded files are made accessible by other users on
      // this system, while temporary downloads are marked as read-only.
      let unixMode;
      if (isTemporaryDownload) {
        unixMode = 0o400;
      } else {
        unixMode = 0o666;
      }
      // On Unix, the umask of the process is respected.
      await IOUtils.setPermissions(aDownload.target.path, unixMode);
    } catch (ex) {
      // We should report errors with making the permissions less restrictive
      // or marking the file as read-only on Unix and Mac, but this should not
      // prevent the download from completing.
      if (!DOMException.isInstance(ex)) {
        Cu.reportError(ex);
      }
    }

    let aReferrer = null;
    if (aDownload.source.referrerInfo) {
      aReferrer = aDownload.source.referrerInfo.originalReferrer;
    }

    await lazy.gDownloadPlatform.downloadDone(
      lazy.NetUtil.newURI(aDownload.source.url),
      aReferrer,
      new lazy.FileUtils.File(aDownload.target.path),
      aDownload.contentType,
      aDownload.source.isPrivate
    );
  },

  /**
   * Decide whether a download of this type, opened from the downloads
   * list, should open internally.
   *
   * @param aMimeType
   *        The MIME type of the file, as a string
   * @param [optional] aExtension
   *        The file extension, which can match instead of the MIME type.
   */
  shouldViewDownloadInternally(aMimeType, aExtension) {
    // Refuse all files by default, this is meant to be replaced with a check
    // for specific types via Integration.downloads.register().
    return false;
  },

  /**
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
   * @param options.openWhere     Optional string indicating how to open when handling
   *                              download by opening the target file URI.
   *                              One of "window", "tab", "tabshifted"
   * @param options.useSystemDefault
   *                              Optional value indicating how to handle launching this download,
   *                              this time only. Will override the associated mimeInfo.preferredAction
   *
   * @return {Promise}
   * @resolves When the instruction to launch the file has been
   *           successfully given to the operating system. Note that
   *           the OS might still take a while until the file is actually
   *           launched.
   * @rejects  JavaScript exception if there was an error trying to launch
   *           the file.
   */
  async launchDownload(aDownload, { openWhere, useSystemDefault = null }) {
    let file = new lazy.FileUtils.File(aDownload.target.path);

    // In case of a double extension, like ".tar.gz", we only
    // consider the last one, because the MIME service cannot
    // handle multiple extensions.
    let fileExtension = null,
      mimeInfo = null;
    let match = file.leafName.match(/\.([^.]+)$/);
    if (match) {
      fileExtension = match[1];
    }

    let isWindowsExe =
      AppConstants.platform == "win" &&
      fileExtension &&
      fileExtension.toLowerCase() == "exe";

    let isExemptExecutableExtension = false;
    try {
      let url = new URL(aDownload.source.url);
      isExemptExecutableExtension = Services.policies.isExemptExecutableExtension(
        url.origin,
        fileExtension?.toLowerCase()
      );
    } catch (e) {
      // Invalid URL, go down the original path.
    }

    // Ask for confirmation if the file is executable, except for .exe on
    // Windows where the operating system will show the prompt based on the
    // security zone.  We do this here, instead of letting the caller handle
    // the prompt separately in the user interface layer, for two reasons.  The
    // first is because of its security nature, so that add-ons cannot forget
    // to do this check.  The second is that the system-level security prompt
    // would be displayed at launch time in any case.
    // We allow policy to override this behavior for file extensions on specific domains.
    if (
      file.isExecutable() &&
      !isWindowsExe &&
      !isExemptExecutableExtension &&
      !(await this.confirmLaunchExecutable(file.path))
    ) {
      return;
    }

    try {
      // The MIME service might throw if contentType == "" and it can't find
      // a MIME type for the given extension, so we'll treat this case as
      // an unknown mimetype.
      mimeInfo = lazy.gMIMEService.getFromTypeAndExtension(
        aDownload.contentType,
        fileExtension
      );
    } catch (e) {}

    if (aDownload.launcherPath) {
      if (!mimeInfo) {
        // This should not happen on normal circumstances because launcherPath
        // is only set when we had an instance of nsIMIMEInfo to retrieve
        // the custom application chosen by the user.
        throw new Error(
          "Unable to create nsIMIMEInfo to launch a custom application"
        );
      }

      // Custom application chosen
      let localHandlerApp = Cc[
        "@mozilla.org/uriloader/local-handler-app;1"
      ].createInstance(Ci.nsILocalHandlerApp);
      localHandlerApp.executable = new lazy.FileUtils.File(
        aDownload.launcherPath
      );

      mimeInfo.preferredApplicationHandler = localHandlerApp;
      mimeInfo.preferredAction = Ci.nsIMIMEInfo.useHelperApp;

      this.launchFile(file, mimeInfo);
      // After an attempt has been made to launch the download, clear the
      // launchWhenSucceeded bit so future attempts to open the download can go
      // through Firefox when possible.
      aDownload.launchWhenSucceeded = false;
      return;
    }

    if (!useSystemDefault && mimeInfo) {
      useSystemDefault = mimeInfo.preferredAction == mimeInfo.useSystemDefault;
    }
    if (!useSystemDefault) {
      // No explicit instruction was passed to launch this download using the default system viewer.
      if (
        aDownload.handleInternally ||
        (mimeInfo &&
          this.shouldViewDownloadInternally(mimeInfo.type, fileExtension) &&
          !mimeInfo.alwaysAskBeforeHandling &&
          (mimeInfo.preferredAction === Ci.nsIHandlerInfo.handleInternally ||
            (["image/svg+xml", "text/xml", "application/xml"].includes(
              mimeInfo.type
            ) &&
              mimeInfo.preferredAction === Ci.nsIHandlerInfo.saveToDisk)) &&
          !aDownload.launchWhenSucceeded)
      ) {
        lazy.DownloadUIHelper.loadFileIn(file, {
          browsingContextId: aDownload.source.browsingContextId,
          isPrivate: aDownload.source.isPrivate,
          openWhere,
          userContextId: aDownload.source.userContextId,
        });
        return;
      }
    }

    // An attempt will now be made to launch the download, clear the
    // launchWhenSucceeded bit so future attempts to open the download can go
    // through Firefox when possible.
    aDownload.launchWhenSucceeded = false;

    // When a file has no extension, and there's an executable file with the
    // same name in the same folder, Windows shell can get confused.
    // For this reason we show the file in the containing folder instead of
    // trying to open it.
    // We also don't trust mimeinfo, it could be a type we can forward to a
    // system handler, but it could also be an executable type, and we
    // don't have an exhaustive list with all of them.
    if (!fileExtension && AppConstants.platform == "win") {
      // We can't check for the existance of a same-name file with every
      // possible executable extension, so this is a catch-all.
      this.showContainingDirectory(aDownload.target.path);
      return;
    }

    // No custom application chosen, let's launch the file with the default
    // handler. First, let's try to launch it through the MIME service.
    if (mimeInfo) {
      mimeInfo.preferredAction = Ci.nsIMIMEInfo.useSystemDefault;
      try {
        this.launchFile(file, mimeInfo);
        return;
      } catch (ex) {}
    }

    // If it didn't work or if there was no MIME info available,
    // let's try to directly launch the file.
    try {
      this.launchFile(file);
      return;
    } catch (ex) {}

    // If our previous attempts failed, try sending it through
    // the system's external "file:" URL handler.
    lazy.gExternalProtocolService.loadURI(
      lazy.NetUtil.newURI(file),
      Services.scriptSecurityManager.getSystemPrincipal()
    );
  },

  /**
   * Asks for confirmation for launching the specified executable file. This
   * can be overridden by regression tests to avoid the interactive prompt.
   */
  async confirmLaunchExecutable(path) {
    // We don't anchor the prompt to a specific window intentionally, not
    // only because this is the same behavior as the system-level prompt,
    // but also because the most recently active window is the right choice
    // in basically all cases.
    return lazy.DownloadUIHelper.getPrompter().confirmLaunchExecutable(path);
  },

  /**
   * Launches the specified file, unless overridden by regression tests.
   * @note Always use launchDownload() from the outside of this module, it is
   *       both more powerful and safer.
   */
  launchFile(file, mimeInfo) {
    if (mimeInfo) {
      mimeInfo.launchWithFile(file);
    } else {
      file.launch();
    }
  },

  /**
   * Shows the containing folder of a file.
   *
   * @param aFilePath
   *        The path to the file.
   *
   * @return {Promise}
   * @resolves When the instruction to open the containing folder has been
   *           successfully given to the operating system. Note that
   *           the OS might still take a while until the folder is actually
   *           opened.
   * @rejects  JavaScript exception if there was an error trying to open
   *           the containing folder.
   */
  async showContainingDirectory(aFilePath) {
    let file = new lazy.FileUtils.File(aFilePath);

    try {
      // Show the directory containing the file and select the file.
      file.reveal();
      return;
    } catch (ex) {}

    // If reveal fails for some reason (e.g., it's not implemented on unix
    // or the file doesn't exist), try using the parent if we have it.
    let parent = file.parent;
    if (!parent) {
      throw new Error(
        "Unexpected reference to a top-level directory instead of a file"
      );
    }

    try {
      // Open the parent directory to show where the file should be.
      parent.launch();
      return;
    } catch (ex) {}

    // If launch also fails (probably because it's not implemented), let
    // the OS handler try to open the parent.
    lazy.gExternalProtocolService.loadURI(
      lazy.NetUtil.newURI(parent),
      Services.scriptSecurityManager.getSystemPrincipal()
    );
  },

  /**
   * Calls the directory service, create a downloads directory and returns an
   * nsIFile for the downloads directory.
   *
   * @return {Promise}
   * @resolves The directory string path.
   */
  _createDownloadsDirectory(aName) {
    // We read the name of the directory from the list of translated strings
    // that is kept by the UI helper module, even if this string is not strictly
    // displayed in the user interface.
    let directoryPath = PathUtils.join(
      this._getDirectory(aName),
      lazy.DownloadUIHelper.strings.downloadsFolder
    );

    // Create the Downloads folder and ignore if it already exists.
    return IOUtils.makeDirectory(directoryPath, {
      createAncestors: false,
    }).then(() => directoryPath);
  },

  /**
   * Returns the string path for the given directory service location name. This
   * can be overridden by regression tests to return the path of the system
   * temporary directory in all cases.
   */
  _getDirectory(name) {
    return Services.dirsvc.get(name, Ci.nsIFile).path;
  },
  /**
   * Initializes the DownloadSpamProtection instance.
   * This is used to observe and group multiple automatic downloads.
   */
  _initializeDownloadSpamProtection() {
    this.downloadSpamProtection = new lazy.DownloadSpamProtection();
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
  addListObservers(aList, aIsPrivate) {
    DownloadObserver.registerView(aList, aIsPrivate);
    if (!DownloadObserver.observersAdded) {
      DownloadObserver.observersAdded = true;
      for (let topic of kObserverTopics) {
        Services.obs.addObserver(DownloadObserver, topic);
      }
    }
    return Promise.resolve();
  },

  /**
   * Force a save on _store if it exists. Used to ensure downloads do not
   * persist after being sanitized on Android.
   *
   * @return {Promise}
   * @resolves When _store.save() completes.
   */
  forceSave() {
    if (this._store) {
      return this._store.save();
    }
    return Promise.resolve();
  },
};

var DownloadObserver = {
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
    let downloadsSet = aIsPrivate
      ? this._privateInProgressDownloads
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
      },
    };

    // We register the view asynchronously.
    aList.addView(downloadsView).catch(Cu.reportError);
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
    aCancel,
    aDownloadsCount,
    aPromptType
  ) {
    // Handle test mode
    if (lazy.gCombinedDownloadIntegration._testPromptDownloads) {
      lazy.gCombinedDownloadIntegration._testPromptDownloads = aDownloadsCount;
      return;
    }

    if (!aDownloadsCount) {
      return;
    }

    // If user has already dismissed the request, then do nothing.
    if (aCancel instanceof Ci.nsISupportsPRBool && aCancel.data) {
      return;
    }

    let prompter = lazy.DownloadUIHelper.getPrompter();
    aCancel.data = prompter.confirmCancelDownloads(
      aDownloadsCount,
      prompter[aPromptType]
    );
  },

  /**
   * Resume all downloads that were paused when going offline, used when waking
   * from sleep or returning from being offline.
   */
  _resumeOfflineDownloads: function DO_resumeOfflineDownloads() {
    this._wakeTimer = null;

    for (let download of this._canceledOfflineDownloads) {
      download.start().catch(() => {});
    }
    this._canceledOfflineDownloads.clear();
  },

  // nsIObserver
  observe: function DO_observe(aSubject, aTopic, aData) {
    let downloadsCount;
    switch (aTopic) {
      case "quit-application-requested":
        downloadsCount =
          this._publicInProgressDownloads.size +
          this._privateInProgressDownloads.size;
        this._confirmCancelDownloads(aSubject, downloadsCount, "ON_QUIT");
        break;
      case "offline-requested":
        downloadsCount =
          this._publicInProgressDownloads.size +
          this._privateInProgressDownloads.size;
        this._confirmCancelDownloads(aSubject, downloadsCount, "ON_OFFLINE");
        break;
      case "last-pb-context-exiting":
        downloadsCount = this._privateInProgressDownloads.size;
        this._confirmCancelDownloads(
          aSubject,
          downloadsCount,
          "ON_LEAVE_PRIVATE_BROWSING"
        );
        break;
      case "last-pb-context-exited":
        let promise = (async function() {
          let list = await Downloads.getList(Downloads.PRIVATE);
          let downloads = await list.getAll();

          // We can remove the downloads and finalize them in parallel.
          for (let download of downloads) {
            list.remove(download).catch(Cu.reportError);
            download.finalize(true).catch(Cu.reportError);
          }
        })();
        // Handle test mode
        if (lazy.gCombinedDownloadIntegration._testResolveClearPrivateList) {
          lazy.gCombinedDownloadIntegration._testResolveClearPrivateList(
            promise
          );
        } else {
          promise.catch(ex => Cu.reportError(ex));
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
        let wakeDelay = Services.prefs.getIntPref(
          "browser.download.manager.resumeOnWakeDelay",
          10000
        );

        if (wakeDelay >= 0) {
          this._wakeTimer = new Timer(
            this._resumeOfflineDownloads.bind(this),
            wakeDelay,
            Ci.nsITimer.TYPE_ONE_SHOT
          );
        }
        break;
      case "network:offline-status-changed":
        if (aData == "online") {
          this._resumeOfflineDownloads();
        }
        break;
      // We need to unregister observers explicitly before we reach the
      // "xpcom-shutdown" phase, otherwise observers may be notified when some
      // required services are not available anymore. We can't unregister
      // observers on "quit-application", because this module is also loaded
      // during "make package" automation, and the quit notification is not sent
      // in that execution environment (bug 973637).
      case "xpcom-will-shutdown":
        for (let topic of kObserverTopics) {
          Services.obs.removeObserver(this, topic);
        }
        break;
      case "blocked-automatic-download":
        if (
          AppConstants.MOZ_BUILD_APP == "browser" &&
          !DownloadIntegration.downloadSpamProtection
        ) {
          DownloadIntegration._initializeDownloadSpamProtection();
        }
        DownloadIntegration.downloadSpamProtection.update(aData);
        break;
    }
  },

  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
};

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
var DownloadHistoryObserver = function(aList) {
  this._list = aList;

  const placesObserver = new PlacesWeakCallbackWrapper(
    this.handlePlacesEvents.bind(this)
  );
  PlacesObservers.addListener(
    ["history-cleared", "page-removed"],
    placesObserver
  );
};

DownloadHistoryObserver.prototype = {
  /**
   * DownloadList object linked to this observer.
   */
  _list: null,

  handlePlacesEvents(events) {
    for (const event of events) {
      switch (event.type) {
        case "history-cleared": {
          this._list.removeFinished();
          break;
        }
        case "page-removed": {
          if (event.isRemovedFromStore) {
            this._list.removeFinished(
              download => event.url === download.source.url
            );
          }
          break;
        }
      }
    }
  },
};

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
var DownloadAutoSaveView = function(aList, aStore) {
  this._list = aList;
  this._store = aStore;
  this._downloadsMap = new Map();
  this._writer = new lazy.DeferredTask(() => this._store.save(), kSaveDelayMs);
  lazy.AsyncShutdown.profileBeforeChange.addBlocker(
    "DownloadAutoSaveView: writing data",
    () => this._writer.finalize()
  );
};

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
  initialize() {
    // We set _initialized to true after adding the view, so that
    // onDownloadAdded doesn't cause a save to occur.
    return this._list.addView(this).then(() => (this._initialized = true));
  },

  /**
   * This map contains only Download objects that should be saved to disk, and
   * associates them with the result of their getSerializationHash function, for
   * the purpose of detecting changes to the relevant properties.
   */
  _downloadsMap: null,

  /**
   * DeferredTask for the save operation.
   */
  _writer: null,

  /**
   * Called when the list of downloads changed, this triggers the asynchronous
   * serialization of the list of downloads.
   */
  saveSoon() {
    this._writer.arm();
  },

  // DownloadList callback
  onDownloadAdded(aDownload) {
    if (lazy.gCombinedDownloadIntegration.shouldPersistDownload(aDownload)) {
      this._downloadsMap.set(aDownload, aDownload.getSerializationHash());
      if (this._initialized) {
        this.saveSoon();
      }
    }
  },

  // DownloadList callback
  onDownloadChanged(aDownload) {
    if (!lazy.gCombinedDownloadIntegration.shouldPersistDownload(aDownload)) {
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

  // DownloadList callback
  onDownloadRemoved(aDownload) {
    if (this._downloadsMap.has(aDownload)) {
      this._downloadsMap.delete(aDownload);
      this.saveSoon();
    }
  },
};
