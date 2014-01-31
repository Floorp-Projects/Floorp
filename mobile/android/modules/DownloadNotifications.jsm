/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["DownloadNotifications"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Downloads", "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Notifications", "resource://gre/modules/Notifications.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "strings",
                            () => Services.strings.createBundle("chrome://browser/locale/browser.properties"));
Object.defineProperty(this, "nativeWindow",
                      { get: () => Services.wm.getMostRecentWindow("navigator:browser").NativeWindow });

const kButtons = {
  PAUSE: new DownloadNotificationButton("pause",
                                        "drawable://pause",
                                        "alertDownloadsPause",
                                        notification => notification.pauseDownload()),
  RESUME: new DownloadNotificationButton("resume",
                                         "drawable://play",
                                         "alertDownloadsResume",
                                         notification => notification.resumeDownload()),
  CANCEL: new DownloadNotificationButton("cancel",
                                         "drawable://close",
                                         "alertDownloadsCancel",
                                         notification => notification.cancelDownload())
};

let notifications = new Map();

var DownloadNotifications = {
  init: function () {
    if (!this._viewAdded) {
      Downloads.getList(Downloads.ALL)
               .then(list => list.addView(this))
               .then(null, Cu.reportError);

      this._viewAdded = true;
    }
  },

  uninit: function () {
    if (this._viewAdded) {
      Downloads.getList(Downloads.ALL)
               .then(list => list.removeView(this))
               .then(null, Cu.reportError);

      for (let notification of notifications.values()) {
        notification.hide();
      }

      this._viewAdded = false;
    }
  },

  onDownloadAdded: function (download) {
    let notification = new DownloadNotification(download);
    notifications.set(download, notification);

    notification.showOrUpdate();
    if (download.currentBytes == 0) {
      nativeWindow.toast.show(strings.GetStringFromName("alertDownloadsToast"), "long");
    }
  },

  onDownloadChanged: function (download) {
    let notification = notifications.get(download);
    if (!notification) {
      Cu.reportError("Download doesn't have a notification.");
      return;
    }

    notification.showOrUpdate();
  },

  onDownloadRemoved: function (download) {
    let notification = notifications.get(download);
    if (!notification) {
      Cu.reportError("Download doesn't have a notification.");
      return;
    }

    notification.hide();
    notifications.delete(download);
  }
};

function DownloadNotification(download) {
  this.download = download;
  this._fileName = OS.Path.basename(download.target.path);

  this.id = null;
}

DownloadNotification.prototype = {
  _updateFromDownload: function () {
    this._downloading = !this.download.stopped;
    this._paused = this.download.canceled && this.download.hasPartialData;
    this._succeeded = this.download.succeeded;

    this._show = this._downloading || this._paused || this._succeeded;
  },

  get options() {
    if (!this._show) {
      return null;
    }

    let options = {
      icon : "drawable://alert_download",
      onClick : (id, cookie) => this.onClick(),
      onCancel : (id, cookie) => this._notificationId = null,
      cookie : this.download
    };

    if (this._downloading) {
      if (this.download.currentBytes == 0) {
        this._updateOptionsForStatic(options, "alertDownloadsStart2");
      } else {
        this._updateOptionsForOngoing(options, [kButtons.PAUSE, kButtons.CANCEL]);
      }
    } else if (this._paused) {
      this._updateOptionsForOngoing(options, [kButtons.RESUME, kButtons.CANCEL]);
    } else if (this._succeeded) {
      options.persistent = false;
      this._updateOptionsForStatic(options, "alertDownloadsDone2");
    }

    return options;
  },

  _updateOptionsForStatic : function (options, titleName) {
    options.title = strings.GetStringFromName(titleName);
    options.message = this._fileName;
  },

  _updateOptionsForOngoing: function (options, buttons) {
    options.title = this._fileName;
    options.message = this.download.progress + "%";
    options.buttons = buttons;
    options.ongoing = true;
    options.progress = this.download.progress;
    options.persistent = true;
  },

  showOrUpdate: function () {
    this._updateFromDownload();

    if (this._show) {
      if (!this.id) {
        this.id = Notifications.create(this.options);
      } else {
        Notifications.update(this.id, this.options);
      }
    } else {
      this.hide();
    }
  },

  hide: function () {
    if (this.id) {
      Notifications.cancel(this.id);
      this.id = null;
    }
  },

  onClick: function () {
    if (this.download.succeeded) {
      this.download.launch().then(null, Cu.reportError);
    } else {
      ConfirmCancelPrompt.show(this);
    }
  },

  pauseDownload: function () {
    this.download.cancel().then(null, Cu.reportError);
  },

  resumeDownload: function () {
    this.download.start().then(null, Cu.reportError);
  },

  cancelDownload: function () {
    this.hide();

    this.download.cancel().then(null, Cu.reportError);
    this.download.removePartialData().then(null, Cu.reportError);
  }
};

var ConfirmCancelPrompt = {
  showing: false,
  show: function (downloadNotification) {
    if (this.showing) {
      return;
    }

    this.showing = true;
    // Open a prompt that offers a choice to cancel the download
    let title = strings.GetStringFromName("downloadCancelPromptTitle");
    let message = strings.GetStringFromName("downloadCancelPromptMessage");

    if (Services.prompt.confirm(null, title, message)) {
      downloadNotification.cancelDownload();
    }
    this.showing = false;
  }
};

function DownloadNotificationButton(buttonId, iconUrl, titleStringName, onClicked) {
  this.buttonId = buttonId;
  this.title = strings.GetStringFromName(titleStringName);
  this.icon = iconUrl;
  this.onClicked = (id, download) => {
    let notification = notifications.get(download);
    if (!notification) {
      Cu.reportError("No DownloadNotification for button");
      return;
    }

    onClicked(notification);
  }
}
