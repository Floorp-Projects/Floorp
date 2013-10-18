// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function dump(a) {
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(a);
}

const URI_GENERIC_ICON_DOWNLOAD = "drawable://alert_download";
const URI_PAUSE_ICON = "drawable://pause";
const URI_CANCEL_ICON = "drawable://close";
const URI_RESUME_ICON = "drawable://play";

const PAUSE_ACTION = {
  actionKind : "pause",
  title : Strings.browser.GetStringFromName("alertDownloadsPause"),
  icon : URI_PAUSE_ICON
};

const CANCEL_ACTION = {
  actionKind : "cancel",
  title : Strings.browser.GetStringFromName("alertDownloadsCancel"),
  icon : URI_CANCEL_ICON
};

const RESUME_ACTION = {
  actionKind : "resume",
  title : Strings.browser.GetStringFromName("alertDownloadsResume"),
  icon : URI_RESUME_ICON
};

XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

var Downloads = {
  _initialized: false,
  _dlmgr: null,
  _progressAlert: null,
  _privateDownloads: [],
  _showingPrompt: false,

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
    Services.obs.addObserver(this, "Notification:Event", true);
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

    OS.File.remove(f.path);
  },

  getNotificationIdFromDownload: function dl_getNotificationIdFromDownload(aDownload) {
    return aDownload.target.spec.replace("file:", "download:");
  },

  showNotification: function dl_showNotification(aDownload, aMessage, aTitle, aOptions) {
    let msg = {
        type: "Notification:Show",
        id: this.getNotificationIdFromDownload(aDownload),
        title: aTitle,
        smallIcon: URI_GENERIC_ICON_DOWNLOAD,
        text: aMessage,
        ongoing: false,
        cookie: aDownload.guid,
        when: aDownload.startTime
    };

    if (aOptions && aOptions.icon) {
      msg.smallIcon = aOptions.icon;
    }

    if (aOptions && aOptions.percentComplete) {
      msg.progress_value = aOptions.percentComplete;
      msg.progress_max = 100;
    }
    if (aOptions && aOptions.actions) {
      msg.actions = aOptions.actions;
    }
    if (aOptions && aOptions.ongoing) {
      msg.ongoing = aOptions.ongoing;
    }
    this._bridge.handleGeckoMessage(JSON.stringify(msg));
  },

  removeNotification: function dl_removeNotification(aDownload) {
    let msg = {
        type: "Notification:Hide",
        id: this.getNotificationIdFromDownload(aDownload)
    };
    this._bridge.handleGeckoMessage(JSON.stringify(msg));
  },

  showCancelConfirmPrompt: function dl_showCancelConfirmPrompt(aDownload) {
    if (this._showingPrompt)
      return;
    this._showingPrompt = true;
    // Open a prompt that offers a choice to cancel the download
    let title = Strings.browser.GetStringFromName("downloadCancelPromptTitle");
    let message = Strings.browser.GetStringFromName("downloadCancelPromptMessage");
    let flags = Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_YES +
                Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_NO;
    let choice = Services.prompt.confirmEx(null, title, message, flags,
                                           null, null, null, null, {});
    if (choice == 0)
      this.cancelDownload(aDownload);
    this._showingPrompt = false;
  },

  handleClickEvent: function dl_handleClickEvent(aDownload) {
    // Only open the downloaded file if the download is complete
    if (aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_FINISHED)
      this.openDownload(aDownload);
    else if (aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING ||
                aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_PAUSED)
      this.showCancelConfirmPrompt(aDownload);
  },

  handleNotificationEvent: function dl_handleNotificationEvent(aNotifData, aDownload) {
    switch (aNotifData.eventType) {
      case "notification-clicked":
        this.handleClickEvent(aDownload);
        break;
      case "cancel":
        this.cancelDownload(aDownload);
        break;
      case "pause":
        aDownload.pause();
        break;
      case "resume":
        aDownload.resume();
        break;
      case "notification-cleared":
        // notification cleared by the user
        break;
    }
  },

  // observer for last-pb-context-exited
  observe: function dl_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "Notification:Event": {
        let data = JSON.parse(aData);
        let guid = data.cookie;
        this._dlmgr.getDownloadByGUID(guid, (function(status, download) {
          if (Components.isSuccessCode(status))
            this.handleNotificationEvent(data, download);
        }).bind(this));
        break;
      }
      case "last-pb-context-exited": {
        let download;
        while ((download = this._privateDownloads.pop())) {
          try {
            Downloads.removeNotification(download);
          } catch (e) {
            dump("Error removing private download: " + e);
          }
        }
        break;
      }
    }
  },

  QueryInterface: function (aIID) {
    if (!aIID.equals(Ci.nsISupports) &&
        !aIID.equals(Ci.nsIObserver) &&
        !aIID.equals(Ci.nsISupportsWeakReference))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  },

  get _bridge() {
    delete this._bridge;
    return this._bridge = Cc["@mozilla.org/android/bridge;1"].getService(Ci.nsIAndroidBridge);

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
      Downloads.showNotification(aDownload, strings.GetStringFromName("alertDownloadsNoSpace"),
                                     strings.GetStringFromName("alertDownloadsSize"));
      aDownload.cancel();
    }

    if (aDownload.percentComplete == -1) {
      // Undetermined progress is not supported yet
      return;
    }
    Downloads.showNotification(aDownload, aDownload.percentComplete + "%",
                            aDownload.displayName, { percentComplete: aDownload.percentComplete,
                                                     ongoing: true,
                                                     actions: [PAUSE_ACTION, CANCEL_ACTION] });

  },

  onDownloadStateChange: function(aState, aDownload) {
    let state = aDownload.state;
    switch (state) {
      case Ci.nsIDownloadManager.DOWNLOAD_QUEUED:
        NativeWindow.toast.show(Strings.browser.GetStringFromName("alertDownloadsToast"), "long");
        Downloads.showNotification(aDownload, Strings.browser.GetStringFromName("alertDownloadsStart2"),
                            aDownload.displayName);
        break;
      case Ci.nsIDownloadManager.DOWNLOAD_PAUSED:
        Downloads.showNotification(aDownload, aDownload.percentComplete + "%",
                            aDownload.displayName, { percentComplete: aDownload.percentComplete,
                                                     ongoing: true,
                                                     actions: [RESUME_ACTION, CANCEL_ACTION] });
        break;
      case Ci.nsIDownloadManager.DOWNLOAD_FAILED:
      case Ci.nsIDownloadManager.DOWNLOAD_CANCELED:
      case Ci.nsIDownloadManager.DOWNLOAD_BLOCKED_PARENTAL:
      case Ci.nsIDownloadManager.DOWNLOAD_DIRTY:
      case Ci.nsIDownloadManager.DOWNLOAD_FINISHED: {
        Downloads.removeNotification(aDownload);
        if (aDownload.isPrivate) {
          let index = Downloads._privateDownloads.indexOf(aDownload);
          if (index != -1) {
            Downloads._privateDownloads.splice(index, 1);
          }
        }

        if (state == Ci.nsIDownloadManager.DOWNLOAD_FINISHED) {
          Downloads.showNotification(aDownload, Strings.browser.GetStringFromName("alertDownloadsDone2"),
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
