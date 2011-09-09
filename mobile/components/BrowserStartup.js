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
 * The Original Code is Fennec Browser Startup component.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gavin Sharp <gavin@gavinsharp.com>
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
