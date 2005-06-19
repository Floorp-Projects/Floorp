/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Update Service.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Goodger <ben@mozilla.org> (Original Author)
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

const nsIUpdateItem           = Components.interfaces.nsIUpdateItem;
const nsIIncrementalDownload  = Components.interfaces.nsIIncrementalDownload;

const XMLNS_XUL               = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const PREF_UPDATE_MANUAL_URL  = "app.update.url.manual";

const URI_UPDATES_PROPERTIES  = "chrome://mozapps/locale/update/updates.properties";

const STATE_DOWNLOADING       = "downloading";
const STATE_PENDING           = "pending";
const STATE_APPLYING          = "applying";
const STATE_SUCCEEDED         = "succeeded";
const STATE_FAILED            = "failed";

/**
 * Logs a string to the error console. 
 * @param   string
 *          The string to write to the error console..
 */  
function LOG(string) {
  dump("*** " + string + "\n");
}

var gUpdates = {
  update: null,
  updateStrings: null,
  brandStrings: null,
  onClose: function() {
    var objects = {
      checking: gCheckingPage,
      updatesfound: gUpdatesAvailablePage
    };
    var pageid = document.documentElement.currentPage.pageid;
    if ("onClose" in objects[pageid]) 
      objects[pageid].onClose();
  },
  
  onLoad: function() {
    if (window.arguments) {
      var um = Components.classes["@mozilla.org/updates/update-manager;1"]
                         .getService(Components.interfaces.nsIUpdateManager);
      var arg0 = window.arguments[0];
      if (arg0 instanceof Components.interfaces.nsIUpdate) {
        this.update = arg0;
        document.documentElement.advance();
      }
      else if (arg0 == 1 || (arg0 == 0 && um.activeUpdate)) {
        var checking = document.getElementById("checking");
        checking.setAttribute("next", "downloading");
        document.documentElement.advance();
      }
    }
    
    this.updateStrings = document.getElementById("updateStrings");
    this.brandStrings = document.getElementById("brandStrings");
  },
  
  advanceToErrorPage: function(currentPage, reason) {
    currentPage.setAttribute("next", "errors");
    var errorReason = document.getElementById("errorReason");
    errorReason.value = reason;
    var errorLink = document.getElementById("errorLink");
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch2);
    var manualURL = pref.getComplexValue(PREF_UPDATE_MANUAL_URL, 
                                         Components.interfaces.nsIPrefLocalizedString);
    errorLink.href = manualURL.data;
    var errorLinkLabel = document.getElementById("errorLinkLabel");
    errorLinkLabel.value = manualURL.data;
    
    var pageTitle = this.updateStrings.getString("errorsPageHeader");
    
    var errorPage = document.getElementById("errors");
    errorPage.setAttribute("label", pageTitle);
    document.documentElement.setAttribute("label", pageTitle);
    document.documentElement.advance();
  },
  
  set headerVisible(visible) {
    document.documentElement.setAttribute("label", visible ? " " : "");
  }
}

var gCheckingPage = {
  /**
   * The nsIUpdateChecker that is currently checking for updates. We hold onto 
   * this so we can cancel the update check if the user closes the window.
   */
  _checker: null,
  
  /**
   * Starts the update check when the page is shown.
   */
  onPageShow: function() {
    var wiz = document.documentElement;
    wiz.getButton("next").disabled = true;
    
    var aus = Components.classes["@mozilla.org/updates/update-service;1"]
                        .getService(Components.interfaces.nsIApplicationUpdateService);
    this._checker = aus.checkForUpdates(this.updateListener);
    
    gUpdates.headerVisible = true;
  },
  
  /**
   * The user has closed the window, either by pressing cancel or using a Window
   * Manager control, so stop checking for updates.
   */
  onClose: function() {
    if (this._checker)
      this._checker.stopChecking();
  },
  
  updateListener: {
    /**
     * See nsIUpdateCheckListener.idl
     */
    onProgress: function(request, position, totalSize) {
      var pm = document.getElementById("checkingProgress");
      checkingProgress.setAttribute("mode", "normal");
      checkingProgress.setAttribute("value", Math.floor(100 * (position/totalSize)));
    },

    /**
     * See nsIUpdateCheckListener.idl
     */
    onCheckComplete: function(updates, updateCount) {
      var aus = Components.classes["@mozilla.org/updates/update-service;1"]
                          .getService(Components.interfaces.nsIApplicationUpdateService);
      gUpdates.update = aus.selectUpdate(updates, updates.length);
      if (!gUpdates.update) {
        LOG("Could not select an appropriate update, either because there were none," + 
            " or |selectUpdate| failed.");
        var checking = document.getElementById("checking");
        checking.setAttribute("next", "noupdatesfound");
      }
      document.documentElement.advance();
    },

    /**
     * See nsIUpdateCheckListener.idl
     */
    onError: function() {
      LOG("UpdateCheckListener: ERROR");
    },
    
    /**
     * See nsISupports.idl
     */
    QueryInterface: function(iid) {
      if (!aIID.equals(Components.interfaces.nsIUpdateCheckListener) &&
          !aIID.equals(Components.interfaces.nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;
      return this;
    }
  }
};

var gNoUpdatesPage = {
  onPageShow: function() {
    document.documentElement.getButton("back").disabled = true;
    document.documentElement.getButton("cancel").disabled = true;
    document.documentElement.getButton("finish").focus();
  }
};

var gUpdatesAvailablePage = {
  _incompatibleItems: null,
  
  onPageShow: function() {
    var brandName = gUpdates.brandStrings.getString("brandShortName");
    var updateName = gUpdates.updateStrings.getFormattedString("updateName", 
                                                              [brandName, gUpdates.update.version]);
    var updateNameElement = document.getElementById("updateName");
    updateNameElement.value = updateName;
    var displayType = gUpdates.updateStrings.getString("updateType_" + gUpdates.update.type);
    var updateTypeElement = document.getElementById("updateType");
    updateTypeElement.setAttribute("type", gUpdates.update.type);
    var intro = gUpdates.updateStrings.getFormattedString("introType_" + gUpdates.update.type, [brandName]);
    while (updateTypeElement.hasChildNodes())
      updateTypeElement.removeChild(updateTypeElement.firstChild);
    updateTypeElement.appendChild(document.createTextNode(intro));
    
    var updateMoreInfoURL = document.getElementById("updateMoreInfoURL");
    updateMoreInfoURL.href = gUpdates.update.detailsURL;
    
    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                       .getService(Components.interfaces.nsIExtensionManager);
    var items = em.getIncompatibleItemList("", gUpdates.update.version,
                                           nsIUpdateItem.TYPE_ADDON, { });
    if (items.length > 0) {
      // There are addons that are incompatible with this update, so show the 
      // warning message.
      var incompatibleWarning = document.getElementById("incompatibleWarning");
      incompatibleWarning.hidden = false;
      
      this._incompatibleItems = items;
    }
    
    var dlButton = document.getElementById("download-button");
    dlButton.focus();
    
    gUpdates.headerVisible = false;
  },
  
  onInstallNow: function() {
    var nextPageID = gUpdates.update.licenseURL ? "license" : "downloading";
    var updatesfound = document.getElementById("updatesfound");
    updatesfound.setAttribute("next", nextPageID);
    document.documentElement.advance();
  },
  
  showIncompatibleItems: function() {
    openDialog("chrome://mozapps/content/update/incompatible.xul", "", 
               "dialog,centerscreen,modal,resizable,titlebar", this._incompatibleItems);
  }
};

var gLicensePage = {
  _licenseContent: null,
  onPageShow: function() {
    this._licenseContent = document.getElementById("licenseContent");
    
    var nextButton = document.documentElement.getButton("next");
    nextButton.disabled = true;
    nextButton.label = gUpdates.updateStrings.getString("IAgreeLabel");
    document.documentElement.getButton("back").disabled = true;
    document.documentElement.getButton("next").focus();

    this._licenseContent.addEventListener("load", this.onLicenseLoad, false);
    this._licenseContent.url = gUpdates.update.licenseURL;
    
    gUpdates.headerVisible = true;
  },
  
  onLicenseLoad: function() {
    document.documentElement.getButton("next").disabled = false;
  },
  
  onClose: function() {
    this._licenseContent.stopDownloading();
  }
};

/**
 * Formats status messages for a download operation based on the progress
 * of the download.
 * @constructor
 */
function DownloadStatusFormatter() {
  this._startTime = Math.floor((new Date()).getTime() / 1000);
  this._elapsed = 0;
  
  var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                      .getService(Components.interfaces.nsIStringBundleService);
  var bundle = sbs.createBundle(URI_UPDATES_PROPERTIES);
  
  this._statusFormat = bundle.GetStringFromName("statusFormat");
  this._statusFormatKBMB = bundle.GetStringFromName("statusFormatKBMB");
  this._statusFormatKBKB = bundle.GetStringFromName("statusFormatKBKB");
  this._statusFormatMBMB = bundle.GetStringFromName("statusFormatMBMB");
  this._statusFormatUnknownMB = bundle.GetStringFromName("statusFormatUnknownMB");
  this._statusFormatUnknownKB = bundle.GetStringFromName("statusFormatUnknownKB");
  this._remain = bundle.GetStringFromName("remain");
  this._unknownFilesize = bundle.GetStringFromName("unknownFilesize");
  this._longTimeFormat = bundle.GetStringFromName("longTimeFormat");
  this._shortTimeFormat = bundle.GetStringFromName("shortTimeFormat");
}
DownloadStatusFormatter.prototype = {
  /**
   * Time when the download started (in seconds since epoch)
   */
  _startTime: 0,

  /**
   * Time elapsed since the start of the download operation (in seconds)
   */
  _elapsed: 0,
  
  /**
   * Transfer rate of the download
   */
  rate: 0,
  
  /**
   * Number of Kilobytes downloaded so far in the form:
   *  376KB of 9.3MB
   */
  progress: "",

  /**
   * Format a human-readable status message based on the current download
   * progress.
   * @param   currSize
   *          The current number of bytes transferred
   * @param   finalSize
   *          The total number of bytes to be transferred
   * @returns A human readable status message, e.g.
   *          "3.4 of 4.7MB; 01:15 remain"
   */
  formatStatus: function(currSize, finalSize) {
    var now = Math.floor((new Date()).getTime() / 1000);
    
    // 1) Determine the Download Progress in Kilobytes
    var total = parseInt(finalSize/1024 + 0.5);
    this.progress = this._formatKBytes(parseInt(currSize/1024 + 0.5), total);
    
    // 2) Determine the Transfer Rate
    if (!this.rate || now > (this._startTime + this._elapsed + 2)) {
      this._elapsed = now - this._startTime;
      this.rate = this._elapsed ? (currSize / 1024) / this._elapsed : 0;
      if (this.rate > 100)
        this.rate = Math.round(this.rate);
      if (this.rate == 0)
        this.rate = "??.?";
    }

    // 3) Determine the Time Remaining
    var remainingTime = this._unknownFileSize;
    if (this.rate && (finalSize > 0)) {
      remainingTime = Math.floor(((finalSize - currSize) / 1024) / this.rate);
      remainingTime = this._formatSeconds(remainingTime); 
    }
      
    var status = this._statusFormat;
    status = this._replaceInsert(status, 1, this.progress);
    status = this._replaceInsert(status, 2, this.rate);
    status = this._replaceInsert(status, 3, remainingTime);
    status = this._replaceInsert(status, 4, this._remain);
    return status;
  },

  /**
   * Inserts a string into another string at the specified index, e.g. for
   * the format string var foo ="#1 #2 #3", |_replaceInsert(foo, 2, "test")|
   * returns "#1 test #3";
   * @param   format
   *          The format string
   * @param   index
   *          The Index to insert into
   * @param   value
   *          The value to insert
   * @returns The string with the value inserted. 
   */  
  _replaceInsert: function(format, index, value) {
    return format.replace(new RegExp("#" + index), value);
  },

  /**
   * Formats progress in the form of kilobytes transfered vs. total to 
   * transfer.
   * @param   currentKB
   *          The current amount of data transfered, in kilobytes.
   * @param   totalKB
   *          The total amount of data that must be transfered, in kilobytes.
   * @returns A string representation of the progress, formatted according to:
   * 
   *            KB           totalKB           returns
   *            x, < 1MB     y < 1MB           x of y KB
   *            x, < 1MB     y >= 1MB          x KB of y MB
   *            x, >= 1MB    y >= 1MB          x of y MB
   */
   _formatKBytes: function(currentKB, totalKB) {
    var progressHasMB = parseInt(currentKB / 1024) > 0;
    var totalHasMB = parseInt(totalKB / 1024) > 0;
    
    var format = "";
    if (!progressHasMB && !totalHasMB) {
      if (!totalKB) {
        format = this._statusFormatUnknownKB;
        format = this._replaceInsert(format, 1, currentKB);
      } else {
        format = this._statusFormatKBKB;
        format = this._replaceInsert(format, 1, currentKB);
        format = this._replaceInsert(format, 2, totalKB);
      }
    }
    else if (progressHasMB && totalHasMB) {
      format = this._statusFormatMBMB;
      format = this._replaceInsert(format, 1, (currentKB / 1024).toFixed(1));
      format = this._replaceInsert(format, 2, (totalKB / 1024).toFixed(1));
    }
    else if (totalHasMB && !progressHasMB) {
      format = this._statusFormatKBMB;
      format = this._replaceInsert(format, 1, currentKB);
      format = this._replaceInsert(format, 2, (totalKB / 1024).toFixed(1));
    }
    else if (progressHasMB && !totalHasMB) {
      format = this._statusFormatUnknownMB;
      format = this._replaceInsert(format, 1, (currentKB / 1024).toFixed(1));
    }
    return format;  
  },

  /**
   * Formats a time in seconds into something human readable.
   * @param   seconds
   *          The time to format
   * @returns A human readable string representing the date.
   */
  _formatSeconds: function(seconds) {
    // Determine number of hours/minutes/seconds
    var hours = (seconds - (seconds % 3600)) / 3600;
    seconds -= hours * 3600;
    var minutes = (seconds - (seconds % 60)) / 60;
    seconds -= minutes * 60;
    
    // Pad single digit values
    if (hours < 10)
      hours = "0" + hours;
    if (minutes < 10)
      minutes = "0" + minutes;
    if (seconds < 10)
      seconds = "0" + seconds;
    
    // Insert hours, minutes, and seconds into result string.
    var result = parseInt(hours) ? this._longTimeFormat : this._shortTimeFormat;
    result = this._replaceInsert(result, 1, hours);
    result = this._replaceInsert(result, 2, minutes);
    result = this._replaceInsert(result, 3, seconds);

    return result;
  }
};

var gDownloadingPage = {
  /** 
   * DOM Elements
   */
  _downloadName     : null,
  _downloadStatus   : null,
  _downloadProgress : null,
  _downloadThrobber : null,
  _pauseButton      : null,
  
  /** 
   * An instance of the status formatter object
   */
  _statusFormatter  : null,
  
  /** 
   *
   */
  onPageShow: function() {
    this._downloadName = document.getElementById("downloadName");
    this._downloadStatus = document.getElementById("downloadStatus");
    this._downloadProgress = document.getElementById("downloadProgress");
    this._downloadThrobber = document.getElementById("downloadThrobber");
    this._pauseButton = document.getElementById("pauseButton");
  
    if (gUpdates.update) {
      // Add this UI as a listener for active downloads
      var updates = 
          Components.classes["@mozilla.org/updates/update-service;1"].
          getService(Components.interfaces.nsIApplicationUpdateService);
      var state = updates.downloadUpdate(gUpdates.update, false);
      if (state == "failed") 
        this.showVerificationError();
      else
        updates.addDownloadListener(this);
    }
    
    document.documentElement.getButton("back").disabled = true;
    document.documentElement.getButton("next").disabled = true;
    var cancelButton = document.documentElement.getButton("cancel");
    cancelButton.label = gUpdates.updateStrings.getString("closeButtonLabel");
    cancelButton.focus();
    gUpdates.headerVisible = true;
  },
  
  /** 
   *
   */
  _setStatus: function(status) {
    while (this._downloadStatus.hasChildNodes())
      this._downloadStatus.removeChild(this._downloadStatus.firstChild);
    this._downloadStatus.appendChild(document.createTextNode(status));
  },
  
  _paused: false,
  _oldStatus: "",
  _oldName: "",
  _oldMode: "",

  /** 
   *
   */
  onPause: function() {
    var updates = 
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);
    if (this._paused) {
      updates.downloadUpdate(gUpdates.update, false);
      this._downloadName.value = gUpdates.updateStrings.getFormattedString("downloadingPrefix", [gUpdates.update.name]);
      this._setStatus(this._oldStatus);
      this._downloadProgress.mode = this._oldMode;
      this._pauseButton.label = gUpdates.updateStrings.getString("pauseButtonPause");
    }
    else {
      updates.pauseDownload();
      this._oldStatus = this._downloadStatus.textContent;
      this._oldMode = this._downloadProgress.mode;
      this._downloadName.value = gUpdates.updateStrings.getFormattedString("pausedName", [gUpdates.update.name]);
      this._setStatus(gUpdates.updateStrings.getFormattedString("pausedStatus", 
          [this._statusFormatter.progress]));
      this._downloadProgress.mode = "normal";
      this._pauseButton.label = gUpdates.updateStrings.getString("pauseButtonResume");
    }
    this._paused = !this._paused;
  },
  
  /** 
   *
   */
  onClose: function() {
    // Remove ourself as a download listener so that we don't continue to be 
    // fed progress and state notifications after the UI we're updating has 
    // gone away.
    var updates = 
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);
    updates.removeDownloadListener(this);
    
    var um = 
        Components.classes["@mozilla.org/updates/update-manager;1"]
                  .getService(Components.interfaces.nsIUpdateManager);
    um.activeUpdate = gUpdates.update;
  },
  
  /** 
   *
   */
  onStartRequest: function(request, context) {
    request.QueryInterface(nsIIncrementalDownload);
    LOG("gDownloadingPage.onStartRequest: " + request.URI.spec);

    this._statusFormatter = new DownloadStatusFormatter();
    
    this._downloadThrobber.setAttribute("state", "loading");
  },
  
  /** 
   *
   */
  onProgress: function(request, context, progress, maxProgress) {
    request.QueryInterface(nsIIncrementalDownload);
    // LOG("gDownloadingPage.onProgress: " + request.URI.spec + ", " + progress + "/" + maxProgress);

    gUpdates.update.selectedPatch.status = 
      this._statusFormatter.formatStatus(progress, maxProgress);

    this._downloadProgress.mode = "normal";
    this._downloadProgress.value = gUpdates.update.selectedPatch.progress;
    this._pauseButton.disabled = false;
    var name = gUpdates.updateStrings.getFormattedString("downloadingPrefix", [gUpdates.update.name]);
    this._downloadName.value = name;
    this._setStatus(gUpdates.update.selectedPatch.status);
  },
  
  /** 
   *
   */
  onStatus: function(request, context, status, statusText) {
    request.QueryInterface(nsIIncrementalDownload);
    LOG("gDownloadingPage.onStatus: " + request.URI.spec + " status = " + status + ", text = " + statusText);
  },
  
  /** 
   *
   */
  onStopRequest: function(request, context, status) {
    request.QueryInterface(nsIIncrementalDownload);
    LOG("gDownloadingPage.onStopRequest: " + request.URI.spec + ", status = " + status);
    
    this._downloadThrobber.removeAttribute("state");

    const NS_BINDING_ABORTED = 0x804b0002;
    switch (status) {
    case Components.results.NS_ERROR_UNEXPECTED:
      if (gUpdates.update.selectedPatch.state == STATE_FAILED)
        this.showVerificationError();
      else {
        // Verification failed for a partial patch, complete patch is now
        // downloading so return early and do NOT remove the download listener!
        
        // Reset the progress meter to "undertermined" mode so that we don't 
        // show old progress for the new download of the "complete" patch.
        this._downloadProgress.mode = "undetermined";
        this._pauseButton.disabled = true;
        return;
      }
      break;
    case NS_BINDING_ABORTED:
      LOG("gDownloadingPage.onStopRequest: Pausing Download");
      // Return early, do not remove UI listener since the user may resume
      // downloading again.
      return;
    case Components.results.NS_OK:
      LOG("gDownloadingPage.onStopRequest: Patch Verification Succeeded");
      document.documentElement.advance();
      break;
    }

    var updates = 
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);
    updates.removeDownloadListener(this);
  },
  
  /** 
   * Advance the wizard to the "Verification Error" page
   */
  showVerificationError: function() {
    var brandName = gUpdates.brandStrings.getString("brandShortName");
    var verificationError = gUpdates.updateStrings.getFormattedString("verificationError", [brandName]);
    var downloadingPage = document.getElementById("downloading");
    gUpdates.advanceToErrorPage(downloadingPage, verificationError);
  },
   
  /**
   * See nsISupports.idl
   */
  QueryInterface: function(iid) {
    if (!iid.equals(Components.interfaces.nsIRequestObserver) &&
        !iid.equals(Components.interfaces.nsIProgressEventSink) &&
        !iid.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

var gErrorsPage = {
  onPageShow: function() {
    document.documentElement.getButton("back").disabled = true;
    document.documentElement.getButton("cancel").disabled = true;
    document.documentElement.getButton("finish").focus();
    
    gUpdates.headerVisible = true;
  }
};

var gFinishedPage = {
  onPageShow: function() {
    document.documentElement.getButton("back").disabled = true;
    var finishButton = document.documentElement.getButton("finish");
    finishButton.label = gUpdates.updateStrings.getString("restartButton");
    finishButton.focus();
    var cancelButton = document.documentElement.getButton("cancel");
    cancelButton.label = gUpdates.updateStrings.getString("laterButton");
    
    var self = this;
    function onRestartLater() {
      // self.onRestartLater();
      self._later = true;
    }
    cancelButton.addEventListener("command", onRestartLater, false);
  },
  
  onClose: function() {
    LOG("FINISH = " + this._later);
  },
  
  onRestartLater: function() {
    var ps = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                       .getService(Components.interfaces.nsIPromptService);
    var brandName = gUpdates.brandStrings.getString("brandShortName");
    var message = gUpdates.updateStrings.getFormattedString("restartLaterMessage",
      [brandName]);
    ps.alert(window, gUpdates.updateStrings.getString("restartLaterTitle"), 
             message);
  }
};

