/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["WebappRT"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
  "resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS",
  "resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, 'NativeApp',
  'resource://gre/modules/NativeApp.jsm');

XPCOMUtils.defineLazyServiceGetter(this, "appsService",
                                  "@mozilla.org/AppsService;1",
                                  "nsIAppsService");

this.WebappRT = {
  get launchURI() {
    return this.localeManifest.fullLaunchPath();
  },

  get localeManifest() {
    return new ManifestHelper(this.config.app.manifest,
                              this.config.app.origin);
  },

  get appID() {
    let manifestURL = WebappRT.config.app.manifestURL;
    if (!manifestURL) {
      return Ci.nsIScriptSecurityManager.NO_APP_ID;
    }

    return appsService.getAppLocalIdByManifestURL(manifestURL);
  },

  loadConfig: function() {
    if (this.config) {
      return;
    }

    let webappJson = OS.Path.join(Services.dirsvc.get("AppRegD", Ci.nsIFile).path,
                                  "webapp.json");
    this.config = yield AppsUtils.loadJSONAsync(webappJson);
  },

  isUpdatePending: Task.async(function*() {
    let webappJson = OS.Path.join(Services.dirsvc.get("AppRegD", Ci.nsIFile).path,
                                  "update", "webapp.json");

    if (!(yield OS.File.exists(webappJson))) {
      return false;
    }

    return true;
  }),

  applyUpdate: Task.async(function*() {
    let webappJson = OS.Path.join(Services.dirsvc.get("AppRegD", Ci.nsIFile).path,
                                  "update", "webapp.json");
    let config = yield AppsUtils.loadJSONAsync(webappJson);

    let nativeApp = new NativeApp(config.app, config.app.manifest,
                                  config.app.categories,
                                  config.registryDir);
    try {
      yield nativeApp.applyUpdate(config.app);
    } catch (ex) {
      return false;
    }

    // The update has been applied successfully, the new config file
    // is the config file that was in the update directory.
    this.config = config;

    return true;
  }),

  startUpdateService: function() {
    let manifestURL = WebappRT.config.app.manifestURL;
    // We used to install apps without storing their manifest URL.
    // Now we can't update them.
    if (!manifestURL) {
      return;
    }

    // Check for updates once a day.
    let timerManager = Cc["@mozilla.org/updates/timer-manager;1"].
                       getService(Ci.nsIUpdateTimerManager);
    timerManager.registerTimer("updateTimer", () => {
      let window = Services.wm.getMostRecentWindow("webapprt:webapp");
      window.navigator.mozApps.mgmt.getAll().onsuccess = function() {
        let thisApp = null;
        for (let app of this.result) {
          if (app.manifestURL == manifestURL) {
            thisApp = app;
            break;
          }
        }

        // This shouldn't happen if the app is installed.
        if (!thisApp) {
          Cu.reportError("Couldn't find the app in the webapps registry");
          return;
        }

        thisApp.ondownloadavailable = () => {
          // Download available, download it!
          thisApp.download();
        };

        thisApp.ondownloadsuccess = () => {
          // Update downloaded, apply it!
          window.navigator.mozApps.mgmt.applyDownload(thisApp);
        };

        thisApp.ondownloadapplied = () => {
          // Application updated, nothing to do.
        };

        thisApp.checkForUpdate();
      }
    }, 24 * 60 * 60);
  },
};
