/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["DownloadNotifications"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Downloads", "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils", "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Notifications", "resource://gre/modules/Notifications.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Snackbars", "resource://gre/modules/Snackbars.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UITelemetry", "resource://gre/modules/UITelemetry.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ParentalControls",
  "@mozilla.org/parental-controls-service;1", "nsIParentalControlsService");

var Log = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.i.bind(null, "DownloadNotifications");

XPCOMUtils.defineLazyGetter(this, "strings",
                            () => Services.strings.createBundle("chrome://browser/locale/browser.properties"));

Object.defineProperty(this, "window",
                      { get: () => Services.wm.getMostRecentWindow("navigator:browser") });

const kButtons = {
  PAUSE: new DownloadNotificationButton("pause",
                                        "drawable://pause",
                                        "alertDownloadsPause"),
  RESUME: new DownloadNotificationButton("resume",
                                         "drawable://play",
                                         "alertDownloadsResume"),
  CANCEL: new DownloadNotificationButton("cancel",
                                         "drawable://close",
                                         "alertDownloadsCancel")
};

var notifications = new Map();

var DownloadNotifications = {
  _notificationKey: "downloads",

  init: function () {
    Downloads.getList(Downloads.ALL)
             .then(list => list.addView(this))
             .then(() => this._viewAdded = true, Cu.reportError);

    // All click, cancel, and button presses will be handled by this handler as part of the Notifications callback API.
    Notifications.registerHandler(this._notificationKey, this);
  },

  onDownloadAdded: function (download) {
    // Don't create notifications for pre-existing succeeded downloads.
    // We still add notifications for canceled downloads in case the
    // user decides to retry the download.
    if (download.succeeded && !this._viewAdded) {
      return;
    }

    if (!ParentalControls.isAllowed(ParentalControls.DOWNLOAD)) {
      download.cancel().catch(Cu.reportError);
      download.removePartialData().catch(Cu.reportError);
      Snackbars.show(strings.GetStringFromName("downloads.disabledInGuest"), Snackbars.LENGTH_LONG);
      return;
    }

    let notification = new DownloadNotification(download);
    notifications.set(download, notification);
    notification.showOrUpdate();

    // If this is a new download, show a snackbar as well.
    if (this._viewAdded) {
      Snackbars.show(strings.GetStringFromName("alertDownloadsToast"), Snackbars.LENGTH_LONG);
    }
  },

  onDownloadChanged: function (download) {
    let notification = notifications.get(download);

    if (download.succeeded) {
      let file = new FileUtils.File(download.target.path);

      Snackbars.show(strings.formatStringFromName("alertDownloadSucceeded", [file.leafName], 1), Snackbars.LENGTH_LONG, {
        action: {
          label: strings.GetStringFromName("helperapps.open"),
          callback: () => {
            UITelemetry.addEvent("launch.1", "toast", null, "downloads");
            try {
              file.launch();
            } catch (ex) {
              this.showInAboutDownloads(download);
            }
            if (notification) {
              notification.hide();
            }
          }
        }});
    }

    if (notification) {
      notification.showOrUpdate();
    }
  },

  onDownloadRemoved: function (download) {
    let notification = notifications.get(download);
    if (!notification) {
      Cu.reportError("Download doesn't have a notification.");
      return;
    }

    notification.hide();
    notifications.delete(download);
  },

  _findDownloadForCookie: function(cookie) {
    return Downloads.getList(Downloads.ALL)
      .then(list => list.getAll())
      .then((downloads) => {
        for (let download of downloads) {
          let cookie2 = getCookieFromDownload(download);
          if (cookie2 === cookie) {
            return download;
          }
        }

        throw "Couldn't find download for " + cookie;
      });
  },

  onCancel: function(cookie) {
    // TODO: I'm not sure what we do here...
  },

  showInAboutDownloads: function (download) {
    let hash = "#" + window.encodeURIComponent(download.target.path);

    // Force using string equality to find a tab
    window.BrowserApp.selectOrAddTab("about:downloads" + hash, null, { startsWith: true });
  },

  onClick: function(cookie) {
    this._findDownloadForCookie(cookie).then((download) => {
      if (download.succeeded) {
        // We don't call Download.launch(), because there's (currently) no way to
        // tell if the file was actually launched or not, and we want to show
        // about:downloads if the launch failed.
        let file = new FileUtils.File(download.target.path);
        try {
          file.launch();
        } catch (ex) {
          this.showInAboutDownloads(download);
        }
      } else {
        ConfirmCancelPrompt.show(download);
      }
    }).catch(Cu.reportError);
  },

  onButtonClick: function(button, cookie) {
    this._findDownloadForCookie(cookie).then((download) => {
      if (button === kButtons.PAUSE.buttonId) {
        download.cancel().catch(Cu.reportError);
      } else if (button === kButtons.RESUME.buttonId) {
        download.start().catch(Cu.reportError);
      } else if (button === kButtons.CANCEL.buttonId) {
        download.cancel().catch(Cu.reportError);
        download.removePartialData().catch(Cu.reportError);
      }
    }).catch(Cu.reportError);
  },
};

function getCookieFromDownload(download) {
  return download.target.path +
         download.source.url +
         download.startTime;
}

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
      icon: "drawable://alert_download",
      cookie: getCookieFromDownload(this.download),
      handlerKey: DownloadNotifications._notificationKey
    };

    if (this._downloading) {
      options.icon = "drawable://alert_download_animation";
      if (this.download.currentBytes == 0) {
        this._updateOptionsForStatic(options, "alertDownloadsStart2");
      } else {
        let buttons = this.download.hasPartialData ? [kButtons.PAUSE, kButtons.CANCEL] :
                                                     [kButtons.CANCEL]
        this._updateOptionsForOngoing(options, buttons);
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
      } else if (!this.options.ongoing) {
        // We need to explictly cancel ongoing notifications,
        // since updating them to be non-ongoing doesn't seem
        // to work. See bug 1130834.
        Notifications.cancel(this.id);
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
};

var ConfirmCancelPrompt = {
  show: function (download) {
    // Open a prompt that offers a choice to cancel the download
    let title = strings.GetStringFromName("downloadCancelPromptTitle1");
    let message = strings.GetStringFromName("downloadCancelPromptMessage1");

    if (Services.prompt.confirm(null, title, message)) {
      download.cancel().catch(Cu.reportError);
      download.removePartialData().catch(Cu.reportError);
    }
  }
};

function DownloadNotificationButton(buttonId, iconUrl, titleStringName, onClicked) {
  this.buttonId = buttonId;
  this.title = strings.GetStringFromName(titleStringName);
  this.icon = iconUrl;
}
