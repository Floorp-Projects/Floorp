const nsIUpdateItem = Components.interfaces.nsIUpdateItem;
const nsIIncrementalDownload = Components.interfaces.nsIIncrementalDownload;

/**
 * Logs a string to the error console. 
 * @param   string
 *          The string to write to the error console..
 */  
function LOG(string) {
  dump("*** " + string + "\n");
}

var gUpdates = {
  updates: null,
  onClose: function() {
    var objects = {
      checking: gCheckingPage,
      updatesfound: gUpdatesAvailablePage
    };
    var pageid = document.documentElement.currentPage.pageid;
    if ("onClose" in objects[pageid]) 
      objects[pageid].onClose();
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
      gUpdates.updates = updates;
      document.documentElement.advance();
    },

    /**
     * See nsIUpdateCheckListener.idl
     */
    onError: function() {
      dump("*** UpdateCheckListener: ERROR\n");
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
    var newestUpdate = gUpdates.updates[0];
    var vc = Components.classes["@mozilla.org/updates/version-checker;1"]
                       .createInstance(Components.interfaces.nsIVersionChecker);
    for (var i = 0; i < gUpdates.updates.length; ++i) {
      if (vc.compare(gUpdates.updates[i].version, newestUpdate.version) > 0)
        newestVersion = gUpdates.updates[i];
    }
    
    var updateStrings = document.getElementById("updateStrings");
    var brandStrings = document.getElementById("brandStrings");
    var brandName = brandStrings.getString("brandShortName");
    var updateName = updateStrings.getFormattedString("updateName", 
                                                      [brandName, newestVersion.version]);
    var updateNameElement = document.getElementById("updateName");
    updateNameElement.value = updateName;
    
    var displayType = updateStrings.getString("updateType_" + newestVersion.type);
    var updateTypeElement = document.getElementById("updateType");
    updateTypeElement.setAttribute("type", newestVersion.type);
    var intro = updateStrings.getFormattedString("introType_" + newestVersion.type, [brandName]);
    while (updateTypeElement.hasChildNodes())
      updateTypeElement.removeChild(updateTypeElement.firstChild);
    updateTypeElement.appendChild(document.createTextNode(intro));
    
    var updateMoreInfoURL = document.getElementById("updateMoreInfoURL");
    updateMoreInfoURL.href = newestVersion.detailsurl;
    
    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                       .getService(Components.interfaces.nsIExtensionManager);
    var items = em.getIncompatibleItemList("", newestVersion.version,
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
    var update = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",  
                                          "update");
    update.setAttribute("state", "downloading");
    update.setAttribute("name", "Firefox 1.0.4");
    update.setAttribute("status", "Blah");
    update.setAttribute("url", "http://www.bengoodger.com/");
    update.id = "activeDownloadItem";
    var updatesView = document.getElementById("updatesView");
    updatesView.appendChild(update);
    
    // Add this UI as a listener for active downloads
    var updates = 
        Components.classes["@mozilla.org/updates/update-service;1"]
                  .getService(Components.interfaces.nsIApplicationUpdateService);
    for (var i = 0; i < gUpdates.updates.length; ++i) 
      updates.downloadUpdate(gUpdates.updates[i]);
    updates.addDownloadListener(this);
    
    // Build the UI for previously installed updates
    // ...
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
    LOG("gDownloadingPage.onProgress: " + request.URI.spec + ", " + progress + "/" + maxProgress);
    
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

