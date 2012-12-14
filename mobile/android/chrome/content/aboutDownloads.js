/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces, Cc = Components.classes, Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DownloadUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/PluralForm.jsm");
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

let gStrings = Services.strings.createBundle("chrome://browser/locale/aboutDownloads.properties");

let downloadTemplate =
"<li downloadGUID='{guid}' role='button' state='{state}' contextmenu='downloadmenu'>" +
  "<img class='icon' src='{icon}'/>" +
  "<div class='details'>" +
     "<div class='row'>" +
       // This is a hack so that we can crop this label in its center
       "<xul:label class='title' crop='center' value='{target}'/>" +
       "<div class='date'>{date}</div>" +
     "</div>" +
     "<div class='size'>{size}</div>" +
     "<div class='domain'>{domain}</div>" +
     "<div class='displayState'>{displayState}</div>" +
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


var ContextMenus = {
  target: null,

  init: function() {
    document.addEventListener("contextmenu", this, false);
    this.items = [
      { name: "open", states: [Downloads._dlmgr.DOWNLOAD_FINISHED] },
      { name: "retry", states: [Downloads._dlmgr.DOWNLOAD_FAILED, Downloads._dlmgr.DOWNLOAD_CANCELED] },
      { name: "remove", states: [Downloads._dlmgr.DOWNLOAD_FINISHED,Downloads._dlmgr.DOWNLOAD_FAILED, Downloads._dlmgr.DOWNLOAD_CANCELED] },
      { name: "removeall", states: [Downloads._dlmgr.DOWNLOAD_FINISHED,Downloads._dlmgr.DOWNLOAD_FAILED, Downloads._dlmgr.DOWNLOAD_CANCELED] },
      { name: "pause", states: [Downloads._dlmgr.DOWNLOAD_DOWNLOADING] },
      { name: "resume", states: [Downloads._dlmgr.DOWNLOAD_PAUSED] },
      { name: "cancel", states: [Downloads._dlmgr.DOWNLOAD_DOWNLOADING, Downloads._dlmgr.DOWNLOAD_NOTSTARTED, Downloads._dlmgr.DOWNLOAD_QUEUED, Downloads._dlmgr.DOWNLOAD_PAUSED] },
    ];
  },

  handleEvent: function(event) {
    // store the target of context menu events so that we know which app to act on
    this.target = event.target;
    while (!this.target.hasAttribute("contextmenu")) {
      this.target = this.target.parentNode;
    }
    if (!this.target)
      return;

    let state = parseInt(this.target.getAttribute("state"));
    for (let i = 0; i < this.items.length; i++) {
      var item = this.items[i];
      let enabled = (item.states.indexOf(state) > -1);
      if (enabled)
        document.getElementById("contextmenu-" + item.name).removeAttribute("hidden");
      else
        document.getElementById("contextmenu-" + item.name).setAttribute("hidden", "true");
    }
  },

  // Open shown only for downloads that completed successfully
  open: function(event) {
    Downloads.openDownload(this.target);
    this.target = null;
  },

  // Retry shown when its failed, canceled, blocked(covered in failed, see _getState())
  retry: function (event) {
    Downloads.retryDownload(this.target);
    this.target = null;
  },

  // Remove shown when its canceled, finished, failed(failed includes blocked and dirty, see _getState())
  remove: function (event) {
    Downloads.removeDownload(this.target);
    this.target = null;
  },

  // Pause shown when item is currently downloading
  pause: function (event) {
    Downloads.pauseDownload(this.target);
    this.target = null;
  },

  // Resume shown for paused items only
  resume: function (event) {
    Downloads.resumeDownload(this.target);
    this.target = null;
  },

  // Cancel shown when its downloading, notstarted, queued or paused
  cancel: function (event) {
    Downloads.cancelDownload(this.target);
    this.target = null;
  },

  removeAll: function(event) {
    Downloads.removeAll();
    this.target = null;
  }
}


let Downloads = {
  init: function dl_init() {
    function onClick(evt) {
      let target = evt.target;
      while (target.nodeName != "li") {
        target = target.parentNode;
        if (!target)
          return;
      }

      Downloads.openDownload(target);
    }

    this._normalList = document.getElementById("normal-downloads-list");
    this._privateList = document.getElementById("private-downloads-list");

    this._normalList.addEventListener("click", onClick, false);
    this._privateList.addEventListener("click", onClick, false);

    this._dlmgr = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
    this._dlmgr.addPrivacyAwareListener(this);

    Services.obs.addObserver(this, "last-pb-context-exited", false);

    // If we have private downloads, show them all immediately. If we were to
    // add them asynchronously, there's a small chance we could get a
    // "last-pb-context-exited" notification before downloads are added to the
    // list, meaning we'd show private downloads without any private tabs open.
    let privateEntries = this.getDownloads({ isPrivate: true });
    this._stepAddEntries(privateEntries, this._privateList, privateEntries.length);

    // Add non-private downloads
    let normalEntries = this.getDownloads({ isPrivate: false });
    this._stepAddEntries(normalEntries, this._normalList, 1);
    ContextMenus.init();
  },

  uninit: function dl_uninit() {
    let contextmenus = gChromeWin.NativeWindow.contextmenus;
    contextmenus.remove(this.openMenuItem);
    contextmenus.remove(this.removeMenuItem);
    contextmenus.remove(this.pauseMenuItem);
    contextmenus.remove(this.resumeMenuItem);
    contextmenus.remove(this.retryMenuItem);
    contextmenus.remove(this.cancelMenuItem);
    contextmenus.remove(this.deleteAllMenuItem);

    this._dlmgr.removeListener(this);
    Services.obs.removeObserver(this, "last-pb-context-exited");
  },

  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
                             aCurTotalProgress, aMaxTotalProgress, aDownload) { },
  onDownloadStateChange: function(aState, aDownload) {
    switch (aDownload.state) {
      case Ci.nsIDownloadManager.DOWNLOAD_FAILED:
      case Ci.nsIDownloadManager.DOWNLOAD_CANCELED:
      case Ci.nsIDownloadManager.DOWNLOAD_BLOCKED_PARENTAL:
      case Ci.nsIDownloadManager.DOWNLOAD_DIRTY:
      case Ci.nsIDownloadManager.DOWNLOAD_FINISHED:
        // For all "completed" states, move them after active downloads
        this._moveDownloadAfterActive(this._getElementForDownload(aDownload.guid));

      // Fall-through the rest
      case Ci.nsIDownloadManager.DOWNLOAD_SCANNING:
      case Ci.nsIDownloadManager.DOWNLOAD_QUEUED:
      case Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING:
        let item = this._getElementForDownload(aDownload.guid);
        if (item)
          this._updateDownloadRow(item, aDownload);
        else
          this._insertDownloadRow(aDownload);
        break;
    }
  },
  onStateChange: function(aWebProgress, aRequest, aState, aStatus, aDownload) { },
  onSecurityChange: function(aWebProgress, aRequest, aState, aDownload) { },

  // Called when last private window is closed
  observe: function (aSubject, aTopic, aData) {
    this._privateList.innerHTML = "";
  },

  _moveDownloadAfterActive: function dl_moveDownloadAfterActive(aItem) {
    // Move downloads that just reached a "completed" state below any active
    try {
      // Iterate down until we find a non-active download
      let next = aItem.nextElementSibling;
      while (next && this._inProgress(next.getAttribute("state")))
        next = next.nextElementSibling;
      // Move the item
      aItem.parentNode.insertBefore(aItem, next);
    } catch (ex) {
      this.logError("_moveDownloadAfterActive() " + ex);
    }
  },
  
  _inProgress: function dl_inProgress(aState) {
    return [
      this._dlmgr.DOWNLOAD_NOTSTARTED,
      this._dlmgr.DOWNLOAD_QUEUED,
      this._dlmgr.DOWNLOAD_DOWNLOADING,
      this._dlmgr.DOWNLOAD_PAUSED,
      this._dlmgr.DOWNLOAD_SCANNING,
    ].indexOf(parseInt(aState)) != -1;
  },

  _insertDownloadRow: function dl_insertDownloadRow(aDownload) {
    let updatedState = this._getState(aDownload.state);
    let item = this._createItem(downloadTemplate, {
      guid: aDownload.guid,
      target: aDownload.displayName,
      icon: "moz-icon://" + aDownload.displayName + "?size=64",
      date: DownloadUtils.getReadableDates(new Date())[0],
      domain: DownloadUtils.getURIHost(aDownload.source.spec)[0],
      size: this._getDownloadSize(aDownload.size),
      displayState: this._getStateString(updatedState),
      state: updatedState
    });
    list = aDownload.isPrivate ? this._privateList : this._normalList;
    list.insertAdjacentHTML("afterbegin", item);
  },

  _getDownloadSize: function dl_getDownloadSize(aSize) {
    let displaySize = DownloadUtils.convertByteUnits(aSize);
    if (displaySize[0] > 0) // [0] is size, [1] is units
      return displaySize.join("");
    else
      return gStrings.GetStringFromName("downloadState.unknownSize");
  },
  
  // Not all states are displayed as-is on mobile, some are translated to a generic state
  _getState: function dl_getState(aState) {
    let str;
    switch (aState) {
      // Downloading and Scanning states show up as "Downloading"
      case this._dlmgr.DOWNLOAD_DOWNLOADING:
      case this._dlmgr.DOWNLOAD_SCANNING:
        str = this._dlmgr.DOWNLOAD_DOWNLOADING;
        break;
        
      // Failed, Dirty and Blocked states show up as "Failed"
      case this._dlmgr.DOWNLOAD_FAILED:
      case this._dlmgr.DOWNLOAD_DIRTY:
      case this._dlmgr.DOWNLOAD_BLOCKED_POLICY:
      case this._dlmgr.DOWNLOAD_BLOCKED_PARENTAL:
        str = this._dlmgr.DOWNLOAD_FAILED;
        break;
        
      /* QUEUED and NOTSTARTED are not translated as they 
         dont fall under a common state but we still need
         to display a common "status" on the UI */
         
      default:
        str = aState;
    }
    return str;
  },
  
  // Note: This doesn't cover all states as some of the states are translated in _getState()
  _getStateString: function dl_getStateString(aState) {
    let str;
    switch (aState) {
      case this._dlmgr.DOWNLOAD_DOWNLOADING:
        str = "downloadState.downloading";
        break;
      case this._dlmgr.DOWNLOAD_CANCELED:
        str = "downloadState.canceled";
        break;
      case this._dlmgr.DOWNLOAD_FAILED:
        str = "downloadState.failed";
        break;
      case this._dlmgr.DOWNLOAD_PAUSED:
        str = "downloadState.paused";
        break;
        
      // Queued and Notstarted show up as "Starting..."
      case this._dlmgr.DOWNLOAD_QUEUED:
      case this._dlmgr.DOWNLOAD_NOTSTARTED:
        str = "downloadState.starting";
        break;
        
      default:
        return "";
    }
    return gStrings.GetStringFromName(str);
  },

  _updateItem: function dl_updateItem(aItem, aValues) {
    for (let i in aValues) {
      aItem.querySelector("." + i).textContent = aValues[i];
    }
  },

  _initStatement: function dv__initStatement(aIsPrivate) {
    let dbConn = aIsPrivate ? this._dlmgr.privateDBConnection : this._dlmgr.DBConnection;
    return dbConn.createStatement(
      "SELECT guid, name, source, state, startTime, endTime, referrer, " +
             "currBytes, maxBytes, state IN (?1, ?2, ?3, ?4, ?5) isActive " +
      "FROM moz_downloads " +
      "ORDER BY isActive DESC, endTime DESC, startTime DESC");
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

  _getEntry: function dv__getEntry(aStmt) {
    try {
      if (!aStmt.executeStep()) {
        return null;
      }

      let updatedState = this._getState(aStmt.row.state);
      // Try to get the attribute values from the statement
      return {
        guid: aStmt.row.guid,
        target: aStmt.row.name,
        icon: "moz-icon://" + aStmt.row.name + "?size=64",
        date: DownloadUtils.getReadableDates(new Date(aStmt.row.endTime / 1000))[0],
        domain: DownloadUtils.getURIHost(aStmt.row.source)[0],
        size: this._getDownloadSize(aStmt.row.maxBytes),
        displayState: this._getStateString(updatedState),
        state: updatedState
      };

    } catch (e) {
      // Something went wrong when stepping or getting values, so clear and quit
      this.logError("_getEntry() " + e);
      aStmt.reset();
      return null;
    }
  },

  _stepAddEntries: function dv__stepAddEntries(aEntries, aList, aNumItems) {
    if (aEntries.length == 0)
      return;

    let attrs = aEntries.shift();
    let item = this._createItem(downloadTemplate, attrs);
    aList.insertAdjacentHTML("beforeend", item);

    // Add another item to the list if we should; otherwise, let the UI update
    // and continue later
    if (aNumItems > 1) {
      this._stepAddEntries(aEntries, aList, aNumItems - 1);
    } else {
      // Use a shorter delay for earlier downloads to display them faster
      let delay = Math.min(aList.itemCount * 10, 300);
      setTimeout(function () {
        this._stepAddEntries(aEntries, aList, 5);
      }.bind(this), delay);
    }
  },

  getDownloads: function dl_getDownloads(aParams) {
    aParams = aParams || {};
    let stmt = this._initStatement(aParams.isPrivate);

    stmt.reset();
    stmt.bindInt32Parameter(0, Ci.nsIDownloadManager.DOWNLOAD_NOTSTARTED);
    stmt.bindInt32Parameter(1, Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING);
    stmt.bindInt32Parameter(2, Ci.nsIDownloadManager.DOWNLOAD_PAUSED);
    stmt.bindInt32Parameter(3, Ci.nsIDownloadManager.DOWNLOAD_QUEUED);
    stmt.bindInt32Parameter(4, Ci.nsIDownloadManager.DOWNLOAD_SCANNING);

    let entries = [];
    while (entry = this._getEntry(stmt)) {
      entries.push(entry);
    }

    stmt.finalize();

    return entries;
  },

  _getElementForDownload: function dl_getElementForDownload(aKey) {
    return document.body.querySelector("li[downloadGUID='" + aKey + "']");
  },

  _getDownloadForElement: function dl_getDownloadForElement(aElement, aCallback) {
    let guid = aElement.getAttribute("downloadGUID");
    this._dlmgr.getDownloadByGUID(guid, function(status, download) {
      if (!Components.isSuccessCode(status)) {
        return;
      }
      aCallback(download);
    });
  },

  _removeItem: function dl_removeItem(aItem) {
    // Make sure we have an item to remove
    if (!aItem)
      return;
  
    aItem.parentNode.removeChild(aItem);
  },
  
  openDownload: function dl_openDownload(aItem) {
    this._getDownloadForElement(aItem, function(aDownload) {
      try {
        let f = aDownload.targetFile;
        if (f) f.launch();
      } catch (ex) {
        this.logError("openDownload() " + ex, aDownload);
      }
    });
  },

  removeDownload: function dl_removeDownload(aItem) {
    this._getDownloadForElement(aItem, function(aDownload) {
      let f = null;
      try {
        f = aDownload.targetFile;
      } catch (ex) {
        // even if there is no file, pretend that there is so that we can remove
        // it from the list
        f = { leafName: "" };
      }
      aDownload.remove();
      try {
        if (f) f.remove();
      } catch (ex) {
        this.logError("removeDownload() " + ex, aDownload);
      }
    });
    aItem.parentNode.removeChild(aItem);
  },

  removeAll: function dl_removeAll() {
    let title = gStrings.GetStringFromName("downloadAction.deleteAll");
    let messageForm = gStrings.GetStringFromName("downloadMessage.deleteAll");
    let elements = document.body.querySelectorAll("li[state='" + this._dlmgr.DOWNLOAD_FINISHED + "']," +
                                               "li[state='" + this._dlmgr.DOWNLOAD_CANCELED + "']," +
                                               "li[state='" + this._dlmgr.DOWNLOAD_FAILED + "']");
    let message = PluralForm.get(elements.length, messageForm)
                            .replace("#1", elements.length);
    let flags = Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_OK +
                Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_CANCEL;
    let choice = Services.prompt.confirmEx(null, title, message, flags,
                                           null, null, null, null, {});
    if (choice == 0) {
      for (let i = 0; i < elements.length; i++) {
        this.removeDownload(elements[i]);
      }
    }
  },

  pauseDownload: function dl_pauseDownload(aItem) {
    this._getDownloadForElement(aItem, function(aDownload) {
      try {
        aDownload.pause();
        this._updateDownloadRow(aItem, aDownload);
      } catch (ex) {
        this.logError("Error: pauseDownload() " + ex, aDownload);
      }
    }.bind(this));
  },

  resumeDownload: function dl_resumeDownload(aItem) {
    this._getDownloadForElement(aItem, function(aDownload) {
      try {
        aDownload.resume();
        this._updateDownloadRow(aItem, aDownload);
      } catch (ex) {
        this.logError("resumeDownload() " + ex, aDownload);
      }
    }.bind(this));
  },

  retryDownload: function dl_retryDownload(aItem) {
    this._getDownloadForElement(aItem, function(aDownload) {
      try {
        this._removeItem(aItem);
        aDownload.retry();
      } catch (ex) {
        this.logError("retryDownload() " + ex, aDownload);
      }
    }.bind(this));
  },

  cancelDownload: function dl_cancelDownload(aItem) {
    this._getDownloadForElement(aItem, function(aDownload) {
      try {
        aDownload.cancel();
        let f = aDownload.targetFile;

        if (f.exists())
          f.remove(false);

        this._updateDownloadRow(aItem, aDownload);
      } catch (ex) {
        this.logError("cancelDownload() " + ex, aDownload);
      }
    }.bind(this));
  },

  _updateDownloadRow: function dl_updateDownloadRow(aItem, aDownload) {
    try {
      let updatedState = this._getState(aDownload.state);
      aItem.setAttribute("state", updatedState);
      this._updateItem(aItem, {
        size: this._getDownloadSize(aDownload.size),
        displayState: this._getStateString(updatedState),
        date: DownloadUtils.getReadableDates(new Date())[0]
      });
    } catch (ex) {
      this.logError("_updateDownloadRow() " + ex, aDownload);
    }
  },

  /**
   * Logs the error to the console.
   *
   * @param aMessage  error message to log
   * @param aDownload (optional) if given, and if the download is private, the
   *                  log message is suppressed
   */
  logError: function dl_logError(aMessage, aDownload) {
    if (!aDownload || !aDownload.isPrivate) {
      console.log("Error: " + aMessage);
    }
  },

  QueryInterface: function (aIID) {
    if (!aIID.equals(Ci.nsIDownloadProgressListener) &&
        !aIID.equals(Ci.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
}
