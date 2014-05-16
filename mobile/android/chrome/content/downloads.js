// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cu = Components.utils;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function dump(a) {
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(a);
}

XPCOMUtils.defineLazyModuleGetter(this, "Notifications",
                                  "resource://gre/modules/Notifications.jsm");

const URI_GENERIC_ICON_DOWNLOAD = "drawable://alert_download";
const URI_PAUSE_ICON = "drawable://pause";
const URI_CANCEL_ICON = "drawable://close";
const URI_RESUME_ICON = "drawable://play";


XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

var Downloads = {
  _initialized: false,
  _dlmgr: null,
  _progressAlert: null,
  _privateDownloads: [],
  _showingPrompt: false,
  _downloadsIdMap: {},

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

    OS.File.remove(f.path);
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

  clickCallback: function dl_clickCallback(aDownloadId) {
    this._dlmgr.getDownloadByGUID(aDownloadId, (function(status, download) {
          if (Components.isSuccessCode(status))
            this.handleClickEvent(download);
        }).bind(this));
  },

  pauseClickCallback: function dl_buttonPauseCallback(aDownloadId) {
    this._dlmgr.getDownloadByGUID(aDownloadId, (function(status, download) {
          if (Components.isSuccessCode(status))
            download.pause();
        }).bind(this));
  },

  resumeClickCallback: function dl_buttonPauseCallback(aDownloadId) {
    this._dlmgr.getDownloadByGUID(aDownloadId, (function(status, download) {
          if (Components.isSuccessCode(status))
            download.resume();
        }).bind(this));
  },

  cancelClickCallback: function dl_buttonPauseCallback(aDownloadId) {
    this._dlmgr.getDownloadByGUID(aDownloadId, (function(status, download) {
          if (Components.isSuccessCode(status))
            this.cancelDownload(download);
        }).bind(this));
  },

  notificationCanceledCallback: function dl_notifCancelCallback(aId, aDownloadId) {
    let notificationId = this._downloadsIdMap[aDownloadId];
    if (notificationId && notificationId == aId)
      delete this._downloadsIdMap[aDownloadId];
  },

  createNotification: function dl_createNotif(aDownload, aOptions) {
    let notificationId = Notifications.create(aOptions);
    this._downloadsIdMap[aDownload.guid] = notificationId;
  },

  updateNotification: function dl_updateNotif(aDownload, aOptions) {
    let notificationId = this._downloadsIdMap[aDownload.guid];
    if (notificationId)
      Notifications.update(notificationId, aOptions);
  },

  cancelNotification: function dl_cleanNotif(aDownload) {
    Notifications.cancel(this._downloadsIdMap[aDownload.guid]);
    delete this._downloadsIdMap[aDownload.guid];
  },

  // observer for last-pb-context-exited
  observe: function dl_observe(aSubject, aTopic, aData) {
    let download;
    while ((download = this._privateDownloads.pop())) {
      try {
        let notificationId = aDownload.guid;
        Notifications.clear(notificationId);
        Downloads.removeNotification(download);
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

const PAUSE_BUTTON = {
  buttonId: "pause",
  title : Strings.browser.GetStringFromName("alertDownloadsPause"),
  icon : URI_PAUSE_ICON,
  onClicked: function (aId, aCookie) {
    Downloads.pauseClickCallback(aCookie);
  }
};

const CANCEL_BUTTON = {
  buttonId: "cancel",
  title : Strings.browser.GetStringFromName("alertDownloadsCancel"),
  icon : URI_CANCEL_ICON,
  onClicked: function (aId, aCookie) {
    Downloads.cancelClickCallback(aCookie);
  }
};

const RESUME_BUTTON = {
  buttonId: "resume",
  title : Strings.browser.GetStringFromName("alertDownloadsResume"),
  icon: URI_RESUME_ICON,
  onClicked: function (aId, aCookie) {
    Downloads.resumeClickCallback(aCookie);
  }
};

function DownloadNotifOptions (aDownload, aTitle, aMessage) {
  this.icon = URI_GENERIC_ICON_DOWNLOAD;
  this.onCancel = function (aId, aCookie) {
    Downloads.notificationCanceledCallback(aId, aCookie);
  }
  this.onClick = function (aId, aCookie) {
    Downloads.clickCallback(aCookie);
  }
  this.title = aTitle;
  this.message = aMessage;
  this.buttons = null;
  this.cookie = aDownload.guid;
  this.persistent = true;
}

function DownloadProgressNotifOptions (aDownload, aButtons) {
  DownloadNotifOptions.apply(this, [aDownload, aDownload.displayName, aDownload.percentComplete + "%"]);
  this.ongoing = true;
  this.progress = aDownload.percentComplete;
  this.buttons = aButtons;
}

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
      Downloads.updateNotification(aDownload, new DownloadNotifOptions(aDownload,
                                                                        strings.GetStringFromName("alertDownloadsNoSpace"),
                                                                        strings.GetStringFromName("alertDownloadsSize")));
      aDownload.cancel();
    }

    if (aDownload.percentComplete == -1) {
      // Undetermined progress is not supported yet
      return;
    }

    Downloads.updateNotification(aDownload, new DownloadProgressNotifOptions(aDownload, [PAUSE_BUTTON, CANCEL_BUTTON]));
  },

  onDownloadStateChange: function(aState, aDownload) {
    let state = aDownload.state;
    switch (state) {
      case Ci.nsIDownloadManager.DOWNLOAD_QUEUED: {
        NativeWindow.toast.show(Strings.browser.GetStringFromName("alertDownloadsToast"), "long");
        Downloads.createNotification(aDownload, new DownloadNotifOptions(aDownload,
                                                                         Strings.browser.GetStringFromName("alertDownloadsStart2"),
                                                                         aDownload.displayName));
        break;
      }
      case Ci.nsIDownloadManager.DOWNLOAD_PAUSED: {
        Downloads.updateNotification(aDownload, new DownloadProgressNotifOptions(aDownload, [RESUME_BUTTON, CANCEL_BUTTON]));
        break;
      }
      case Ci.nsIDownloadManager.DOWNLOAD_FAILED:
      case Ci.nsIDownloadManager.DOWNLOAD_CANCELED:
      case Ci.nsIDownloadManager.DOWNLOAD_BLOCKED_PARENTAL:
      case Ci.nsIDownloadManager.DOWNLOAD_DIRTY:
      case Ci.nsIDownloadManager.DOWNLOAD_FINISHED: {
        Downloads.cancelNotification(aDownload);
        if (aDownload.isPrivate) {
          let index = Downloads._privateDownloads.indexOf(aDownload);
          if (index != -1) {
            Downloads._privateDownloads.splice(index, 1);
          }
        }

        if (state == Ci.nsIDownloadManager.DOWNLOAD_FINISHED) {
          Downloads.createNotification(aDownload, new DownloadNotifOptions(aDownload,
                                                                Strings.browser.GetStringFromName("alertDownloadsDone2"),
                                                                aDownload.displayName));
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
