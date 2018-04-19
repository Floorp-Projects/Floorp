/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals LoadContextInfo, FormHistory, Accounts */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Accounts: "resource://gre/modules/Accounts.jsm",
  DownloadIntegration: "resource://gre/modules/DownloadIntegration.jsm",
  Downloads: "resource://gre/modules/Downloads.jsm",
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  FormHistory: "resource://gre/modules/FormHistory.jsm",
  OfflineAppCacheHelper: "resource://gre/modules/offlineAppCache.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  ServiceWorkerCleanUp: "resource://gre/modules/ServiceWorkerCleanUp.jsm",
  Task: "resource://gre/modules/Task.jsm",
  TelemetryStopwatch: "resource://gre/modules/TelemetryStopwatch.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  quotaManagerService: ["@mozilla.org/dom/quota-manager-service;1", "nsIQuotaManagerService"],
});


var EXPORTED_SYMBOLS = ["Sanitizer"];

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

  // This code is mostly based on the Sanitizer code for desktop Firefox
  // (browser/modules/Sanitzer.jsm), however over the course of time some
  // general differences have evolved:
  // - async shutdown (and seenException handling) isn't implemented in Fennec
  // - currently there is only limited support for range-based clearing of data

  // Any further specific differences caused by architectural differences between
  // Fennec and desktop Firefox are documented below for each item.
  items: {
    // Same as desktop Firefox.
    cache: {
      clear: function() {
        return new Promise(function(resolve, reject) {
          let refObj = {};
          TelemetryStopwatch.start("FX_SANITIZE_CACHE", refObj);

          try {
            Services.cache2.clear();
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

    // Compared to desktop, we don't clear plugin data, as plugins
    // aren't supported on Android.
    cookies: {
      clear: function() {
        return new Promise(function(resolve, reject) {
          let refObj = {};
          TelemetryStopwatch.start("FX_SANITIZE_COOKIES_2", refObj);

          Services.cookies.removeAll();

          TelemetryStopwatch.finish("FX_SANITIZE_COOKIES_2", refObj);

          // Clear deviceIds. Done asynchronously (returns before complete).
          try {
            let mediaMgr = Cc["@mozilla.org/mediaManagerService;1"]
                             .getService(Ci.nsIMediaManagerService);
            mediaMgr.sanitizeDeviceIds(0);
          } catch (er) { }

          resolve();
        });
      },

      get canClear() {
        return true;
      }
    },

    // Same as desktop Firefox.
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

    // Same as desktop Firefox.
    offlineApps: {
      async clear() {
        // AppCache
        // This doesn't wait for the cleanup to be complete.
        OfflineAppCacheHelper.clear();

        // LocalStorage
        Services.obs.notifyObservers(null, "extension:purge-localStorage");

        // ServiceWorkers
        await ServiceWorkerCleanUp.removeAll();

        // QuotaManager
        let promises = [];
        await new Promise(resolve => {
          quotaManagerService.getUsage(request => {
            if (request.resultCode != Cr.NS_OK) {
              // We are probably shutting down. We don't want to propagate the
              // error, rejecting the promise.
              resolve();
              return;
            }

            for (let item of request.result) {
              let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(item.origin);
              let uri = principal.URI;
              if (uri.scheme == "http" || uri.scheme == "https" || uri.scheme == "file") {
                promises.push(new Promise(r => {
                  let req = quotaManagerService.clearStoragesForPrincipal(principal, null, false);
                  req.callback = () => { r(); };
                }));
              }
            }
            resolve();
          });
        });

        return Promise.all(promises);
      },

      get canClear() {
          return true;
      }
    },

    // History on Android is implemented by the Java frontend and requires
    // different handling. Everything else is the same as for desktop Firefox.
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

    // Equivalent to openWindows on desktop, but specific to Fennec's implementation
    // of tabbed browsing and the session store.
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

    // Specific to Fennec.
    searchHistory: {
      clear: function() {
        return EventDispatcher.instance.sendRequestForResult({ type: "Sanitize:ClearHistory", clearSearchHistory: true })
          .catch(e => Cu.reportError("Java-side search history clearing failed: " + e));
      },

      get canClear() {
        return true;
      }
    },

    // Browser search is handled by searchHistory above and the find bar doesn't
    // require extra handling. FormHistory itself is cleared like on desktop.
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

    // Adapted from desktop, but heavily modified - see comments below.
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

    // Specific to Fennec.
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

    // Same as desktop Firefox.
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

    // Specific to Fennec.
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

var Sanitizer = new Sanitizer();
