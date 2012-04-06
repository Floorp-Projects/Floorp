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

#ifdef ANDROID
const URI_GENERIC_ICON_DOWNLOAD = "drawable://alertdownloads";
#else
const URI_GENERIC_ICON_DOWNLOAD = "chrome://browser/skin/images/alert-downloads-30.png";
#endif

var DownloadsView = {
  _initialized: false,
  _list: null,
  _dlmgr: null,
  _progress: null,
  _progressAlert: null,

  _initStatement: function dv__initStatement() {
    if (this._stmt)
      this._stmt.finalize();

    this._stmt = this._dlmgr.DBConnection.createStatement(
      "SELECT id, target, name, source, state, startTime, endTime, referrer, " +
             "currBytes, maxBytes, state IN (?1, ?2, ?3, ?4, ?5) isActive " +
      "FROM moz_downloads " +
      "ORDER BY isActive DESC, endTime DESC, startTime DESC");
  },

  _getLocalFile: function dv__getLocalFile(aFileURI) {
    // if this is a URL, get the file from that
    // XXX it's possible that using a null char-set here is bad
    const fileUrl = Services.io.newURI(aFileURI, null, null).QueryInterface(Ci.nsIFileURL);
    return fileUrl.file.clone().QueryInterface(Ci.nsILocalFile);
  },

  _getReferrerOrSource: function dv__getReferrerOrSource(aItem) {
    // Give the referrer if we have it set, otherwise, provide the source
    return aItem.getAttribute("referrer") || aItem.getAttribute("uri");
  },

  _createItem: function dv__createItem(aAttrs) {
    // Make sure this doesn't already exist
    let item = this.getElementForDownload(aAttrs.id);
    if (!item) {
      item = document.createElement("richlistitem");

      // Copy the attributes from the argument into the item
      for (let attr in aAttrs)
        item.setAttribute(attr, aAttrs[attr]);
  
      // Initialize other attributes
      item.setAttribute("typeName", "download");
      item.setAttribute("id", "dl-" + aAttrs.id);
      item.setAttribute("downloadID", aAttrs.id);
      item.setAttribute("iconURL", "moz-icon://" + aAttrs.file + "?size=32");
      item.setAttribute("lastSeconds", Infinity);
      item.setAttribute("class", "panel-listitem");
  
      // Initialize more complex attributes
      this._updateTime(item);
      this._updateStatus(item);
    }
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
    // Clear the list except for the header row
    let header = document.getElementById("downloads-list-header");
    while (header.nextSibling)
      this._list.removeChild(header.nextSibling);
  },
  
  _ifEmptyShowMessage: function dv__ifEmptyShowMessage() {
    // The header row is not counted in the list itemCount
    if (this._list.itemCount == 0) {
      let emptyString = Strings.browser.GetStringFromName("downloadsEmpty");
      let emptyItem = this._list.appendItem(emptyString);
      emptyItem.id = "dl-empty-message";
    }
  },

  get visible() {
    let items = document.getElementById("panel-items");
    if (BrowserUI.isPanelVisible() && items.selectedPanel.id == "downloads-container")
      return true;
    return false;
  },

  init: function dv_init() {
    if (this._initialized)
      return;
    this._initialized = true;

    // Monitor downloads and display alerts
    var os = Services.obs;
    os.addObserver(this, "dl-start", true);
    os.addObserver(this, "dl-failed", true);
    os.addObserver(this, "dl-done", true);
    os.addObserver(this, "dl-blocked", true);
    os.addObserver(this, "dl-dirty", true);
    os.addObserver(this, "dl-cancel", true);

    // Monitor downloads being removed by the download manager (non-UI)
    os.addObserver(this, "download-manager-remove-download", true);
  },

  delayedInit: function dv__delayedInit() {
    if (this._list)
      return;

    this.init(); // In case the panel is selected before init has been called.

    this._list = document.getElementById("downloads-list");

    if (this._dlmgr == null)
      this._dlmgr = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);

    this._progress = new DownloadProgressListener();
    this._dlmgr.addListener(this._progress);

    this._initStatement();
    this.getDownloads();
  },

  getDownloads: function dv_getDownloads() {
    clearTimeout(this._timeoutID);
    this._stmt.reset();

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
        // Show empty message if needed
        this._ifEmptyShowMessage();
        
        // Send a notification that we finished, but wait for clear list to update
        setTimeout(function() {
          Services.obs.notifyObservers(window, "download-manager-ui-done", null);
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
      };

      // If the download is active, grab the real progress, otherwise default 100
      let isActive = this._stmt.getInt32(10);
      attrs.progress = isActive ? this._dlmgr.getDownload(attrs.id).percentComplete : 100;

      // Make the item and add it to the end
      let item = this._createItem(attrs);
      this._list.appendChild(item);
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

    // Remove the "no downloads" item, if visible
    let emptyItem = document.getElementById("dl-empty-message");
    if (emptyItem)
      this._list.removeChild(emptyItem);
      
    // Make the item and add it to the beginning (but before the header)
    let header = document.getElementById("downloads-list-header");
    let item = this._createItem(attrs);
    this._list.insertBefore(item, header.nextSibling);
  },

  downloadCompleted: function dv_downloadCompleted(aDownload) {
    if (DownloadsView.visible)
      return;

    // Move the download below active
    let element = this.getElementForDownload(aDownload.id);

    // Iterate down until we find a non-active download
    let next = element.nextSibling;
    while (next && next.inProgress)
      next = next.nextSibling;

    // Move the item
    this._list.insertBefore(element, next);
  },

  _updateStatus: function dv__updateStatus(aItem) {
    let strings = Strings.browser;

    let status = "";

    // Display the file size, but show "Unknown" for negative sizes
    let fileSize = Number(aItem.getAttribute("maxBytes"));
    let sizeText = strings.GetStringFromName("downloadsUnknownSize");
    if (fileSize >= 0) {
      let [size, unit] = this._DownloadUtils.convertByteUnits(fileSize);
      sizeText = this._replaceInsert(strings.GetStringFromName("downloadsKnownSize"), 1, size);
      sizeText = this._replaceInsert(sizeText, 2, unit);
    }

    // Insert 1 is the download size
    status = this._replaceInsert(strings.GetStringFromName("downloadsStatus"), 1, sizeText);

    // Insert 2 is the eTLD + 1 or other variations of the host
    let [displayHost, fullHost] = this._DownloadUtils.getURIHost(this._getReferrerOrSource(aItem));
    status = this._replaceInsert(status, 2, displayHost);
  
    // Insert 3 is the left time if download status is DOWNLOADING
    let currstate = Number(aItem.getAttribute("state"));
    if (currstate == Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING) {
      let downloadSize = Number(aItem.getAttribute("currBytes"));
      let passedTime = (Date.now() - aItem.getAttribute("startTime"))/1000;
      let totalTime = (passedTime / downloadSize) * fileSize;
      let leftTime = totalTime - passedTime;
      let [time, lastTime] = this._DownloadUtils.getTimeLeft(leftTime);

      let stringTime = this._replaceInsert(strings.GetStringFromName("downloadsTime"), 1, time);
      status = status + " " + stringTime;
    } 
      
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

    let strings = Strings.browser;

    // Figure out if the end time is from today, yesterday, this week, etc.
    let dateTime;
    if (end >= today) {
      // Download finished after today started, show the time
      dateTime = dts.FormatTime("", dts.timeFormatNoSeconds, end.getHours(), end.getMinutes(), 0);
    }
    else if (today - end < (24 * 60 * 60 * 1000)) {
      // Download finished after yesterday started, show yesterday
      dateTime = strings.GetStringFromName("downloadsYesterday");
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
      dateTime = this._replaceInsert(strings.GetStringFromName("downloadsMonthDate"), 1, month);
      dateTime = this._replaceInsert(dateTime, 2, date);
    }

    aItem.setAttribute("datetime", dateTime);
  },

  _replaceInsert: function dv__replaceInsert(aText, aIndex, aValue) {
    return aText.replace("#" + aIndex, aValue);
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

  removeDownload: function dv_removeDownload(aItem) {
    let f = this._getLocalFile(aItem.getAttribute("file"));
    let res = Services.prompt.confirm(null, Strings.browser.GetStringFromName("downloadsDeleteTitle"), f.leafName);
    if(res) {
      this._dlmgr.removeDownload(aItem.getAttribute("downloadID"));
      if (f.exists())
          f.remove(false);
    }
  },

  cancelDownload: function dv_cancelDownload(aItem) {
    this._dlmgr.cancelDownload(aItem.getAttribute("downloadID"));
    let f = this._getLocalFile(aItem.getAttribute("file"));
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
    let uri = this._getReferrerOrSource(aItem);
    if (uri)
      BrowserUI.newTab(uri, Browser.selectedTab);
  },

  showAlert: function dv_showAlert(aName, aMessage, aTitle, aIcon) {
    if (this.visible)
      return;

    var notifier = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);

    // Callback for tapping on the alert popup
    let observer = {
      observe: function (aSubject, aTopic, aData) {
        if (aTopic == "alertclickcallback")
          BrowserUI.showPanel("downloads-container");
      }
    };

    if (!aTitle)
      aTitle = Strings.browser.GetStringFromName("alertDownloads");
    if (!aIcon)
      aIcon = URI_GENERIC_ICON_DOWNLOAD;

    notifier.showAlertNotification(aIcon, aTitle, aMessage, true, "", observer, aName);
  },

  observe: function (aSubject, aTopic, aData) {
    if (aTopic == "download-manager-remove-download") {
      // A null subject here indicates "remove multiple", so we just rebuild.
      if (!aSubject) {
        // Rebuild the default view
        this.getDownloads();
        return;
      }

      // Otherwise, remove a single download
      let id = aSubject.QueryInterface(Ci.nsISupportsPRUint32);
      let element = this.getElementForDownload(id.data);
      this._removeItem(element);

      // Show empty message if needed
      this._ifEmptyShowMessage();
    }
    else {
      let download = aSubject.QueryInterface(Ci.nsIDownload);
      let msgKey = "";

      if (aTopic == "dl-start") {
        msgKey = "alertDownloadsStart";
        if (!this._progressAlert) {
          if (!this._dlmgr)
            this._dlmgr = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
          this._progressAlert = new AlertDownloadProgressListener();
          this._dlmgr.addListener(this._progressAlert);
        }
      } else if (aTopic == "dl-done") {
        msgKey = "alertDownloadsDone";
      }

      if (msgKey)
        this.showAlert(download.target.spec.replace("file:", "download:"),
                       Strings.browser.formatStringFromName(msgKey, [download.displayName], 1));
    }
  },

  QueryInterface: function (aIID) {
    if (!aIID.equals(Ci.nsIObserver) &&
        !aIID.equals(Ci.nsISupportsWeakReference) &&
        !aIID.equals(Ci.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

XPCOMUtils.defineLazyModuleGetter(DownloadsView, "_DownloadUtils",
                                  "resource://gre/modules/DownloadUtils.jsm", "DownloadUtils");

// DownloadProgressListener is used for managing the DownloadsView UI. This listener
// is only active if the view has been completely initialized.
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
      DownloadsView.showAlert(aDownload.target.spec.replace("file:", "download:"),
                              strings.GetStringFromName("alertDownloadsNoSpace"),
                              strings.GetStringFromName("alertDownloadsSize"));

      Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager).cancelDownload(aDownload.id);
    }

#ifdef ANDROID
    if (aDownload.percentComplete == -1) {
      // Undetermined progress is not supported yet
      return;
    }
    let alertsService = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
    let progressListener = alertsService.QueryInterface(Ci.nsIAlertsProgressListener);
    let notificationName = aDownload.target.spec.replace("file:", "download:");
    progressListener.onProgress(notificationName, aDownload.percentComplete, 100);
#endif
  },

  onDownloadStateChange: function(aState, aDownload) {
#ifdef ANDROID
    let state = aDownload.state;
    switch (state) {
      case Ci.nsIDownloadManager.DOWNLOAD_FAILED:
      case Ci.nsIDownloadManager.DOWNLOAD_CANCELED:
      case Ci.nsIDownloadManager.DOWNLOAD_BLOCKED_PARENTAL:
      case Ci.nsIDownloadManager.DOWNLOAD_DIRTY:
      case Ci.nsIDownloadManager.DOWNLOAD_FINISHED: {
        let alertsService = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
        let progressListener = alertsService.QueryInterface(Ci.nsIAlertsProgressListener);
        let notificationName = aDownload.target.spec.replace("file:", "download:");
        progressListener.onCancel(notificationName);
        break;
      }
    }
#endif
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
