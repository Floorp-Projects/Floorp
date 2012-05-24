/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces, Cc = Components.classes, Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DownloadUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let gStrings = Services.strings.createBundle("chrome://browser/locale/aboutDownloads.properties");

let downloadTemplate =
"<li downloadID='{id}' role='button' state='{state}'>" +
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

    Services.obs.addObserver(this, "dl-start", false);
    Services.obs.addObserver(this, "dl-failed", false);
    Services.obs.addObserver(this, "dl-scanning", false);
    Services.obs.addObserver(this, "dl-done", false);
    Services.obs.addObserver(this, "dl-blocked", false);
    Services.obs.addObserver(this, "dl-dirty", false);
    Services.obs.addObserver(this, "dl-cancel", false);

    this.getDownloads();

    let contextmenus = gChromeWin.NativeWindow.contextmenus;

    // Open shown only for downloads that completed successfully
    Downloads.openMenuItem = contextmenus.add(gStrings.GetStringFromName("downloadAction.open"),
                                              contextmenus.SelectorContext("li[state='" + this._dlmgr.DOWNLOAD_FINISHED + "']"),
      function (aTarget) {
        Downloads.openDownload(aTarget);
      }
    );
    
    // Retry shown when its failed, canceled, blocked(covered in failed, see _getState())
    Downloads.retryMenuItem = contextmenus.add(gStrings.GetStringFromName("downloadAction.retry"),
                                               contextmenus.SelectorContext("li[state='" + this._dlmgr.DOWNLOAD_FAILED + "']," +
                                                                            "li[state='" + this._dlmgr.DOWNLOAD_CANCELED + "']"),
      function (aTarget) {
        Downloads.retryDownload(aTarget);
      }
    );
    
    // Remove shown when its canceled, finished, failed(failed includes blocked and dirty, see _getState())
    Downloads.removeMenuItem = contextmenus.add(gStrings.GetStringFromName("downloadAction.remove"),
                                                contextmenus.SelectorContext("li[state='" + this._dlmgr.DOWNLOAD_CANCELED + "']," +
                                                                             "li[state='" + this._dlmgr.DOWNLOAD_FINISHED + "']," +
                                                                             "li[state='" + this._dlmgr.DOWNLOAD_FAILED + "']"),
      function (aTarget) {
        Downloads.removeDownload(aTarget);
      }
    );

    // Pause shown when item is currently downloading
    Downloads.pauseMenuItem = contextmenus.add(gStrings.GetStringFromName("downloadAction.pause"),
                                               contextmenus.SelectorContext("li[state='" + this._dlmgr.DOWNLOAD_DOWNLOADING + "']"),
      function (aTarget) {
        Downloads.pauseDownload(aTarget);
      }
    );
    
    // Resume shown for paused items only
    Downloads.resumeMenuItem = contextmenus.add(gStrings.GetStringFromName("downloadAction.resume"),
                                                contextmenus.SelectorContext("li[state='" + this._dlmgr.DOWNLOAD_PAUSED + "']"),
      function (aTarget) {
        Downloads.resumeDownload(aTarget);
      }
    );
    
    // Cancel shown when its downloading, notstarted, queued or paused
    Downloads.cancelMenuItem = contextmenus.add(gStrings.GetStringFromName("downloadAction.cancel"),
                                                contextmenus.SelectorContext("li[state='" + this._dlmgr.DOWNLOAD_DOWNLOADING + "']," +
                                                                             "li[state='" + this._dlmgr.DOWNLOAD_NOTSTARTED + "']," +
                                                                             "li[state='" + this._dlmgr.DOWNLOAD_QUEUED + "']," +
                                                                             "li[state='" + this._dlmgr.DOWNLOAD_PAUSED + "']"),
      function (aTarget) {
        Downloads.cancelDownload(aTarget);
      }
    );
  },

  uninit: function () {
    let contextmenus = gChromeWin.NativeWindow.contextmenus;
    contextmenus.remove(this.openMenuItem);
    contextmenus.remove(this.removeMenuItem);
    contextmenus.remove(this.pauseMenuItem);
    contextmenus.remove(this.resumeMenuItem);
    contextmenus.remove(this.retryMenuItem);
    contextmenus.remove(this.cancelMenuItem);

    Services.obs.removeObserver(this, "dl-start");
    Services.obs.removeObserver(this, "dl-failed");
    Services.obs.removeObserver(this, "dl-scanning");
    Services.obs.removeObserver(this, "dl-done");
    Services.obs.removeObserver(this, "dl-blocked");
    Services.obs.removeObserver(this, "dl-dirty");
    Services.obs.removeObserver(this, "dl-cancel");
  },

  observe: function (aSubject, aTopic, aData) {
    let download = aSubject.QueryInterface(Ci.nsIDownload);
    switch (aTopic) {
      case "dl-blocked":
      case "dl-dirty":
      case "dl-cancel":
      case "dl-done":
      case "dl-failed":
        // For all "completed" states, move them after active downloads
        this._moveDownloadAfterActive(this._getElementForDownload(download.id));
      
      // Fall-through the rest
      case "dl-start":
      case "dl-scanning":
        let item = this._getElementForDownload(download.id);
        if (item)
          this._updateDownloadRow(item);
        else
          this._insertDownloadRow(download);
        break;
    }
  },

  _moveDownloadAfterActive: function (aItem) {
    // Move downloads that just reached a "completed" state below any active
    try {
      // Iterate down until we find a non-active download
      let next = aItem.nextSibling;
      while (next && this._inProgress(next.getAttribute("state")))
        next = next.nextSibling;
      // Move the item
      this._list.insertBefore(aItem, next);
    } catch (ex) {
      console.log("ERROR: _moveDownloadAfterActive() : " + ex);
    }
  },
  
  _inProgress: function (aState) {
    return [
      this._dlmgr.DOWNLOAD_NOTSTARTED,
      this._dlmgr.DOWNLOAD_QUEUED,
      this._dlmgr.DOWNLOAD_DOWNLOADING,
      this._dlmgr.DOWNLOAD_PAUSED,
      this._dlmgr.DOWNLOAD_SCANNING,
    ].indexOf(parseInt(aState)) != -1;
  },

  _insertDownloadRow: function (aDownload) {
    let updatedState = this._getState(aDownload.state);
    let item = this._createItem(downloadTemplate, {
      id: aDownload.id,
      target: aDownload.displayName,
      icon: "moz-icon://" + aDownload.displayName + "?size=64",
      date: DownloadUtils.getReadableDates(new Date())[0],
      domain: DownloadUtils.getURIHost(aDownload.source.spec)[0],
      size: this._getDownloadSize(aDownload.size),
      displayState: this._getStateString(updatedState),
      state: updatedState
    });
    this._list.insertAdjacentHTML("afterbegin", item);
  },

  _getDownloadSize: function (aSize) {
    let displaySize = DownloadUtils.convertByteUnits(aSize);
    if (displaySize[0] > 0) // [0] is size, [1] is units
      return displaySize.join("");
    else
      return gStrings.GetStringFromName("downloadState.unknownSize");
  },
  
  // Not all states are displayed as-is on mobile, some are translated to a generic state
  _getState: function (aState) {
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
  _getStateString: function (aState) {
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

  _updateItem: function (aItem, aValues) {
    for (let i in aValues) {
      aItem.querySelector("." + i).textContent = aValues[i];
    }
  },

  _initStatement: function dv__initStatement() {
    if (this._stmt)
      this._stmt.finalize();

    this._stmt = this._dlmgr.DBConnection.createStatement(
      "SELECT id, name, source, state, startTime, endTime, referrer, " +
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

  _stepDownloads: function dv__stepDownloads(aNumItems) {
    try {
      if (!this._stmt.executeStep()) {
        this._stmt.finalize();
        this._stmt = null;
        return;
      }
  
      let updatedState = this._getState(this._stmt.row.state);
      // Try to get the attribute values from the statement
      let attrs = {
        id: this._stmt.row.id,
        target: this._stmt.row.name,
        icon: "moz-icon://" + this._stmt.row.name + "?size=64",
        date: DownloadUtils.getReadableDates(new Date(this._stmt.row.endTime / 1000))[0],
        domain: DownloadUtils.getURIHost(this._stmt.row.source)[0],
        size: this._getDownloadSize(this._stmt.row.maxBytes),
        displayState: this._getStateString(updatedState),
        state: updatedState
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

  _removeItem: function dv__removeItem(aItem) {
    // Make sure we have an item to remove
    if (!aItem)
      return;
  
    let index = this._list.selectedIndex;
    this._list.removeChild(aItem);
    this._list.selectedIndex = Math.min(index, this._list.itemCount - 1);
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

  pauseDownload: function (aItem) {
    try {
      let download = this._getDownloadForElement(aItem);
      this._dlmgr.pauseDownload(aItem.getAttribute("downloadID"));
      this._updateDownloadRow(aItem);
    } catch (ex) {
      console.log("Error: pauseDownload() " + ex);  
    }

  },

  resumeDownload: function (aItem) {
    try {
      let download = this._getDownloadForElement(aItem);
      this._dlmgr.resumeDownload(aItem.getAttribute("downloadID"));
      this._updateDownloadRow(aItem);
    } catch (ex) {
      console.log("Error: resumeDownload() " + ex);  
    }
  },

  retryDownload: function (aItem) {
    try {
      let download = this._getDownloadForElement(aItem);
      this._removeItem(aItem);
      this._dlmgr.retryDownload(aItem.getAttribute("downloadID"));
    } catch (ex) {
      console.log("Error: retryDownload() " + ex);  
    }
  },

  cancelDownload: function (aItem) {
    try {
      this._dlmgr.cancelDownload(aItem.getAttribute("downloadID"));
      let download = this._getDownloadForElement(aItem);
      let f = download.targetFile;

      if (f.exists())
        f.remove(false);
      
      this._updateDownloadRow(aItem);
    } catch (ex) {
      console.log("Error: cancelDownload() " + ex);  
    }
  },
  
  _updateDownloadRow: function (aItem){
    try {
      let download = this._getDownloadForElement(aItem);
      let updatedState = this._getState(download.state);
      aItem.setAttribute("state", updatedState);
      this._updateItem(aItem, {
        size: this._getDownloadSize(download.size),
        displayState: this._getStateString(updatedState),
        date: DownloadUtils.getReadableDates(new Date())[0]
      });
    } catch (ex){
       console.log("ERROR: _updateDownloadRow(): " + ex);
    }
  }
}
