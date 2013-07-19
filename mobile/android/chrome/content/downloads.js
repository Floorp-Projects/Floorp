// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function dump(a) {
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(a);
}

const URI_GENERIC_ICON_DOWNLOAD = "drawable://alert_download";

var Downloads = {
  _initialized: false,
  _dlmgr: null,
  _progressAlert: null,
  _privateDownloads: [],

  _getLocalFile: function dl__getLocalFile(aFileURI) {
    // if this is a URL, get the file from that
    // XXX it's possible that using a null char-set here is bad
    const fileUrl = Services.io.newURI(aFileURI, null, null).QueryInterface(Ci.nsIFileURL);
    return fileUrl.file.clone().QueryInterface(Ci.nsILocalFile);
  },

  init: function dl_init() {
    if (this._initialized)
      return;
    this._initialized = true;

    // Monitor downloads and display alerts
    this._dlmgr = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
    this._progressAlert = new AlertDownloadProgressListener();
    this._dlmgr.addPrivacyAwareListener(this._progressAlert);
    Services.obs.addObserver(this, "last-pb-context-exited", true);
  },

  openDownload: function dl_openDownload(aDownload) {
    let fileUri = aDownload.target.spec;
    let guid = aDownload.guid;
    let f = this._getLocalFile(fileUri);
    try {
      f.launch();
    } catch (ex) { 
      // in case we are not able to open the file (i.e. there is no app able to handle it)
      // we just open the browser tab showing it 
      BrowserApp.addTab("about:downloads?id=" + guid);
    }
  },

  cancelDownload: function dl_cancelDownload(aDownload) {
    aDownload.cancel();
    
    let fileURI = aDownload.target.spec;
    let f = this._getLocalFile(fileURI);
    if (f.exists())
      f.remove(false);
  },

  showAlert: function dl_showAlert(aDownload, aMessage, aTitle, aIcon) { 
    let self = this;

    // Use this flag to make sure we only show one prompt at a time
    let cancelPrompt = false;

    // Callback for tapping on the alert popup
    let observer = {
      observe: function (aSubject, aTopic, aData) {
        if (aTopic == "alertclickcallback") {
          if (aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_FINISHED) {
            // Only open the downloaded file if the download is complete
            self.openDownload(aDownload);
          } else if (aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING &&
                     !cancelPrompt) {
            cancelPrompt = true;
            // Open a prompt that offers a choice to cancel the download
            let title = Strings.browser.GetStringFromName("downloadCancelPromptTitle");
            let message = Strings.browser.GetStringFromName("downloadCancelPromptMessage");
            let flags = Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_YES +
                        Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_NO;

            let choice = Services.prompt.confirmEx(null, title, message, flags,
                                                   null, null, null, null, {});
            if (choice == 0)
              self.cancelDownload(aDownload);
            cancelPrompt = false;
          }
        }
      }
    };

    if (!aIcon)
      aIcon = URI_GENERIC_ICON_DOWNLOAD;

    if (aDownload.isPrivate) {
      this._privateDownloads.push(aDownload);
    }

    var notifier = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
    notifier.showAlertNotification(aIcon, aTitle, aMessage, true, "", observer,
                                   aDownload.target.spec.replace("file:", "download:"));
  },

  // observer for last-pb-context-exited
  observe: function dl_observe(aSubject, aTopic, aData) {
    let alertsService = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
    let progressListener = alertsService.QueryInterface(Ci.nsIAlertsProgressListener);
    let download;
    while ((download = this._privateDownloads.pop())) {
      try {
        let notificationName = download.target.spec.replace("file:", "download:");
        progressListener.onCancel(notificationName);
      } catch (e) {
        dump("Error removing private download: " + e);
      }
    }
  },

  QueryInterface: function (aIID) {
    if (!aIID.equals(Ci.nsISupports) &&
        !aIID.equals(Ci.nsIObserver) &&
        !aIID.equals(Ci.nsISupportsWeakReference))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

// AlertDownloadProgressListener is used to display progress in the alert notifications.
function AlertDownloadProgressListener() { }

AlertDownloadProgressListener.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsIDownloadProgressListener
  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress, aDownload) {
    let strings = Strings.browser;
    let availableSpace = -1;
    try {
      // diskSpaceAvailable is not implemented on all systems
      let availableSpace = aDownload.targetFile.diskSpaceAvailable;
    } catch(ex) { }
    let contentLength = aDownload.size;
    if (availableSpace > 0 && contentLength > 0 && contentLength > availableSpace) {
      Downloads.showAlert(aDownload, strings.GetStringFromName("alertDownloadsNoSpace"),
                                     strings.GetStringFromName("alertDownloadsSize"));

      aDownload.cancel();
    }

    if (aDownload.percentComplete == -1) {
      // Undetermined progress is not supported yet
      return;
    }
    let alertsService = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
    let progressListener = alertsService.QueryInterface(Ci.nsIAlertsProgressListener);
    let notificationName = aDownload.target.spec.replace("file:", "download:");
    progressListener.onProgress(notificationName, aDownload.percentComplete, 100);
  },

  onDownloadStateChange: function(aState, aDownload) {
    let state = aDownload.state;
    switch (state) {
      case Ci.nsIDownloadManager.DOWNLOAD_QUEUED:
        NativeWindow.toast.show(Strings.browser.GetStringFromName("alertDownloadsToast"), "long");
        Downloads.showAlert(aDownload, Strings.browser.GetStringFromName("alertDownloadsStart2"),
                            aDownload.displayName);
        break;
      case Ci.nsIDownloadManager.DOWNLOAD_FAILED:
      case Ci.nsIDownloadManager.DOWNLOAD_CANCELED:
      case Ci.nsIDownloadManager.DOWNLOAD_BLOCKED_PARENTAL:
      case Ci.nsIDownloadManager.DOWNLOAD_DIRTY:
      case Ci.nsIDownloadManager.DOWNLOAD_FINISHED: {
        let alertsService = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
        let progressListener = alertsService.QueryInterface(Ci.nsIAlertsProgressListener);
        let notificationName = aDownload.target.spec.replace("file:", "download:");
        progressListener.onCancel(notificationName);

        if (aDownload.isPrivate) {
          let index = this._privateDownloads.indexOf(aDownload);
          if (index != -1) {
            this._privateDownloads.splice(index, 1);
          }
        }

        if (state == Ci.nsIDownloadManager.DOWNLOAD_FINISHED) {
          Downloads.showAlert(aDownload, Strings.browser.GetStringFromName("alertDownloadsDone2"),
                              aDownload.displayName);
        }
        break;
      }
    }
  },

  onStateChange: function(aWebProgress, aRequest, aState, aStatus, aDownload) { },
  onSecurityChange: function(aWebProgress, aRequest, aState, aDownload) { },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports
  QueryInterface: function (aIID) {
    if (!aIID.equals(Ci.nsIDownloadProgressListener) &&
        !aIID.equals(Ci.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};
