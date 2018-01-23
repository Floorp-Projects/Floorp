// -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals LoadContextInfo, FormHistory, Accounts */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Accounts: "resource://gre/modules/Accounts.jsm",
  DownloadIntegration: "resource://gre/modules/DownloadIntegration.jsm",
  Downloads: "resource://gre/modules/Downloads.jsm",
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  FormHistory: "resource://gre/modules/FormHistory.jsm",
  LoadContextInfo: "resource://gre/modules/LoadContextInfo.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Task: "resource://gre/modules/Task.jsm",
  TelemetryStopwatch: "resource://gre/modules/TelemetryStopwatch.jsm",
});

function dump(a) {
  Services.console.logStringMessage(a);
}

this.EXPORTED_SYMBOLS = ["Sanitizer"];

function Sanitizer() {}
Sanitizer.prototype = {
  clearItem: function(aItemName, startTime) {
    // Only a subset of items support deletion with startTime.
    // Those who do not will be rejected with error message.
    if (typeof startTime != "undefined") {
      switch (aItemName) {
        // Normal call to DownloadFiles remove actual data from storage, but our web-extension consumer
        // deletes only download history. So, for this reason we are passing a flag 'deleteFiles'.
        case "downloadHistory":
          return this._clear("downloadFiles", { startTime, deleteFiles: false });
        case "formdata":
          return this._clear(aItemName, { startTime });
        default:
          return Promise.reject({message: `Invalid argument: ${aItemName} does not support startTime argument.`});
      }
    } else {
      return this._clear(aItemName);
    }
  },

 _clear: function(aItemName, options) {
    let item = this.items[aItemName];
    let canClear = item.canClear;
    if (typeof canClear == "function") {
      let maybeDoClear = async () => {
        let canClearResult = await new Promise(resolve => {
          canClear(resolve);
        });

        if (canClearResult) {
          return item.clear(options);
        }
      };
      return maybeDoClear();
    } else if (canClear) {
      return item.clear(options);
    }
  },

  items: {
    cache: {
      clear: function() {
        return new Promise(function(resolve, reject) {
          let refObj = {};
          TelemetryStopwatch.start("FX_SANITIZE_CACHE", refObj);

          var cache = Cc["@mozilla.org/netwerk/cache-storage-service;1"].getService(Ci.nsICacheStorageService);
          try {
            cache.clear();
          } catch (er) {}

          let imageCache = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools)
                                                           .getImgCacheForDocument(null);
          try {
            imageCache.clearCache(false); // true=chrome, false=content
          } catch (er) {}

          TelemetryStopwatch.finish("FX_SANITIZE_CACHE", refObj);
          resolve();
        });
      },

      get canClear() {
        return true;
      }
    },

    cookies: {
      clear: function() {
        return new Promise(function(resolve, reject) {
          let refObj = {};
          TelemetryStopwatch.start("FX_SANITIZE_COOKIES_2", refObj);

          Services.cookies.removeAll();

          TelemetryStopwatch.finish("FX_SANITIZE_COOKIES_2", refObj);
          resolve();
        });
      },

      get canClear() {
        return true;
      }
    },

    siteSettings: {
      clear: Task.async(function* () {
        let refObj = {};
        TelemetryStopwatch.start("FX_SANITIZE_SITESETTINGS", refObj);

        // Clear site-specific permissions like "Allow this site to open popups"
        Services.perms.removeAll();

        // Clear site-specific settings like page-zoom level
        Cc["@mozilla.org/content-pref/service;1"]
          .getService(Ci.nsIContentPrefService2)
          .removeAllDomains(null);

        // Clear site security settings
        var sss = Cc["@mozilla.org/ssservice;1"]
                    .getService(Ci.nsISiteSecurityService);
        sss.clearAll();

        // Clear push subscriptions
        yield new Promise((resolve, reject) => {
          let push = Cc["@mozilla.org/push/Service;1"]
                       .getService(Ci.nsIPushService);
          push.clearForDomain("*", status => {
            if (Components.isSuccessCode(status)) {
              resolve();
            } else {
              reject(new Error("Error clearing push subscriptions: " +
                               status));
            }
          });
        });
        TelemetryStopwatch.finish("FX_SANITIZE_SITESETTINGS", refObj);
      }),

      get canClear() {
        return true;
      }
    },

    offlineApps: {
      clear: function() {
        return new Promise(function(resolve, reject) {
          var cacheService = Cc["@mozilla.org/netwerk/cache-storage-service;1"].getService(Ci.nsICacheStorageService);
          var appCacheStorage = cacheService.appCacheStorage(LoadContextInfo.default, null);
          try {
            appCacheStorage.asyncEvictStorage(null);
          } catch (er) {}

          resolve();
        });
      },

      get canClear() {
          return true;
      }
    },

    history: {
      clear: function() {
        let refObj = {};
        TelemetryStopwatch.start("FX_SANITIZE_HISTORY", refObj);

        return EventDispatcher.instance.sendRequestForResult({ type: "Sanitize:ClearHistory" })
          .catch(e => Cu.reportError("Java-side history clearing failed: " + e))
          .then(function() {
            TelemetryStopwatch.finish("FX_SANITIZE_HISTORY", refObj);
            try {
              Services.obs.notifyObservers(null, "browser:purge-session-history");
            } catch (e) { }

            try {
              var predictor = Cc["@mozilla.org/network/predictor;1"].getService(Ci.nsINetworkPredictor);
              predictor.reset();
            } catch (e) { }
          });
      },

      get canClear() {
        // bug 347231: Always allow clearing history due to dependencies on
        // the browser:purge-session-history notification. (like error console)
        return true;
      }
    },

    openTabs: {
      clear: function() {
        let refObj = {};
        TelemetryStopwatch.start("FX_SANITIZE_OPENWINDOWS", refObj);

        return EventDispatcher.instance.sendRequestForResult({ type: "Sanitize:OpenTabs" })
          .catch(e => Cu.reportError("Java-side tab clearing failed: " + e))
          .then(function() {
            try {
              // clear "Recently Closed" tabs in Android App
              Services.obs.notifyObservers(null, "browser:purge-session-tabs");
            } catch (e) { }
            TelemetryStopwatch.finish("FX_SANITIZE_OPENWINDOWS", refObj);
          });
      },

      get canClear() {
        return true;
      }
    },

    searchHistory: {
      clear: function() {
        return EventDispatcher.instance.sendRequestForResult({ type: "Sanitize:ClearHistory", clearSearchHistory: true })
          .catch(e => Cu.reportError("Java-side search history clearing failed: " + e));
      },

      get canClear() {
        return true;
      }
    },

    formdata: {
      clear: function({ startTime = 0 } = {}) {
        return new Promise(function(resolve, reject) {
          let refObj = {};
          TelemetryStopwatch.start("FX_SANITIZE_FORMDATA", refObj);

          // Conver time to microseconds
          let time = startTime * 1000;
          FormHistory.update({
            op: "remove",
            firstUsedStart: time
          }, {
            handleCompletion() {
              TelemetryStopwatch.finish("FX_SANITIZE_FORMDATA", refObj);
              resolve();
            }
          });
        });
      },

      canClear: function(aCallback) {
        let count = 0;
        let countDone = {
          handleResult: function(aResult) { count = aResult; },
          handleError: function(aError) { Cu.reportError(aError); },
          handleCompletion: function(aReason) { aCallback(aReason == 0 && count > 0); }
        };
        FormHistory.count({}, countDone);
      }
    },

    downloadFiles: {
      clear: Task.async(function* ({ startTime = 0, deleteFiles = true} = {}) {
        let refObj = {};
        TelemetryStopwatch.start("FX_SANITIZE_DOWNLOADS", refObj);

        let list = yield Downloads.getList(Downloads.ALL);
        let downloads = yield list.getAll();
        var finalizePromises = [];

        // Logic copied from DownloadList.removeFinished. Ideally, we would
        // just use that method directly, but we want to be able to remove the
        // downloaded files as well.
        for (let download of downloads) {
          // Remove downloads that have been canceled, even if the cancellation
          // operation hasn't completed yet so we don't check "stopped" here.
          // Failed downloads with partial data are also removed. The startTime
          // check is provided for addons that may want to delete only recent downloads.
          if (download.stopped && (!download.hasPartialData || download.error) &&
              download.startTime.getTime() >= startTime) {
            // Remove the download first, so that the views don't get the change
            // notifications that may occur during finalization.
            yield list.remove(download);
            // Ensure that the download is stopped and no partial data is kept.
            // This works even if the download state has changed meanwhile.  We
            // don't need to wait for the procedure to be complete before
            // processing the other downloads in the list.
            finalizePromises.push(download.finalize(true).then(() => null, Cu.reportError));

            if (deleteFiles) {
              // Delete the downloaded files themselves.
              OS.File.remove(download.target.path).then(() => null, ex => {
                if (!(ex instanceof OS.File.Error && ex.becauseNoSuchFile)) {
                  Cu.reportError(ex);
                }
              });
            }
          }
        }

        yield Promise.all(finalizePromises);
        yield DownloadIntegration.forceSave();
        TelemetryStopwatch.finish("FX_SANITIZE_DOWNLOADS", refObj);
      }),

      get canClear() {
        return true;
      }
    },

    passwords: {
      clear: function() {
        return new Promise(function(resolve, reject) {
          Services.logins.removeAllLogins();
          resolve();
        });
      },

      get canClear() {
        let count = Services.logins.countLogins("", "", ""); // count all logins
        return (count > 0);
      }
    },

    sessions: {
      clear: function() {
        return new Promise(function(resolve, reject) {
          let refObj = {};
          TelemetryStopwatch.start("FX_SANITIZE_SESSIONS", refObj);

          // clear all auth tokens
          var sdr = Cc["@mozilla.org/security/sdr;1"].getService(Ci.nsISecretDecoderRing);
          sdr.logoutAndTeardown();

          // clear FTP and plain HTTP auth sessions
          Services.obs.notifyObservers(null, "net:clear-active-logins");

          TelemetryStopwatch.finish("FX_SANITIZE_SESSIONS", refObj);
          resolve();
        });
      },

      get canClear() {
        return true;
      }
    },

    syncedTabs: {
      clear: function() {
        return EventDispatcher.instance.sendRequestForResult({ type: "Sanitize:ClearSyncedTabs" })
          .catch(e => Cu.reportError("Java-side synced tabs clearing failed: " + e));
      },

      canClear: function(aCallback) {
        Accounts.anySyncAccountsExist().then(aCallback)
          .catch(function(err) {
            Cu.reportError("Java-side synced tabs clearing failed: " + err);
            aCallback(false);
          });
      }
    }

  }
};

this.Sanitizer = new Sanitizer();
