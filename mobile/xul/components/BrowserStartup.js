/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const ADDONS_NOTIFICATION_NAME = "addons";

// Custom factory object to ensure that we're a singleton
const BrowserStartupServiceFactory = {
  _instance: null,
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return this._instance || (this._instance = new BrowserStartup());
  }
};

function BrowserStartup() {
  this._init();
}

BrowserStartup.prototype = {
  // for XPCOM
  classID: Components.ID("{1d542abc-c88b-4636-a4ef-075b49806317}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  _xpcom_factory: BrowserStartupServiceFactory,

  _init: function() {
    Services.obs.addObserver(this, "places-init-complete", false);
    Services.obs.addObserver(this, "final-ui-startup", false);
  },

  _initDefaultBookmarks: function() {
    // We must instantiate the history service since it will tell us if we
    // need to import or restore bookmarks due to first-run, corruption or
    // forced migration (due to a major schema change).
    let histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                  getService(Ci.nsINavHistoryService);

    // If the database is corrupt or has been newly created we should
    // import bookmarks.
    let databaseStatus = histsvc.databaseStatus;
    let importBookmarks = databaseStatus == histsvc.DATABASE_STATUS_CREATE ||
                          databaseStatus == histsvc.DATABASE_STATUS_CORRUPT;

    if (!importBookmarks) {
      // Check to see whether "mobile" root already exists. This is to handle
      // existing profiles created with pre-1.0 builds (which won't have mobile
      // bookmarks root). We can remove this eventually when we stop
      // caring about users migrating to current builds with pre-1.0 profiles.
      let annos = Cc["@mozilla.org/browser/annotation-service;1"].
                  getService(Ci.nsIAnnotationService);
      let mobileRootItems = annos.getItemsWithAnnotation("mobile/bookmarksRoot", {});
      if (mobileRootItems.length > 0)
        return; // no need to do initial import
    }

    Cu.import("resource://gre/modules/PlacesUtils.jsm");

    try {
      let observer = {
        onStreamComplete : function(aLoader, aContext, aStatus, aLength, aResult) {
          let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                          createInstance(Ci.nsIScriptableUnicodeConverter);
          let jsonStr = "";
          try {
            converter.charset = "UTF-8";
            jsonStr = converter.convertFromByteArray(aResult, aResult.length);

            // aReplace=false since this may be called when there are existing
            // bookmarks that we don't want to overwrite ("no mobile root"
            // case from above)
            PlacesUtils.restoreBookmarksFromJSONString(jsonStr, false);
          } catch (err) {
            Cu.reportError("Failed to parse default bookmarks from bookmarks.json: " + err);
          }
        }
      };

      let ioSvc = Cc["@mozilla.org/network/io-service;1"].
                  getService(Ci.nsIIOService);
      let uri = ioSvc.newURI("chrome://browser/locale/bookmarks.json", null, null);
      let channel = ioSvc.newChannelFromURI(uri);
      let sl = Cc["@mozilla.org/network/stream-loader;1"].
               createInstance(Ci.nsIStreamLoader);
      sl.init(observer);
      channel.asyncOpen(sl, channel);
    } catch (err) {
      // Report the error, but ignore it.
      Cu.reportError("Failed to load default bookmarks from bookmarks.json: " + err);
    }
  },

  _startupActions: function() {
#ifdef ANDROID
    // Hide the notification if we had any pending operations
    try {
      if (Services.prefs.getBoolPref("browser.notifications.pending.addons")) {
        Services.prefs.clearUserPref("browser.notifications.pending.addons")
        let alertsService = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
        let progressListener = alertsService.QueryInterface(Ci.nsIAlertsProgressListener);
        if (progressListener)
          progressListener.onCancel(ADDONS_NOTIFICATION_NAME);
      }
    } catch (e) {}
#endif
  },

  // nsIObserver
  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "places-init-complete":
        Services.obs.removeObserver(this, "places-init-complete");
        this._initDefaultBookmarks();
        break;
      case "final-ui-startup":
        Services.obs.removeObserver(this, "final-ui-startup");
        this._startupActions();
        break;
    }
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([BrowserStartup]);
