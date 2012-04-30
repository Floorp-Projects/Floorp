/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces, Cc = Components.classes, Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DownloadUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let gStrings = Services.strings.createBundle("chrome://browser/locale/aboutDownloads.properties");

let downloadTemplate =
"<li downloadID='{id}' role='button' mozDownload=''>" +
  "<img class='icon' src='{icon}'/>" +
  "<div class='details'>" +
     "<div class='row'>" +
       // This is a hack so that we can crop this label in its center
       "<xul:label class='title' crop='center' value='{target}'/>" +
       "<div class='date'>{date}</div>" +
     "</div>" +
     "<div class='size'>{size}</div>" +
     "<div class='domain'>{domain}</div>" +
  "</div>" +
"</li>";

XPCOMUtils.defineLazyGetter(window, "gChromeWin", function ()
  window.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShellTreeItem)
    .rootTreeItem
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindow)
    .QueryInterface(Ci.nsIDOMChromeWindow));

let Downloads = {
  init: function () {
    this._list = document.getElementById("downloads-list");
    this._list.addEventListener("click", function (event) {
      let target = event.target;
      while (target && target.nodeName != "li") {
        target = target.parentNode;
      }

      Downloads.openDownload(target);
    }, false);

    let contextmenus = gChromeWin.NativeWindow.contextmenus;
    Downloads.openMenuItem = contextmenus.add(gStrings.GetStringFromName("downloadAction.open"),
                                              contextmenus.SelectorContext("li[mozDownload]"),
      function (aTarget) {
        Downloads.openDownload(aTarget);
      }
    );
    Downloads.removeMenuItem = contextmenus.add(gStrings.GetStringFromName("downloadAction.remove"),
                                                contextmenus.SelectorContext("li[mozDownload]"),
      function (aTarget) {
        Downloads.removeDownload(aTarget);
      }
    );

    Services.obs.addObserver(this, "dl-failed", false);
    Services.obs.addObserver(this, "dl-done", false);
    Services.obs.addObserver(this, "dl-cancel", false);

    this.getDownloads();
  },

  uninit: function () {
    let contextmenus = gChromeWin.NativeWindow.contextmenus;
    contextmenus.remove(this.openMenuItem);
    contextmenus.remove(this.removeMenuItem);

    Services.obs.removeObserver(this, "dl-failed");
    Services.obs.removeObserver(this, "dl-done");
    Services.obs.removeObserver(this, "dl-cancel");
  },

  observe: function (aSubject, aTopic, aData) {
    let download = aSubject.QueryInterface(Ci.nsIDownload);
    switch (aTopic) {
      case "dl-failed":
      case "dl-cancel":
      case "dl-done":
        if (!this._getElementForDownload(download.id)) {
          let item = this._createItem(downloadTemplate, {
            id: download.id,
            target: download.displayName,
            icon: "moz-icon://" + download.displayName + "?size=64",
            date: DownloadUtils.getReadableDates(new Date())[0],
            domain: DownloadUtils.getURIHost(download.source.spec)[0],
            size: DownloadUtils.convertByteUnits(download.size).join("")
          });
          this._list.insertAdjacentHTML("afterbegin", item);
          break;
        }
    }
  },

  _initStatement: function dv__initStatement() {
    if (this._stmt)
      this._stmt.finalize();

    this._stmt = this._dlmgr.DBConnection.createStatement(
      "SELECT id, name, source, state, startTime, endTime, referrer, " +
             "currBytes, maxBytes, state IN (?1, ?2, ?3, ?4, ?5) isActive " +
      "FROM moz_downloads " +
      "WHERE NOT isActive " +
      "ORDER BY endTime DESC");
  },

  _createItem: function _createItem(aTemplate, aValues) {
    function htmlEscape(s) {
      s = s.replace(/&/g, "&amp;");
      s = s.replace(/>/g, "&gt;");
      s = s.replace(/</g, "&lt;");
      s = s.replace(/"/g, "&quot;");
      s = s.replace(/'/g, "&apos;");
      return s;
    }

    let t = aTemplate;
    for (let key in aValues) {
      if (aValues.hasOwnProperty(key)) {
        let regEx = new RegExp("{" + key + "}", "g");
        let value = htmlEscape(aValues[key].toString());
        t = t.replace(regEx, value);
      }
    }
    return t;
  },

  _stepDownloads: function dv__stepDownloads(aNumItems) {
    try {
      if (!this._stmt.executeStep()) {
        this._stmt.finalize();
        this._stmt = null;
        return;
      }
  
      // Try to get the attribute values from the statement
      let attrs = {
        id: this._stmt.row.id,
        target: this._stmt.row.name,
        icon: "moz-icon://" + this._stmt.row.name + "?size=64",
        date: DownloadUtils.getReadableDates(new Date(this._stmt.row.endTime / 1000))[0],
        domain: DownloadUtils.getURIHost(this._stmt.row.source)[0],
        size: DownloadUtils.convertByteUnits(this._stmt.row.maxBytes).join("")
      };

      let item = this._createItem(downloadTemplate, attrs);
      this._list.insertAdjacentHTML("beforeend", item);
    } catch (e) {
      // Something went wrong when stepping or getting values, so clear and quit
      console.log("Error: " + e);
      this._stmt.reset();
      return;
    }

    // Add another item to the list if we should; otherwise, let the UI update
    // and continue later
    if (aNumItems > 1) {
      this._stepDownloads(aNumItems - 1);
    } else {
      // Use a shorter delay for earlier downloads to display them faster
      let delay = Math.min(this._list.itemCount * 10, 300);
      let self = this;
      this._timeoutID = setTimeout(function () { self._stepDownloads(5); }, delay);
    }
  },

  getDownloads: function () {
    this._dlmgr = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);

    this._initStatement();

    clearTimeout(this._timeoutID);

    this._stmt.reset();
    this._stmt.bindInt32Parameter(0, Ci.nsIDownloadManager.DOWNLOAD_NOTSTARTED);
    this._stmt.bindInt32Parameter(1, Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING);
    this._stmt.bindInt32Parameter(2, Ci.nsIDownloadManager.DOWNLOAD_PAUSED);
    this._stmt.bindInt32Parameter(3, Ci.nsIDownloadManager.DOWNLOAD_QUEUED);
    this._stmt.bindInt32Parameter(4, Ci.nsIDownloadManager.DOWNLOAD_SCANNING);

    // Take a quick break before we actually start building the list
    let self = this;
    this._timeoutID = setTimeout(function () {
      self._stepDownloads(1);
    }, 0);
  },

  _getElementForDownload: function (aKey) {
    return this._list.querySelector("li[downloadID='" + aKey + "']");
  },

  _getDownloadForElement: function (aElement) {
    let id = parseInt(aElement.getAttribute("downloadID"));
    return this._dlmgr.getDownload(id);
  },

  openDownload: function (aItem) {
    let f = null;
    try {
      let download = this._getDownloadForElement(aItem);
      f = download.targetFile;
    } catch(ex) { }

    try {
      if (f) f.launch();
    } catch (ex) { }
  },

  removeDownload: function (aItem) {
    let f = null;
    try {
      let download = this._getDownloadForElement(aItem);
      f = download.targetFile;
    } catch(ex) {
      // even if there is no file, pretend that there is so that we can remove
      // it from the list
      f = { leafName: "" };
    }

    this._dlmgr.removeDownload(aItem.getAttribute("downloadID"));

    this._list.removeChild(aItem);

    try {
      if (f) f.remove(false);
    } catch(ex) { }
  },
}
