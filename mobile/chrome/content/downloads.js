// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

Components.utils.import("resource://gre/modules/DownloadUtils.jsm");

const URI_GENERIC_ICON_DOWNLOAD = "chrome://mozapps/skin/downloads/downloadIcon.png";

var DownloadsView = {
  _pref: null,
  _list: null,
  _dlmgr: null,
  _progress: null,

  _initStatement: function dv__initStatement(aMode) {
    aMode = aMode || "date";

    if (this._stmt)
      this._stmt.finalize();

    let order = "endTime DESC, startTime DESC";
    if (aMode == "name")
      order = "name ASC";
    else if (aMode == "site")
      order = "REPLACE(REPLACE(REPLACE(source, \"http://\", \"\"), \"https://\", \"\"), \"ftp://\", \"\") ASC";

    this._stmt = this._dlmgr.DBConnection.createStatement(
      "SELECT id, target, name, source, state, startTime, endTime, referrer, " +
             "currBytes, maxBytes, state IN (?1, ?2, ?3, ?4, ?5) isActive " +
      "FROM moz_downloads " +
      "ORDER BY isActive DESC, " + order);
  },

  _getLocalFile: function dv__getLocalFile(aFileURI) {
    // if this is a URL, get the file from that
    let ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

    // XXX it's possible that using a null char-set here is bad
    const fileUrl = ios.newURI(aFileURI, null, null).QueryInterface(Ci.nsIFileURL);
    return fileUrl.file.clone().QueryInterface(Ci.nsILocalFile);
  },

  _getReferrerOrSource: function dv__getReferrerOrSource(aItem) {
    // Give the referrer if we have it set, otherwise, provide the source
    return aItem.getAttribute("referrer") || aItem.getAttribute("uri");
  },

  _createItem: function dv__createItem(aAttrs) {
    let item = document.createElement("richlistitem");

    // Copy the attributes from the argument into the item
    for (let attr in aAttrs)
      item.setAttribute(attr, aAttrs[attr]);

    // Initialize other attributes
    item.setAttribute("typeName", "download");
    item.setAttribute("id", "dl-" + aAttrs.id);
    item.setAttribute("downloadID", aAttrs.id);
    item.setAttribute("iconURL", "moz-icon://" + aAttrs.file + "?size=32");
    item.setAttribute("lastSeconds", Infinity);

    // Initialize more complex attributes
    this._updateTime(item);
    this._updateStatus(item);

    return item;
  },

  _removeItem: function dv__removeItem(aItem) {
    // Make sure we have an item to remove
    if (!aItem)
      return;

    let index = this._list.selectedIndex;
    this._list.removeChild(aItem);
    this._list.selectedIndex = Math.min(index, this._list.itemCount - 1);
  },

  _clearList: function dv__clearList() {
    // Clear the list by replacing with a shallow copy
    let empty = this._list.cloneNode(false);
    this._list.parentNode.replaceChild(empty, this._list);
    this._list = empty;
  },

  get visible() {
    let panel = document.getElementById("panel-container");
    let items = document.getElementById("panel-items");
    if (panel.hidden == false && items.selectedPanel.id == "downloads-container")
      return true;
    return false;
  },

  init: function dv_init() {
    if (this._dlmgr)
      return;

    this._dlmgr = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
    this._pref = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch2);

    this._progress = new DownloadProgressListener();
    this._dlmgr.addListener(this._progress);

    var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
    os.addObserver(this, "download-manager-remove-download", false);

    let self = this;
    let panels = document.getElementById("panel-items");
    panels.addEventListener("select",
                            function(aEvent) {
                              if (panels.selectedPanel.id == "downloads-container")
                                self.show();
                            },
                            false);
  },

  show: function dv_show() {
    if (this._list)
      return;

    this._list = document.getElementById("downloads-list");

    this._initStatement();
    this.getDownloads();
  },

  getDownloads: function dv_getDownloads() {
    clearTimeout(this._timeoutID);
    this._stmt.reset();

    // Array of space-separated lower-case search terms
    let search = document.getElementById("downloads-search-text");
    this._searchTerms = search.value.trim().toLowerCase().split(/\s+/);

    // Clear the list before adding items
    this._clearList();

    this._stmt.bindInt32Parameter(0, Ci.nsIDownloadManager.DOWNLOAD_NOTSTARTED);
    this._stmt.bindInt32Parameter(1, Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING);
    this._stmt.bindInt32Parameter(2, Ci.nsIDownloadManager.DOWNLOAD_PAUSED);
    this._stmt.bindInt32Parameter(3, Ci.nsIDownloadManager.DOWNLOAD_QUEUED);
    this._stmt.bindInt32Parameter(4, Ci.nsIDownloadManager.DOWNLOAD_SCANNING);

    // Take a quick break before we actually start building the list
    let self = this;
    this._timeoutID = setTimeout(function() {
      // Start building the list and select the first item
      self._stepDownloads(1);
      self._list.selectedIndex = 0;
    }, 0);
  },

  _stepDownloads: function dv__stepDownloads(aNumItems) {
    try {
      // If we're done adding all items, we can quit
      if (!this._stmt.executeStep()) {
        // Send a notification that we finished, but wait for clear list to update
        setTimeout(function() {
          let os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
          os.notifyObservers(window, "download-manager-ui-done", null);
        }, 0);
        return;
      }

      // Try to get the attribute values from the statement
      let attrs = {
        id: this._stmt.getInt64(0),
        file: this._stmt.getString(1),
        target: this._stmt.getString(2),
        uri: this._stmt.getString(3),
        state: this._stmt.getInt32(4),
        startTime: Math.round(this._stmt.getInt64(5) / 1000),
        endTime: Math.round(this._stmt.getInt64(6) / 1000),
        currBytes: this._stmt.getInt64(8),
        maxBytes: this._stmt.getInt64(9)
      };

      // Only add the referrer if it's not null
      let (referrer = this._stmt.getString(7)) {
        if (referrer)
          attrs.referrer = referrer;
      }

      // If the download is active, grab the real progress, otherwise default 100
      let isActive = this._stmt.getInt32(10);
      attrs.progress = isActive ? this._dlmgr.getDownload(attrs.id).percentComplete : 100;

      // Make the item and add it to the end if it's active or matches the search
      let item = this._createItem(attrs);
      if (item && (isActive || this._matchesSearch(item))) {
        // Add item to the end
        this._list.appendChild(item);
      }
      else {
        // We didn't add an item, so bump up the number of items to process, but
        // not a whole number so that we eventually do pause for a chunk break
        aNumItems += .9;
      }
    }
    catch (e) {
      // Something went wrong when stepping or getting values, so clear and quit
      this._stmt.reset();
      return;
    }

    // Add another item to the list if we should; otherwise, let the UI update
    // and continue later
    if (aNumItems > 1) {
      this._stepDownloads(aNumItems - 1);
    }
    else {
      // Use a shorter delay for earlier downloads to display them faster
      let delay = Math.min(this._list.itemCount * 10, 300);
      let self = this;
      this._timeoutID = setTimeout(function() { self._stepDownloads(5); }, delay);
    }
  },

  _matchesSearch: function dv__matchesSearch(aItem) {
    const searchAttributes = ["target", "status", "datetime"];

    // Return early if we have no search terms
    if (this._searchTerms.length == 0)
      return true;

    // Search through the download attributes that are shown to the user and
    // make it into one big string for easy combined searching
    let combinedSearch = "";
    for each (let attr in searchAttributes)
      combinedSearch += aItem.getAttribute(attr).toLowerCase() + " ";

    // Make sure each of the terms are found
    for each (let term in this._searchTerms)
      if (combinedSearch.search(term) == -1)
        return false;

    return true;
  },

  downloadStarted: function dv_downloadStarted(aDownload) {
    let attrs = {
      id: aDownload.id,
      file: aDownload.target.spec,
      target: aDownload.displayName,
      uri: aDownload.source.spec,
      state: aDownload.state,
      progress: aDownload.percentComplete,
      startTime: Math.round(aDownload.startTime / 1000),
      endTime: Date.now(),
      currBytes: aDownload.amountTransferred,
      maxBytes: aDownload.size
    };

    // Make the item and add it to the beginning
    let item = this._createItem(attrs);
    if (item) {
      // Add item to the beginning
      this._list.insertBefore(item, this._list.firstChild);
    }

    if (this.visible)
      return;

    let strings = document.getElementById("bundle_browser");
    var notifier = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
    notifier.showAlertNotification(URI_GENERIC_ICON_DOWNLOAD, strings.getString("alertDownloads"),
                                   strings.getFormattedString("alertDownloadsStart", [attrs.target]), true, "", this);
  },

  downloadCompleted: function dv_downloadCompleted(aDownload) {
    let element = this.getElementForDownload(aDownload.id);

    // Move the download below active if it should stay in the list
    if (this._matchesSearch(element)) {
      // Iterate down until we find a non-active download
      let next = element.nextSibling;
      while (next && next.inProgress)
        next = next.nextSibling;

      // Move the item
      this._list.insertBefore(element, next);
    }
    else {
      this._removeItem(element);
    }

    if (this.visible)
      return;

    let target = element.getAttribute("target");
    let strings = document.getElementById("bundle_browser");
    var notifier = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
    notifier.showAlertNotification(URI_GENERIC_ICON_DOWNLOAD, strings.getString("alertDownloads"),
                                   strings.getFormattedString("alertDownloadsDone", [target]), true, "", this);
  },

  _updateStatus: function dv__updateStatus(aItem) {
    let strings = document.getElementById("bundle_browser");

    let status = "";
    let state = Number(aItem.getAttribute("state"));

    // Display the file size, but show "Unknown" for negative sizes
    let fileSize = Number(aItem.getAttribute("maxBytes"));
    let sizeText = strings.getString("downloadsUnknownSize");
    if (fileSize >= 0) {
      let [size, unit] = DownloadUtils.convertByteUnits(fileSize);
      sizeText = this._replaceInsert(strings.getString("downloadsKnownSize"), 1, size);
      sizeText = this._replaceInsert(sizeText, 2, unit);
    }

    // Insert 1 is the download size or download state
    status = this._replaceInsert(strings.getString("downloadsStatus"), 1, sizeText);

    // Insert 2 is the eTLD + 1 or other variations of the host
    let [displayHost, fullHost] = DownloadUtils.getURIHost(this._getReferrerOrSource(aItem));
    status = this._replaceInsert(status, 2, displayHost);

    aItem.setAttribute("status", status);
  },

  _updateTime: function dv__updateTime(aItem) {
    // Don't bother updating for things that aren't finished
    if (aItem.inProgress)
      return;

    let dts = Cc["@mozilla.org/intl/scriptabledateformat;1"].getService(Ci.nsIScriptableDateFormat);

    // Figure out when today begins
    let now = new Date();
    let today = new Date(now.getFullYear(), now.getMonth(), now.getDate());

    // Get the end time to display
    let end = new Date(parseInt(aItem.getAttribute("endTime")));

    // Figure out if the end time is from today, yesterday, this week, etc.
    let dateTime;
    if (end >= today) {
      // Download finished after today started, show the time
      dateTime = dts.FormatTime("", dts.timeFormatNoSeconds, end.getHours(), end.getMinutes(), 0);
    }
    else if (today - end < (24 * 60 * 60 * 1000)) {
      // Download finished after yesterday started, show yesterday
      dateTime = "Yesterday";//gStr.yesterday;
    }
    else if (today - end < (6 * 24 * 60 * 60 * 1000)) {
      // Download finished after last week started, show day of week
      dateTime = end.toLocaleFormat("%A");
    }
    else {
      // Download must have been from some time ago.. show month/day
      let month = end.toLocaleFormat("%B");
      // Remove leading 0 by converting the date string to a number
      let date = Number(end.toLocaleFormat("%d"));
      //dateTime = this._replaceInsert(gStr.monthDate, 1, month);
      dateTime = this._replaceInsert("#1 #2", 1, month);
      dateTime = this._replaceInsert(dateTime, 2, date);
    }

    aItem.setAttribute("datetime", dateTime);
  },

  _replaceInsert: function dv__replaceInsert(aText, aIndex, aValue) {
    return aText.replace("#" + aIndex, aValue);
  },

  toggleMode: function dv_toggleMode() {
    let mode = document.getElementById("downloads-sort-mode");
    if (mode.value == "search") {
      document.getElementById("downloads-search-box").collapsed = false;
      document.getElementById("downloads-search-text").value = "";
    }
    else {
      document.getElementById("downloads-search-box").collapsed = true;
      this._initStatement(mode.value);
      this.getDownloads();
    }
  },

  getElementForDownload: function dv_getElementFromDownload(aID) {
    return document.getElementById("dl-" + aID);
  },

  openDownload: function dv_openDownload(aItem) {
    let f = this._getLocalFile(aItem.getAttribute("file"));
    try {
      f.launch();
    } catch (ex) { }

    // TODO: add back the code for "dontAsk"?
  },

  showDownload: function dv_showDownload(aItem) {
    let f = this._getLocalFile(aItem.getAttribute("file"));
    try {
      f.reveal();
    } catch (ex) { }

    // TODO: add back the extra code?
  },

  removeDownload: function dv_removeDownload(aItem) {
    this._dlmgr.removeDownload(aItem.getAttribute("downloadID"));
  },

  cancelDownload: function dv_cancelDownload(aItem) {
    this._dlmgr.cancelDownload(aItem.getAttribute("downloadID"));
    var f = this._getLocalFile(aItem.getAttribute("file"));
    if (f.exists())
      f.remove(false);
  },

  pauseDownload: function dv_pauseDownload(aItem) {
    this._dlmgr.pauseDownload(aItem.getAttribute("downloadID"));
  },

  resumeDownload: function dv_resumeDownload(aItem) {
    this._dlmgr.resumeDownload(aItem.getAttribute("downloadID"));
  },

  retryDownload: function dv_retryDownload(aItem) {
    this._removeItem(aItem);
    this._dlmgr.retryDownload(aItem.getAttribute("downloadID"));
  },

  showPage: function dv_showPage(aItem) {
    BrowserUI.goToURI(this._getReferrerOrSource(aItem));
  },

  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "download-manager-remove-download":
        // A null subject here indicates "remove multiple", so we just rebuild.
        if (!aSubject) {
          // Rebuild the default view
          this.getDownloads();
          break;
        }

        // Otherwise, remove a single download
        let id = aSubject.QueryInterface(Ci.nsISupportsPRUint32);
        let element = this.getElementForDownload(id.data);
        this._removeItem(element);
        break;
    }
  },

  QueryInterface: function (aIID) {
    if (!aIID.equals(Ci.nsIObserver) &&
        !aIID.equals(Ci.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

function DownloadProgressListener() { }

DownloadProgressListener.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsIDownloadProgressListener
  onDownloadStateChange: function dlPL_onDownloadStateChange(aState, aDownload) {
    let state = aDownload.state;
    switch (state) {
      case Ci.nsIDownloadManager.DOWNLOAD_QUEUED:
        DownloadsView.downloadStarted(aDownload);
        break;

      case Ci.nsIDownloadManager.DOWNLOAD_BLOCKED_POLICY:
        DownloadsView.downloadStarted(aDownload);
        // Should fall through, this is a final state but DOWNLOAD_QUEUED
        // is skipped. See nsDownloadManager::AddDownload.
      case Ci.nsIDownloadManager.DOWNLOAD_FAILED:
      case Ci.nsIDownloadManager.DOWNLOAD_CANCELED:
      case Ci.nsIDownloadManager.DOWNLOAD_BLOCKED_PARENTAL:
      case Ci.nsIDownloadManager.DOWNLOAD_DIRTY:
      case Ci.nsIDownloadManager.DOWNLOAD_FINISHED:
        DownloadsView.downloadCompleted(aDownload);
        break;
    }

    let element = DownloadsView.getElementForDownload(aDownload.id);

    // We should eventually know the referrer at some point
    let referrer = aDownload.referrer;
    if (referrer && element.getAttribute("referrer") != referrer.spec)
      element.setAttribute("referrer", referrer.spec);

    // Update to the new state
    element.setAttribute("state", state);
    element.setAttribute("currBytes", aDownload.amountTransferred);
    element.setAttribute("maxBytes", aDownload.size);
    element.setAttribute("endTime", Date.now());

    // Update ui text values after switching states
    DownloadsView._updateTime(element);
    DownloadsView._updateStatus(element);
  },

  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress, aDownload) {
    let element = DownloadsView.getElementForDownload(aDownload.id);
    if (!element)
      return;

    // Update this download's progressmeter
    if (aDownload.percentComplete == -1) {
      element.setAttribute("progressmode", "undetermined");
    }
    else {
      element.setAttribute("progressmode", "normal");
      element.setAttribute("progress", aDownload.percentComplete);
    }

    // Dispatch ValueChange for a11y
    let event = document.createEvent("Events");
    event.initEvent("ValueChange", true, true);
    let progmeter = document.getAnonymousElementByAttribute(element, "anonid", "progressmeter");
    if (progmeter)
      progmeter.dispatchEvent(event);

    // Update the progress so the status can be correctly updated
    element.setAttribute("currBytes", aDownload.amountTransferred);
    element.setAttribute("maxBytes", aDownload.size);

    // Update the rest of the UI
    DownloadsView._updateStatus(element);
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
