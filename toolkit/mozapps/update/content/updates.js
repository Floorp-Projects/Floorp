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

const nsIUpdateItem = Components.interfaces.nsIUpdateItem;
const nsIIncrementalDownload = Components.interfaces.nsIIncrementalDownload;
const XMLNS_XUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const PREF_UPDATE_MANUAL_URL = "app.update.manual.url";

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
    if (window.arguments && window.arguments[0]) {
      this.update = window.arguments[0];
      document.documentElement.advance();
    }
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
    
    var updateStrings = document.getElementById("updateStrings");
    var pageTitle = updateStrings.getString("errorsPageHeader");
    
    var errorPage = document.getElementById("errors");
    errorPage.setAttribute("label", pageTitle);
    document.documentElement.setAttribute("label", pageTitle);
    document.documentElement.advance();
  },
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

var gUpdatesAvailablePage = {
  _incompatibleItems: null,
  
  onPageShow: function() {
    var updateStrings = document.getElementById("updateStrings");
    var brandStrings = document.getElementById("brandStrings");
    var brandName = brandStrings.getString("brandShortName");
    var updateName = updateStrings.getFormattedString("updateName", 
                                                      [brandName, gUpdates.update.version]);
    var updateNameElement = document.getElementById("updateName");
    updateNameElement.value = updateName;
    var displayType = updateStrings.getString("updateType_" + gUpdates.update.type);
    var updateTypeElement = document.getElementById("updateType");
    updateTypeElement.setAttribute("type", gUpdates.update.type);
    var intro = updateStrings.getFormattedString("introType_" + gUpdates.update.type, [brandName]);
    while (updateTypeElement.hasChildNodes())
      updateTypeElement.removeChild(updateTypeElement.firstChild);
    updateTypeElement.appendChild(document.createTextNode(intro));
    
    var updateMoreInfoURL = document.getElementById("updateMoreInfoURL");
    updateMoreInfoURL.href = gUpdates.update.detailsurl;
    
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
  },
  
  showIncompatibleItems: function() {
    openDialog("chrome://mozapps/content/update/incompatible.xul", "", 
               "dialog,centerscreen,modal,resizable,titlebar", this._incompatibleItems);
  }
};

var gDownloadingPage = {
  onPageShow: function() {
    // Build the UI for the active download
    var update = document.createElementNS(XMLNS_XUL, "update");
    update.setAttribute("state", "downloading");
    update.setAttribute("name", "Firefox 1.0.4");
    update.setAttribute("status", "Blah");
    update.setAttribute("url", "http://www.bengoodger.com/");
    update.id = "activeDownloadItem";
    var updatesView = document.getElementById("updatesView");
    updatesView.appendChild(update);
    
    updatesView.addEventListener("update-pause", this.onPause, false);
    
    // Add this UI as a listener for active downloads
    var updates = 
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);
    var state = updates.downloadUpdate(gUpdates.update, false);
    if (state == "failed") 
      this.showVerificationError();
    else
      updates.addDownloadListener(this);
    
    // Build the UI for previously installed updates
    // ...
    
  },
  
  _paused: false,
  onPause: function() {
    var updates = 
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);
    if (this._paused)
      updates.downloadUpdate(gUpdates.update, false);
    else
      updates.pauseDownload();
  },
  
  onClose: function() {
    // Remove ourself as a download listener so that we don't continue to be 
    // fed progress and state notifications after the UI we're updating has 
    // gone away.
    var updates = 
        Components.classes["@mozilla.org/updates/update-service;1"]
                  .getService(Components.interfaces.nsIApplicationUpdateService);
    updates.removeDownloadListener(this);
  },
  
  showCompletedUpdatesChanged: function(checkbox) {
    var updatesView = document.getElementById("updatesView");
    updatesView.setAttribute("showcompletedupdates", checkbox.checked);
  },

  onStartRequest: function(request, context) {
    request.QueryInterface(nsIIncrementalDownload);
    LOG("gDownloadingPage.onStartRequest: " + request.URI.spec);
  },
  
  onProgress: function(request, context, progress, maxProgress) {
    request.QueryInterface(nsIIncrementalDownload);
    //LOG("gDownloadingPage.onProgress: " + request.URI.spec + ", " + progress + "/" + maxProgress);
    
    var active = document.getElementById("activeDownloadItem");
    active.setAttribute("progress", Math.floor(100 * (progress/maxProgress)));
  },
  
  onStatus: function(request, context, status, statusText) {
    request.QueryInterface(nsIIncrementalDownload);
    LOG("gDownloadingPage.onStatus: " + request.URI.spec + " status = " + status + ", text = " + statusText);
  },
  
  onStopRequest: function(request, context, status) {
    request.QueryInterface(nsIIncrementalDownload);
    LOG("gDownloadingPage.onStopRequest: " + request.URI.spec + ", status = " + status);
    
    const NS_BINDING_ABORTED = 0x804b0002;
    var updates = 
        Components.classes["@mozilla.org/updates/update-service;1"]
                  .getService(Components.interfaces.nsIApplicationUpdateService);
    if (status == Components.results.NS_ERROR_UNEXPECTED &&
        !gUpdates.update.isCompleteUpdate) {
      // If we were downloading a patch and the patch verification phase 
      // failed, log this and then commence downloading the complete update.
      LOG("Verification of patch failed, downloading complete update");
      gUpdates.update.isCompleteUpdate = true;
      var state = updates.downloadUpdate(gUpdates.update, false);
      if (state == "failed") 
        this.showVerificationError();
      else
        return;
    }
    else if (status == NS_BINDING_ABORTED) {
      LOG("Download PAUSED");
    }
    updates.removeDownloadListener(this);
  },
  
  showVerificationError: function() {
    var updateStrings = document.getElementById("updateStrings");
    var brandStrings = document.getElementById("brandStrings");
    var brandName = brandStrings.getString("brandShortName");
    var verificationError = updateStrings.getFormattedString("verificationError", [brandName]);
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
  }
};

