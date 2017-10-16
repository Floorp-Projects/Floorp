/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "Downloads", "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadUtils", "resource://gre/modules/DownloadUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "EventDispatcher", "resource://gre/modules/Messaging.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PluralForm", "resource://gre/modules/PluralForm.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

var gStrings = Services.strings.createBundle("chrome://browser/locale/aboutDownloads.properties");
XPCOMUtils.defineLazyGetter(this, "strings",
                            () => Services.strings.createBundle("chrome://browser/locale/aboutDownloads.properties"));

function deleteDownload(download) {
  download.finalize(true).catch(Cu.reportError);
  OS.File.remove(download.target.path).catch(ex => {
    if (!(ex instanceof OS.File.Error && ex.becauseNoSuchFile)) {
      Cu.reportError(ex);
    }
  });
}

var contextMenu = {
  _items: [],
  _targetDownload: null,

  init: function() {
    let element = document.getElementById("downloadmenu");
    element.addEventListener("click",
                             event => event.download = this._targetDownload,
                             true);
    this._items = [
      new ContextMenuItem("open",
                          download => download.succeeded,
                          download => download.launch().catch(Cu.reportError)),
      new ContextMenuItem("retry",
                          download => download.error ||
                                      (download.canceled && !download.hasPartialData),
                          download => download.start().catch(Cu.reportError)),
      new ContextMenuItem("remove",
                          download => download.stopped,
                          download => {
                            Downloads.getList(Downloads.ALL)
                                     .then(list => list.remove(download))
                                     .catch(Cu.reportError);
                            deleteDownload(download);
                          }),
      new ContextMenuItem("pause",
                          download => !download.stopped && download.hasPartialData,
                          download => download.cancel().catch(Cu.reportError)),
      new ContextMenuItem("resume",
                          download => download.canceled && download.hasPartialData,
                          download => download.start().catch(Cu.reportError)),
      new ContextMenuItem("cancel",
                          download => !download.stopped ||
                                      (download.canceled && download.hasPartialData),
                          download => {
                            download.cancel().catch(Cu.reportError);
                            download.removePartialData().catch(Cu.reportError);
                          }),
      // following menu item is a global action
      new ContextMenuItem("removeall",
                          () => downloadLists.finished.length > 0,
                          () => downloadLists.removeFinished())
    ];
  },

  addContextMenuEventListener: function(element) {
    element.addEventListener("contextmenu", this.onContextMenu.bind(this));
  },

  onContextMenu: function(event) {
    let target = event.target;
    while (target && !target.download) {
      target = target.parentNode;
    }
    if (!target) {
      Cu.reportError("No download found for context menu target");
      event.preventDefault();
      return;
    }

    // capture the target download for menu items to use in a click event
    this._targetDownload = target.download;
    for (let item of this._items) {
      item.updateVisibility(target.download);
    }
  }
};

function ContextMenuItem(name, isVisible, action) {
  this.element = document.getElementById("contextmenu-" + name);
  this.isVisible = isVisible;

  this.element.addEventListener("click", event => action(event.download));
}

ContextMenuItem.prototype = {
  updateVisibility: function(download) {
    this.element.hidden = !this.isVisible(download);
  }
};

function DownloadListView(type, listElementId) {
  this.listElement = document.getElementById(listElementId);
  contextMenu.addContextMenuEventListener(this.listElement);

  this.items = new Map();

  Downloads.getList(type)
           .then(list => list.addView(this))
           .catch(Cu.reportError);

  window.addEventListener("unload", event => {
    Downloads.getList(type)
             .then(list => list.removeView(this))
             .catch(Cu.reportError);
  });
}

DownloadListView.prototype = {
  get finished() {
    let finished = [];
    for (let download of this.items.keys()) {
      if (download.stopped && (!download.hasPartialData || download.error)) {
        finished.push(download);
      }
    }

    return finished;
  },

  insertOrMoveItem: function(item) {
    var compare = (a, b) => {
      // active downloads always before stopped downloads
      if (a.stopped != b.stopped) {
        return b.stopped ? -1 : 1;
      }
      // most recent downloads first
      return b.startTime - a.startTime;
    };

    let insertLocation = this.listElement.firstChild;
    while (insertLocation && compare(item.download, insertLocation.download) > 0) {
      insertLocation = insertLocation.nextElementSibling;
    }
    this.listElement.insertBefore(item.element, insertLocation);
  },

  onDownloadAdded: function(download) {
    let item = new DownloadItem(download);
    this.items.set(download, item);
    this.insertOrMoveItem(item);
  },

  onDownloadChanged: function(download) {
    let item = this.items.get(download);
    if (!item) {
      Cu.reportError("No DownloadItem found for download");
      return;
    }

    if (item.stateChanged) {
      this.insertOrMoveItem(item);
    }

    item.onDownloadChanged();
  },

  onDownloadRemoved: function(download) {
    let item = this.items.get(download);
    if (!item) {
      Cu.reportError("No DownloadItem found for download");
      return;
    }

    this.items.delete(download);
    this.listElement.removeChild(item.element);

    EventDispatcher.instance.sendRequest({
      type: "Download:Remove",
      path: download.target.path
    });
  }
};

var downloadLists = {
  init: function() {
    this.publicDownloads = new DownloadListView(Downloads.PUBLIC, "public-downloads-list");
    this.privateDownloads = new DownloadListView(Downloads.PRIVATE, "private-downloads-list");
  },

  get finished() {
    return this.publicDownloads.finished.concat(this.privateDownloads.finished);
  },

  removeFinished: function() {
    let finished = this.finished;
    if (finished.length == 0) {
      return;
    }

    let title = strings.GetStringFromName("downloadAction.deleteAll");
    let messageForm = strings.GetStringFromName("downloadMessage.deleteAll");
    let message = PluralForm.get(finished.length, messageForm).replace("#1", finished.length);

    if (Services.prompt.confirm(null, title, message)) {
      Downloads.getList(Downloads.ALL)
               .then(list => {
                 for (let download of finished) {
                   list.remove(download).catch(Cu.reportError);
                   deleteDownload(download);
                 }
               }, Cu.reportError);
    }
  }
};

function DownloadItem(download) {
  this._download = download;
  this._updateFromDownload();

  this._domain = DownloadUtils.getURIHost(download.source.url)[0];
  this._fileName = this._htmlEscape(OS.Path.basename(download.target.path));
  this._iconUrl = "moz-icon://" + this._fileName + "?size=64";
  this._startDate = this._htmlEscape(DownloadUtils.getReadableDates(download.startTime)[0]);

  this._element = this.createElement();
}

const kDownloadStatePropertyNames = [
  "stopped",
  "succeeded",
  "canceled",
  "error",
  "startTime"
];

DownloadItem.prototype = {
  _htmlEscape: function(s) {
    s = s.replace(/&/g, "&amp;");
    s = s.replace(/>/g, "&gt;");
    s = s.replace(/</g, "&lt;");
    s = s.replace(/"/g, "&quot;");
    s = s.replace(/'/g, "&apos;");
    return s;
  },

  _updateFromDownload: function() {
    this._state = {};
    kDownloadStatePropertyNames.forEach(
      name => this._state[name] = this._download[name],
      this);
  },

  get stateChanged() {
    return kDownloadStatePropertyNames.some(
      name => this._state[name] != this._download[name],
      this);
  },

  get download() {
    return this._download;
  },
  get element() {
    return this._element;
  },

  createElement: function() {
    let template = document.getElementById("download-item");
    // TODO: use this once <template> is working
    // let element = document.importNode(template.content, true);

    // simulate a <template> node...
    let element = template.cloneNode(true);
    element.removeAttribute("id");
    element.removeAttribute("style");

    // launch the download if clicked
    element.addEventListener("click", this.onClick.bind(this));

    // set download as an expando property for the context menu
    element.download = this.download;

    // fill in template placeholders
    this.updateElement(element);

    return element;
  },

  updateElement: function(element) {
    element.querySelector(".date").textContent = this.startDate;
    element.querySelector(".domain").textContent = this.domain;
    element.querySelector(".icon").src = this.iconUrl;
    element.querySelector(".size").textContent = this.size;
    element.querySelector(".state").textContent = this.stateDescription;
    element.querySelector(".title").setAttribute("value", this.fileName);
  },

  onClick: function(event) {
    if (this.download.succeeded) {
      this.download.launch().catch(Cu.reportError);
    }
  },

  onDownloadChanged: function() {
    this._updateFromDownload();
    this.updateElement(this.element);
  },

  // template properties below
  get domain() {
    return this._domain;
  },
  get fileName() {
    return this._fileName;
  },
  get id() {
    return this._id;
  },
  get iconUrl() {
    return this._iconUrl;
  },

  get size() {
    if (this.download.succeeded && this.download.target.exists) {
      return DownloadUtils.convertByteUnits(this.download.target.size).join("");
    } else if (this.download.hasProgress) {
      return DownloadUtils.convertByteUnits(this.download.totalBytes).join("");
    }
    return strings.GetStringFromName("downloadState.unknownSize");
  },

  get startDate() {
    return this._startDate;
  },

  get stateDescription() {
    let name;
    if (this.download.error) {
      name = "downloadState.failed";
    } else if (this.download.canceled) {
      if (this.download.hasPartialData) {
        name = "downloadState.paused";
      } else {
        name = "downloadState.canceled";
      }
    } else if (!this.download.stopped) {
      if (this.download.currentBytes > 0) {
        name = "downloadState.downloading";
      } else {
        name = "downloadState.starting";
      }
    }

    if (name) {
      return strings.GetStringFromName(name);
    }
    return "";
  }
};

window.addEventListener("DOMContentLoaded", event => {
    contextMenu.init();
    downloadLists.init();
});
