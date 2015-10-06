/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { interfaces: Ci, utils: Cu, classes: Cc } = Components;

const nsIDM = Ci.nsIDownloadManager;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadUtils",
  "resource://gre/modules/DownloadUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
  "resource://gre/modules/PluralForm.jsm");

var gStr = {};

var DownloadItem = function(aID, aDownload) {
  this.id = aID;
  this._download = aDownload;

  this.file = aDownload.target.path;
  this.target = OS.Path.basename(aDownload.target.path);
  this.uri = aDownload.source.url;
  this.endTime = new Date();

  this.updateState();

  this.updateData();

  this.createItem();
};

DownloadItem.prototype = {
  /**
   * The XUL element corresponding to the associated richlistbox item.
   */
  element: null,

  /**
   * The inner XUL element for the progress bar, or null if not available.
   */
  _progressElement: null,

  _lastEstimatedSecondsLeft: Infinity,

  updateState: function() {
    // Collapse state using the correct priority.
    if (this._download.succeeded) {
      this.state = nsIDM.DOWNLOAD_FINISHED;
    } else if (this._download.error &&
               this._download.error.becauseBlockedByParentalControls) {
      this.state = nsIDM.DOWNLOAD_BLOCKED_PARENTAL;
    } else if (this._download.error &&
               this._download.error.becauseBlockedByReputationCheck) {
      this.state = nsIDM.DOWNLOAD_DIRTY;
    } else if (this._download.error) {
      this.state = nsIDM.DOWNLOAD_FAILED;
    } else if (this._download.canceled && this._download.hasPartialData) {
      this.state = nsIDM.DOWNLOAD_PAUSED;
    } else if (this._download.canceled) {
      this.state = nsIDM.DOWNLOAD_CANCELED;
    } else if (this._download.stopped) {
      this.state = nsIDM.DOWNLOAD_NOTSTARTED;
    } else {
      this.state = nsIDM.DOWNLOAD_DOWNLOADING;
    }
  },

  updateData: function() {
    let wasInProgress = this.inProgress;

    this.updateState();

    if (wasInProgress && !this.inProgress) {
      this.endTime = new Date();
    }

    this.referrer = this._download.source.referrer;
    this.startTime = this._download.startTime;
    this.currBytes = this._download.currentBytes;
    this.resumable = this._download.hasPartialData;
    this.speed = this._download.speed;

    if (this._download.succeeded) {
      // If the download succeeded, show the final size if available, otherwise
      // use the last known number of bytes transferred.  The final size on disk
      // will be available when bug 941063 is resolved.
      this.maxBytes = this._download.hasProgress ? this._download.totalBytes
                                                 : this._download.currentBytes;
      this.percentComplete = 100;
    } else if (this._download.hasProgress) {
      // If the final size and progress are known, use them.
      this.maxBytes = this._download.totalBytes;
      this.percentComplete = this._download.progress;
    } else {
      // The download final size and progress percentage is unknown.
      this.maxBytes = -1;
      this.percentComplete = -1;
    }
  },

  /**
   * Indicates whether the download is proceeding normally, and not finished
   * yet. This includes paused downloads.  When this property is true, the
   * "progress" property represents the current progress of the download.
   */
  get inProgress() {
    return [
      nsIDM.DOWNLOAD_NOTSTARTED,
      nsIDM.DOWNLOAD_QUEUED,
      nsIDM.DOWNLOAD_DOWNLOADING,
      nsIDM.DOWNLOAD_PAUSED,
      nsIDM.DOWNLOAD_SCANNING,
    ].indexOf(this.state) != -1;
  },

  /**
   * This is true during the initial phases of a download, before the actual
   * download of data bytes starts.
   */
  get starting() {
    return this.state == nsIDM.DOWNLOAD_NOTSTARTED ||
           this.state == nsIDM.DOWNLOAD_QUEUED;
  },

  /**
   * Indicates whether the download is paused.
   */
  get paused() {
    return this.state == nsIDM.DOWNLOAD_PAUSED;
  },

  /**
   * Indicates whether the download is in a final state, either because it
   * completed successfully or because it was blocked.
   */
  get done() {
    return [
      nsIDM.DOWNLOAD_FINISHED,
      nsIDM.DOWNLOAD_BLOCKED_PARENTAL,
      nsIDM.DOWNLOAD_BLOCKED_POLICY,
      nsIDM.DOWNLOAD_DIRTY,
    ].indexOf(this.state) != -1;
  },

  /**
   * Indicates whether the download can be removed.
   */
  get removable() {
    return [
      nsIDM.DOWNLOAD_FINISHED,
      nsIDM.DOWNLOAD_CANCELED,
      nsIDM.DOWNLOAD_BLOCKED_PARENTAL,
      nsIDM.DOWNLOAD_BLOCKED_POLICY,
      nsIDM.DOWNLOAD_DIRTY,
      nsIDM.DOWNLOAD_FAILED,
    ].indexOf(this.state) != -1;
  },

  /**
   * Indicates whether the download is finished and can be opened.
   */
  get openable() {
    return this.state == nsIDM.DOWNLOAD_FINISHED;
  },

  /**
   * Indicates whether the download stopped because of an error, and can be
   * resumed manually.
   */
  get canRetry() {
    return this.state == nsIDM.DOWNLOAD_CANCELED ||
           this.state == nsIDM.DOWNLOAD_FAILED;
  },

  /**
   * Returns the nsILocalFile for the download target.
   *
   * @throws if the native path is not valid.  This can happen if the same
   *         profile is used on different platforms, for example if a native
   *         Windows path is stored and then the item is accessed on a Mac.
   */
  get localFile() {
    return this._getFile(this.file);
  },

  /**
   * Returns the nsILocalFile for the partially downloaded target.
   *
   * @throws if the native path is not valid.  This can happen if the same
   *         profile is used on different platforms, for example if a native
   *         Windows path is stored and then the item is accessed on a Mac.
   */
  get partFile() {
    return this._getFile(this.file + ".part");
  },

  /**
   * Return a nsILocalFile for aFilename. aFilename might be a file URL or
   * a native path.
   *
   * @param aFilename the filename of the file to retrieve.
   * @return an nsILocalFile for the file.
   * @throws if the native path is not valid.  This can happen if the same
   *         profile is used on different platforms, for example if a native
   *         Windows path is stored and then the item is accessed on a Mac.
   * @note This function makes no guarantees about the file's existence -
   *       callers should check that the returned file exists.
   */
  _getFile: function(aFilename) {
    // The download database may contain targets stored as file URLs or native
    // paths.  This can still be true for previously stored items, even if new
    // items are stored using their file URL.  See also bug 239948 comment 12.
    if (aFilename.startsWith("file:")) {
      // Assume the file URL we obtained from the downloads database or from the
      // "spec" property of the target has the UTF-8 charset.
      let fileUrl = NetUtil.newURI(aFilename).QueryInterface(Ci.nsIFileURL);
      return fileUrl.file;
    }

    // The downloads database contains a native path.  Try to create a local
    // file, though this may throw an exception if the path is invalid.
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(aFilename);
    return file;
  },

  /**
   * Show a downloaded file in the system file manager.
   */
  showLocalFile: function() {
    let file = this.localFile;

    try {
      // Show the directory containing the file and select the file.
      file.reveal();
    } catch (ex) {
      // If reveal fails for some reason (e.g., it's not implemented on unix
      // or the file doesn't exist), try using the parent if we have it.
      let parent = file.parent;
      if (!parent) {
        return;
      }

      try {
        // Open the parent directory to show where the file should be.
        parent.launch();
      } catch (ex) {
        // If launch also fails (probably because it's not implemented), let
        // the OS handler try to open the parent.
        Cc["@mozilla.org/uriloader/external-protocol-service;1"].
        getService(Ci.nsIExternalProtocolService).
        loadUrl(NetUtil.newURI(parent));
      }
    }
  },

  /**
   * Open the target file for this download.
   */
  openLocalFile: function() {
    return this._download.launch();
  },

  /**
   * Pause the download, if it is active.
   */
  pause: function() {
    return this._download.cancel();
  },

  /**
   * Resume the download, if it is paused.
   */
  resume: function() {
    return this._download.start();
  },

  /**
   * Attempt to retry the download.
   */
  retry: function() {
    return this._download.start();
  },

  /**
   * Cancel the download.
   */
  cancel: function() {
    this._download.cancel();
    return this._download.removePartialData();
  },

  /**
   * Remove the download.
   */
  remove: function() {
    return Downloads.getList(Downloads.ALL)
                    .then(list => list.remove(this._download))
                    .then(() => this._download.finalize(true));
  },

  /**
   * Create a download richlistitem with the provided attributes.
   */
  createItem: function() {
    this.element = document.createElement("richlistitem");

    this.element.setAttribute("id", this.id);
    this.element.setAttribute("target", this.target);
    this.element.setAttribute("state", this.state);
    this.element.setAttribute("progress", this.percentComplete);
    this.element.setAttribute("type", "download");
    this.element.setAttribute("image", "moz-icon://" + this.file + "?size=32");
  },

  /**
   * Update the status for a download item depending on its state.
   */
  updateStatusText: function() {
    let status = "";
    let statusTip = "";

    switch (this.state) {
      case nsIDM.DOWNLOAD_PAUSED:
        let transfer = DownloadUtils.getTransferTotal(this.currBytes,
                                                      this.maxBytes);
        status = gStr.paused.replace("#1", transfer);

        break;

      case nsIDM.DOWNLOAD_DOWNLOADING:
        let newLast;
        [status, newLast] =
          DownloadUtils.getDownloadStatus(this.currBytes, this.maxBytes,
                                          this.speed,
                                          this._lastEstimatedSecondsLeft);

        this._lastEstimatedSecondsLeft = newLast;

        break;

      case nsIDM.DOWNLOAD_FINISHED:
      case nsIDM.DOWNLOAD_FAILED:
      case nsIDM.DOWNLOAD_CANCELED:
      case nsIDM.DOWNLOAD_BLOCKED_PARENTAL:
      case nsIDM.DOWNLOAD_BLOCKED_POLICY:
      case nsIDM.DOWNLOAD_DIRTY:
        let stateSize = {};
        stateSize[nsIDM.DOWNLOAD_FINISHED] = () => {
          // Display the file size, but show "Unknown" for negative sizes.
          let sizeText = gStr.doneSizeUnknown;
          if (this.maxBytes >= 0) {
            let [size, unit] = DownloadUtils.convertByteUnits(this.maxBytes);
            sizeText = gStr.doneSize.replace("#1", size);
            sizeText = sizeText.replace("#2", unit);
          }
          return sizeText;
        };
        stateSize[nsIDM.DOWNLOAD_FAILED] = () => gStr.stateFailed;
        stateSize[nsIDM.DOWNLOAD_CANCELED] = () => gStr.stateCanceled;
        stateSize[nsIDM.DOWNLOAD_BLOCKED_PARENTAL] = () => gStr.stateBlocked;
        stateSize[nsIDM.DOWNLOAD_BLOCKED_POLICY] = () => gStr.stateBlockedPolicy;
        stateSize[nsIDM.DOWNLOAD_DIRTY] = () => gStr.stateDirty;

        // Insert 1 is the download size or download state.
        status = gStr.doneStatus.replace("#1", stateSize[this.state]());

        let [displayHost, fullHost] =
          DownloadUtils.getURIHost(this.referrer || this.uri);

        // Insert 2 is the eTLD + 1 or other variations of the host.
        status = status.replace("#2", displayHost);
        // Set the tooltip to be the full host.
        statusTip = fullHost;

        break;
    }

    this.element.setAttribute("status", status);
    this.element.setAttribute("statusTip", statusTip || status);
  },

  updateView: function() {
    this.updateData();

    // Update this download's progressmeter.
    if (this.starting) {
      // Before the download starts, the progress meter has its initial value.
      this.element.setAttribute("progressmode", "normal");
      this.element.setAttribute("progress", "0");
    } else if (this.state == Ci.nsIDownloadManager.DOWNLOAD_SCANNING ||
               this.percentComplete == -1) {
      // We might not know the progress of a running download, and we don't know
      // the remaining time during the malware scanning phase.
      this.element.setAttribute("progressmode", "undetermined");
    } else {
      // This is a running download of which we know the progress.
      this.element.setAttribute("progressmode", "normal");
      this.element.setAttribute("progress", this.percentComplete);
    }

    // Find the progress element as soon as the download binding is accessible.
    if (!this._progressElement) {
      this._progressElement =
           document.getAnonymousElementByAttribute(this.element, "anonid",
                                                   "progressmeter");
    }

    // Dispatch the ValueChange event for accessibility, if possible.
    if (this._progressElement) {
      let event = document.createEvent("Events");
      event.initEvent("ValueChange", true, true);
      this._progressElement.dispatchEvent(event);
    }

    // Update the rest of the UI (bytes transferred, bytes total, download rate,
    // time remaining).

    // Update the time that gets shown for completed download items
    // Don't bother updating it for things that aren't finished
    if (!this.inProgress) {
      let [dateCompact, dateComplete] = DownloadUtils.getReadableDates(this.endTime);
      this.element.setAttribute("dateTime", dateCompact);
      this.element.setAttribute("dateTimeTip", dateComplete);
    }

    this.element.setAttribute("state", this.state);

    this.updateStatusText();

    // Update the disabled state of the buttons of a download
    let buttons = this.element.buttons;
    for (let i = 0; i < buttons.length; ++i) {
      let cmd = buttons[i].getAttribute("cmd");
      let enabled = this.isCommandEnabled(cmd);
      buttons[i].disabled = !enabled;

      if ("cmd_pause" == cmd && !enabled) {
        // We need to add the tooltip indicating that the download cannot be
        // paused now.
        buttons[i].setAttribute("tooltiptext", gStr.cannotPause);
      }
    }

    if (this.done) {
      // getTypeFromFile fails if it can't find a type for this file.
      try {
        // Refresh the icon, so that executable icons are shown.
        let mimeService = Cc["@mozilla.org/mime;1"].
                          getService(Ci.nsIMIMEService);
        let contentType = mimeService.getTypeFromFile(this.localFile);

        let oldImage = this.element.getAttribute("image");
        // Tacking on contentType bypasses cache
        this.element.setAttribute("image", oldImage + "&contentType=" + contentType);
      } catch (e) {}
    }
  },

  /**
   * Check if the download matches the provided search term based on the texts
   * shown to the user. All search terms are checked to see if each matches any
   * of the displayed texts.
   *
   * @return Boolean true if it matches the search; false otherwise
   */
  matchesSearch: function(aTerms, aAttributes) {
    return aTerms.some(term => aAttributes.some(attr => this.element.getAttribute(attr).includes(term)));
  },

  isCommandEnabled: function(aCommand) {
    switch (aCommand) {
      case "cmd_cancel":
        return this.inProgress;

      case "cmd_open":
        return this.openable && this.localFile.exists();

      case "cmd_show":
        return this.localFile.exists();

      case "cmd_pause":
        return this.inProgress && !this.paused && this.resumable;

      case "cmd_pauseResume":
        return (this.inProgress || this.paused) && this.resumable;

      case "cmd_resume":
        return this.paused && this.resumable;

      case "cmd_openReferrer":
        return !!this.referrer;

      case "cmd_removeFromList":
        return this.removable;

      case "cmd_retry":
        return this.canRetry;

      case "cmd_copyLocation":
        return true;
    }

    return false;
  },

  doCommand: function(aCommand) {
    if (!this.isCommandEnabled(aCommand)) {
      return;
    }

    switch (aCommand) {
      case "cmd_cancel":
        this.cancel().catch(Cu.reportError);
        break;

      case "cmd_open":
        this.openLocalFile().catch(Cu.reportError);
        break;

      case "cmd_show":
        this.showLocalFile();
        break;

      case "cmd_pause":
        this.pause().catch(Cu.reportError);
        break;

      case "cmd_pauseResume":
        if (this.paused) {
          this.resume().catch(Cu.reportError);
        } else {
          this.pause().catch(Cu.reportError);
        }
        break;

      case "cmd_resume":
        this.resume().catch(Cu.reportError);
        break;

      case "cmd_openReferrer":
        let uri = Services.io.newURI(this.referrer || this.uri, null, null);

        // Direct the URL to the default browser.
        Cc["@mozilla.org/uriloader/external-protocol-service;1"].
        getService(Ci.nsIExternalProtocolService).
        getProtocolHandlerInfo(uri.scheme).
        launchWithURI(uri);
        break;

      case "cmd_removeFromList":
        this.remove().catch(Cu.reportError);
        break;

      case "cmd_retry":
        this.retry().catch(Cu.reportError);
        break;

      case "cmd_copyLocation":
        let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].
                        getService(Ci.nsIClipboardHelper);
        clipboard.copyString(this.uri);
        break;
    }
  },
};

var gDownloadList = {
  downloadItemsMap: new Map(),
  idToDownloadItemMap: new Map(),
  _autoIncrementID: 0,
  downloadView: null,
  searchBox: null,
  searchTerms: [],
  searchAttributes: [ "target", "status", "dateTime", ],
  contextMenus: [
    // DOWNLOAD_DOWNLOADING
    [
      "menuitem_pause",
      "menuitem_cancel",
      "menuseparator",
      "menuitem_show",
      "menuseparator",
      "menuitem_openReferrer",
      "menuitem_copyLocation",
      "menuseparator",
      "menuitem_selectAll",
    ],
    // DOWNLOAD_FINISHED
    [
      "menuitem_open",
      "menuitem_show",
      "menuseparator",
      "menuitem_openReferrer",
      "menuitem_copyLocation",
      "menuseparator",
      "menuitem_selectAll",
      "menuseparator",
      "menuitem_removeFromList"
    ],
    // DOWNLOAD_FAILED
    [
      "menuitem_retry",
      "menuseparator",
      "menuitem_openReferrer",
      "menuitem_copyLocation",
      "menuseparator",
      "menuitem_selectAll",
      "menuseparator",
      "menuitem_removeFromList",
    ],
    // DOWNLOAD_CANCELED
    [
      "menuitem_retry",
      "menuseparator",
      "menuitem_openReferrer",
      "menuitem_copyLocation",
      "menuseparator",
      "menuitem_selectAll",
      "menuseparator",
      "menuitem_removeFromList",
    ],
    // DOWNLOAD_PAUSED
    [
      "menuitem_resume",
      "menuitem_cancel",
      "menuseparator",
      "menuitem_show",
      "menuseparator",
      "menuitem_openReferrer",
      "menuitem_copyLocation",
      "menuseparator",
      "menuitem_selectAll",
    ],
    // DOWNLOAD_QUEUED
    [
      "menuitem_cancel",
      "menuseparator",
      "menuitem_show",
      "menuseparator",
      "menuitem_openReferrer",
      "menuitem_copyLocation",
      "menuseparator",
      "menuitem_selectAll",
    ],
    // DOWNLOAD_BLOCKED_PARENTAL
    [
      "menuitem_openReferrer",
      "menuitem_copyLocation",
      "menuseparator",
      "menuitem_selectAll",
      "menuseparator",
      "menuitem_removeFromList",
    ],
    // DOWNLOAD_SCANNING
    [
      "menuitem_show",
      "menuseparator",
      "menuitem_openReferrer",
      "menuitem_copyLocation",
      "menuseparator",
      "menuitem_selectAll",
    ],
    // DOWNLOAD_DIRTY
    [
      "menuitem_openReferrer",
      "menuitem_copyLocation",
      "menuseparator",
      "menuitem_selectAll",
      "menuseparator",
      "menuitem_removeFromList",
    ],
    // DOWNLOAD_BLOCKED_POLICY
    [
      "menuitem_openReferrer",
      "menuitem_copyLocation",
      "menuseparator",
      "menuitem_selectAll",
      "menuseparator",
      "menuitem_removeFromList",
    ]
  ],

  init: function() {
    this.downloadView = document.getElementById("downloadView");
    this.searchBox = document.getElementById("searchbox");

    this.buildList(true);

    this.downloadView.focus();

    // Clear the search box and move focus to the list on escape from the box.
    this.searchBox.addEventListener("keypress", (e) => {
      this.searchBoxKeyPressHandler(e);
    }, false);

    Downloads.getList(Downloads.ALL)
             .then(list => list.addView(this))
             .catch(Cu.reportError);
  },

  buildList: function(aForceBuild) {
    // Stringify the previous search.
    let prevSearch = this.searchTerms.join(" ");

    // Array of space-separated lower-case search terms.
    this.searchTerms = this.searchBox.value.trim().toLowerCase().split(/\s+/);

    // Unless forced, don't rebuild the download list if the search didn't change.
    if (!aForceBuild && this.searchTerms.join(" ") == prevSearch) {
      return;
    }

    // Clear the list before adding items by replacing with a shallow copy.
    let empty = this.downloadView.cloneNode(false);
    this.downloadView.parentNode.replaceChild(empty, this.downloadView);
    this.downloadView = empty;

    for (let downloadItem of this.idToDownloadItemMap.values()) {
      if (downloadItem.inProgress ||
          downloadItem.matchesSearch(this.searchTerms, this.searchAttributes)) {
        this.downloadView.appendChild(downloadItem.element);

        // Because of the joys of XBL, we can't update the buttons until the
        // download object is in the document.
        downloadItem.updateView();
      }
    }

    this.updateClearListButton();
  },

  /**
   * Remove the finished downloads from the shown download list.
   */
  clearList: Task.async(function*() {
    let searchTerms = this.searchTerms;

    let list = yield Downloads.getList(Downloads.ALL);

    yield list.removeFinished((aDownload) => {
      let downloadItem = this.downloadItemsMap.get(aDownload);
      if (!downloadItem) {
        Cu.reportError("Download doesn't exist.");
        return;
      }

      return downloadItem.matchesSearch(searchTerms, this.searchAttributes);
    });

    // Clear the input as if the user did it and move focus to the list.
    this.searchBox.value = "";
    this.searchBox.doCommand();
    this.downloadView.focus();
  }),

  /**
   * Update the disabled state of the clear list button based on whether or not
   * there are items in the list that can potentially be removed.
   */
  updateClearListButton: function() {
    let button = document.getElementById("clearListButton");

    // The button is enabled if we have items in the list that we can clean up.
    for (let downloadItem of this.idToDownloadItemMap.values()) {
      if (!downloadItem.inProgress &&
          downloadItem.matchesSearch(this.searchTerms, this.searchAttributes)) {
        button.disabled = false;
        return;
      }
    }

    button.disabled = true;
  },

  setSearchboxFocus: function() {
    this.searchBox.focus();
    this.searchBox.select();
  },

  searchBoxKeyPressHandler: function(aEvent) {
    if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE) {
      // Move focus to the list instead of closing the window.
      this.downloadView.focus();
      aEvent.preventDefault();
    }
  },

  buildContextMenu: function(aEvent) {
    if (aEvent.target.id != "downloadContextMenu") {
      return false;
    }

    let popup = document.getElementById("downloadContextMenu");
    while (popup.hasChildNodes()) {
      popup.removeChild(popup.firstChild);
    }

    if (this.downloadView.selectedItem) {
      let downloadItem = this._getSelectedDownloadItem();

      let idx = downloadItem.state;
      if (idx < 0) {
        idx = 0;
      }

      let menus = this.contextMenus[idx];
      for (let i = 0; i < menus.length; ++i) {
        let menuitem = document.getElementById(menus[i]).cloneNode(true);
        let cmd = menuitem.getAttribute("cmd");
        if (cmd) {
          menuitem.disabled = !downloadItem.isCommandEnabled(cmd);
        }

        popup.appendChild(menuitem);
      }

      return true;
    }

    return false;
  },

  /**
   * Perform the default action for the currently selected download item.
   */
  doDefaultForSelected: function() {
    // Make sure we have something selected.
    let download = this._getSelectedDownloadItem();
    if (!download) {
      return;
    }

    // Get the default action (first item in the menu).
    let menuitem = document.getElementById(this.contextMenus[download.state][0]);

    // Try to do the action if the command is enabled.
    download.doCommand(menuitem.getAttribute("cmd"));
  },

  onDownloadDblClick: function(aEvent) {
    // Only do the default action for double primary clicks.
    if (aEvent.button == 0 && aEvent.target.selected) {
      this.doDefaultForSelected();
    }
  },

  /**
   * Helper function to do commands.
   *
   * @param aCmd
   *        The command to be performed.
   * @param aItem
   *        The richlistitem that represents the download that will have the
   *        command performed on it. If this is null, the command is performed on
   *        all downloads. If the item passed in is not a richlistitem that
   *        represents a download, it will walk up the parent nodes until it finds
   *        a DOM node that is.
   */
  performCommand: function(aCmd, aItem) {
    if (!aItem) {
      // Convert the nodelist into an array to keep a copy of the download items.
      let items = Array.from(this.downloadView.selectedItems);

      // Do the command for each download item.
      for (let item of items) {
        this.performCommand(aCmd, item);
      }

      return;
    }

    let elm = aItem;

    while (elm.nodeName != "richlistitem" ||
           elm.getAttribute("type") != "download") {
      elm = elm.parentNode;
    }

    let downloadItem = this._getDownloadItemForElement(elm);
    downloadItem.doCommand(aCmd);
  },

  onDragStart: function(aEvent) {
    let downloadItem = this._getSelectedDownloadItem();
    if (!downloadItem) {
      return;
    }

    let dl = this.downloadView.selectedItem;
    let f = downloadItem.localFile;
    if (!f.exists()) {
      return;
    }

    let dt = aEvent.dataTransfer;
    dt.mozSetDataAt("application/x-moz-file", f, 0);
    let url = Services.io.newFileURI(f).spec;
    dt.setData("text/uri-list", url);
    dt.setData("text/plain", url);
    dt.effectAllowed = "copyMove";
    dt.addElement(dl);
  },

  onDragOver: function(aEvent) {
    let types = aEvent.dataTransfer.types;
    if (types.contains("text/uri-list") ||
        types.contains("text/x-moz-url") ||
        types.contains("text/plain")) {
      aEvent.preventDefault();
    }

    aEvent.stopPropagation();
  },

  onDrop: function(aEvent) {
    let dt = aEvent.dataTransfer;
    // If dragged item is from our source, do not try to
    // redownload already downloaded file.
    if (dt.mozGetDataAt("application/x-moz-file", 0)) {
      return;
    }

    let name;
    let url = dt.getData("URL");

    if (!url) {
      url = dt.getData("text/x-moz-url") || dt.getData("text/plain");
      [url, name] = url.split("\n");
    }

    if (url) {
      let sourceDoc = dt.mozSourceNode ? dt.mozSourceNode.ownerDocument : document;
      saveURL(url, name ? name : url, null, true, true, null, sourceDoc);
    }
  },

  pasteHandler: function() {
    let trans = Cc["@mozilla.org/widget/transferable;1"].
                createInstance(Ci.nsITransferable);
    trans.init(null);
    let flavors = ["text/x-moz-url", "text/unicode"];
    flavors.forEach(trans.addDataFlavor);

    Services.clipboard.getData(trans, Services.clipboard.kGlobalClipboard);

    // Getting the data or creating the nsIURI might fail
    try {
      let data = {};
      trans.getAnyTransferData({}, data, {});
      let [url, name] = data.value.QueryInterface(Ci.nsISupportsString).data.split("\n");

      if (!url) {
        return;
      }

      let uri = Services.io.newURI(url, null, null);

      saveURL(uri.spec, name || uri.spec, null, true, true, null, document);
    } catch (ex) {}
  },

  selectAll: function() {
    this.downloadView.selectAll();
  },

  onUpdateProgress: function() {
    let numActive = 0;
    let totalSize = 0;
    let totalTransferred = 0;

    for (let downloadItem of this.idToDownloadItemMap.values()) {
      if (!downloadItem.inProgress) {
        continue;
      }

      numActive++;

      // Only add to total values if we actually know the download size.
      if (downloadItem.maxBytes > 0 &&
          downloadItem.state != nsIDM.DOWNLOAD_CANCELED &&
          downloadItem.state != nsIDM.DOWNLOAD_FAILED) {
        totalSize += downloadItem.maxBytes;
        totalTransferred += downloadItem.currBytes;
      }
    }

    // Use the default title and reset "last" values if there are no downloads.
    if (numActive == 0) {
      document.title = document.documentElement.getAttribute("statictitle");
      return;
    }

    // Establish the mean transfer speed and amount downloaded.
    let mean = totalTransferred;

    // Calculate the percent transferred, unless we don't have a total file size.
    let title = gStr.downloadsTitlePercent;
    if (totalSize == 0) {
      title = gStr.downloadsTitleFiles;
    } else {
      mean = Math.floor((totalTransferred / totalSize) * 100);
    }

    // Get the correct plural form and insert number of downloads and percent.
    title = PluralForm.get(numActive, title);
    title = title.replace("#1", numActive);
    title = title.replace("#2", mean);

    // Update title of window.
    document.title = title;
  },

  onDownloadAdded: function(aDownload) {
    let newID = this._autoIncrementID++;

    let downloadItem = new DownloadItem(newID, aDownload);
    this.downloadItemsMap.set(aDownload, downloadItem);
    this.idToDownloadItemMap.set(newID, downloadItem);

    if (downloadItem.inProgress ||
        downloadItem.matchesSearch(this.searchTerms, this.searchAttributes)) {
      this.downloadView.appendChild(downloadItem.element);

      // Because of the joys of XBL, we can't update the buttons until the
      // download object is in the document.
      downloadItem.updateView();

      // We might have added an item to an empty list, so update button.
      this.updateClearListButton();
    }
  },

  onDownloadChanged: function(aDownload) {
    let downloadItem = this.downloadItemsMap.get(aDownload);
    if (!downloadItem) {
      Cu.reportError("Download doesn't exist.");
      return;
    }

    downloadItem.updateView();

    this.onUpdateProgress();

    // The download may have changed state, so update the clear list button.
    this.updateClearListButton();

    if (downloadItem.done) {
      // Move the download below active if it should stay in the list.
      if (downloadItem.matchesSearch(this.searchTerms, this.searchAttributes)) {
        // Iterate down until we find a non-active download.
        let next = this.element.nextSibling;
        while (next && next.inProgress) {
          next = next.nextSibling;
        }

        // Move the item.
        this.downloadView.insertBefore(this.element, next);
      } else {
        this.removeFromView(downloadItem);
      }

      return true;
    }

    return false;
  },

  onDownloadRemoved: function(aDownload) {
    let downloadItem = this.downloadItemsMap.get(aDownload);
    if (!downloadItem) {
      Cu.reportError("Download doesn't exist.");
      return;
    }

    this.downloadItemsMap.delete(aDownload);
    this.idToDownloadItemMap.delete(downloadItem.id);

    this.removeFromView(downloadItem);
  },

  removeFromView: function(aDownloadItem) {
    // Make sure we have an item to remove.
    if (!aDownloadItem.element) {
      return;
    }

    let index = this.downloadView.selectedIndex;
    this.downloadView.removeChild(aDownloadItem.element);
    this.downloadView.selectedIndex = Math.min(index, this.downloadView.itemCount - 1);

    // We might have removed the last item, so update the clear list button.
    this.updateClearListButton();
  },
  _getDownloadItemForElement(element) {
    return this.idToDownloadItemMap.get(element.getAttribute("id"));
  },
  _getSelectedDownloadItem() {
    let dl = this.downloadView.selectedItem;
    return dl ? this._getDownloadItemForElement(dl) : null;
  },
};

function Startup() {
  // Convert strings to those in the string bundle.
  let sb = document.getElementById("downloadStrings");
  let strings = ["paused", "cannotPause", "doneStatus", "doneSize",
                 "doneSizeUnknown", "stateFailed", "stateCanceled",
                 "stateBlocked", "stateBlockedPolicy", "stateDirty",
                 "downloadsTitleFiles", "downloadsTitlePercent",];

  for (let name of strings) {
    gStr[name] = sb.getString(name);
  }

  gDownloadList.init();

  let DownloadTaskbarProgress =
    Cu.import("resource://gre/modules/DownloadTaskbarProgress.jsm", {}).DownloadTaskbarProgress;
  DownloadTaskbarProgress.onDownloadWindowLoad(window);
}

function Shutdown() {
  Downloads.getList(Downloads.ALL)
           .then(list => list.removeView(gDownloadList))
           .catch(Cu.reportError);
}
