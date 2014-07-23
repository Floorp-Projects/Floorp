/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["WebappManager"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const UPDATE_URL_PREF = "browser.webapps.updateCheckUrl";

Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");
Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Notifications", "resource://gre/modules/Notifications.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "sendMessageToJava", "resource://gre/modules/Messaging.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PluralForm", "resource://gre/modules/PluralForm.jsm");

XPCOMUtils.defineLazyGetter(this, "Strings", function() {
  return Services.strings.createBundle("chrome://browser/locale/webapp.properties");
});

/**
 * Get the formatted plural form of a string.  Escapes semicolons in arguments
 * to provide to the formatter before formatting the string, then unescapes them
 * after getting its plural form, to avoid tripping up the plural form getter
 * with a semicolon in one of the formatter's arguments, since the plural forms
 * of localized strings are delimited by semicolons.
 *
 * Ideally, we'd get the plural form first and then format the string,
 * so we wouldn't have to escape/unescape the semicolons; but that would require
 * changes to nsIStringBundle and PluralForm.jsm.
 *
 * @param stringName {String} the string to get the formatted plural form of
 * @param formatterArgs {Array} of {String} args to provide to the formatter
 * @param pluralNum {Number} the number that determines the plural form
 * @returns {String} the formatted plural form of the string
 */
function getFormattedPluralForm(stringName, formatterArgs, pluralNum) {
  // Escape semicolons by replacing them with ESC characters.
  let escapedArgs = [arg.replace(/;/g, String.fromCharCode(0x1B)) for (arg of formatterArgs)];
  let formattedString = Strings.formatStringFromName(stringName, escapedArgs, escapedArgs.length);
  let pluralForm = PluralForm.get(pluralNum, formattedString);
  let unescapedString = pluralForm.replace(String.fromCharCode(0x1B), ";", "g");
  return unescapedString;
}

let Log = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog;
let debug = Log.d.bind(null, "WebappManager");

this.WebappManager = {
  __proto__: DOMRequestIpcHelper.prototype,

  get _testing() {
    try {
      return Services.prefs.getBoolPref("browser.webapps.testing");
    } catch(ex) {
      return false;
    }
  },

  install: function(aMessage, aMessageManager) {
    if (this._testing) {
      // Go directly to DOM.  Do not download/install APK, do not collect $200.
      DOMApplicationRegistry.doInstall(aMessage, aMessageManager);
      return;
    }

    this._installApk(aMessage, aMessageManager);
  },

  installPackage: function(aMessage, aMessageManager) {
    if (this._testing) {
      // Go directly to DOM.  Do not download/install APK, do not collect $200.
      DOMApplicationRegistry.doInstallPackage(aMessage, aMessageManager);
      return;
    }

    this._installApk(aMessage, aMessageManager);
  },

  _installApk: function(aMessage, aMessageManager) { return Task.spawn((function*() {
    let filePath;

    try {
      filePath = yield this._downloadApk(aMessage.app.manifestURL);
    } catch(ex) {
      aMessage.error = ex;
      aMessageManager.sendAsyncMessage("Webapps:Install:Return:KO", aMessage);
      debug("error downloading APK: " + ex);
      return;
    }

    sendMessageToJava({
      type: "Webapps:InstallApk",
      filePath: filePath,
      data: aMessage,
    }, (data, error) => {
      if (!!error) {
        aMessage.error = error;
        aMessageManager.sendAsyncMessage("Webapps:Install:Return:KO", aMessage);
        debug("error downloading APK: " + error);
      }
    });
  }).bind(this)); },

  _downloadApk: function(aManifestUrl) {
    debug("_downloadApk for " + aManifestUrl);
    let deferred = Promise.defer();

    // Get the endpoint URL and convert it to an nsIURI/nsIURL object.
    const GENERATOR_URL_PREF = "browser.webapps.apkFactoryUrl";
    const GENERATOR_URL_BASE = Services.prefs.getCharPref(GENERATOR_URL_PREF);
    let generatorUrl = NetUtil.newURI(GENERATOR_URL_BASE).QueryInterface(Ci.nsIURL);

    // Populate the query part of the URL with the manifest URL parameter.
    let params = {
      manifestUrl: aManifestUrl,
    };
    generatorUrl.query =
      [p + "=" + encodeURIComponent(params[p]) for (p in params)].join("&");
    debug("downloading APK from " + generatorUrl.spec);

    let file = Cc["@mozilla.org/download-manager;1"].
               getService(Ci.nsIDownloadManager).
               defaultDownloadsDirectory.
               clone();
    file.append(aManifestUrl.replace(/[^a-zA-Z0-9]/gi, "") + ".apk");
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
    debug("downloading APK to " + file.path);

    let worker = new ChromeWorker("resource://gre/modules/WebappManagerWorker.js");
    worker.onmessage = function(event) {
      let { type, message } = event.data;

      worker.terminate();

      if (type == "success") {
        deferred.resolve(file.path);
      } else { // type == "failure"
        debug("error downloading APK: " + message);
        deferred.reject(message);
      }
    }

    // Trigger the download.
    worker.postMessage({ url: generatorUrl.spec, path: file.path });

    return deferred.promise;
  },

  _deleteAppcachePath: function(aManifest) {
    // We don't yet support pre-installing an appcache because it isn't clear
    // how to do it without degrading the user experience (since users expect
    // apps to be available after the system tells them they've been installed,
    // which has already happened) and because nsCacheService shuts down
    // when we trigger the native install dialog and doesn't re-init itself
    // afterward (TODO: file bug about this behavior).
    if ("appcache_path" in aManifest) {
      debug("deleting appcache_path from manifest: " + aManifest.appcache_path);
      delete aManifest.appcache_path;
    }
  },

  askInstall: function(aData) {
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
    file.initWithPath(aData.profilePath);

    this._deleteAppcachePath(aData.app.manifest);

    DOMApplicationRegistry.registryReady.then(() => {
      DOMApplicationRegistry.confirmInstall(aData, file, (function(aApp, aManifest) {
        this._postInstall(aData.profilePath, aManifest, aData.app.origin, aData.app.apkPackageName);
      }).bind(this));
    });
  },

  _postInstall: function(aProfilePath, aNewManifest, aOrigin, aApkPackageName) {
    // aOrigin may now point to the app: url that hosts this app.
    sendMessageToJava({
      type: "Webapps:Postinstall",
      apkPackageName: aApkPackageName,
      origin: aOrigin,
    });

    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
    file.initWithPath(aProfilePath);
    let localeManifest = new ManifestHelper(aNewManifest, aOrigin);
    this.writeDefaultPrefs(file, localeManifest);
  },

  launch: function({ apkPackageName }) {
    debug("launch: " + apkPackageName);

    sendMessageToJava({
      type: "Webapps:Launch",
      packageName: apkPackageName,
    });
  },

  uninstall: Task.async(function*(aData, aMessageManager) {
    debug("uninstall: " + aData.manifestURL);

    yield DOMApplicationRegistry.registryReady;

    if (this._testing) {
      // Go directly to DOM.  Do not uninstall APK, do not collect $200.
      DOMApplicationRegistry.doUninstall(aData, aMessageManager);
      return;
    }

    let app = DOMApplicationRegistry.getAppByManifestURL(aData.manifestURL);
    if (!app) {
      throw new Error("app not found in registry");
    }

    // If the APK is installed, then _getAPKVersions will return a version
    // for it, so we can use that function to determine its install status.
    let apkVersions = yield this._getAPKVersions([ app.apkPackageName ]);
    if (app.apkPackageName in apkVersions) {
      debug("APK is installed; requesting uninstallation");
      sendMessageToJava({
        type: "Webapps:UninstallApk",
        apkPackageName: app.apkPackageName,
      });

      // We don't need to call DOMApplicationRegistry.doUninstall at this point,
      // because the APK uninstall listener will call autoUninstall once the APK
      // is uninstalled; and if the user cancels the APK uninstallation, then we
      // shouldn't remove the app from the registry anyway.

      // But we should tell the requesting document the result of their request.
      // TODO: tell the requesting document if uninstallation succeeds or fails
      // by storing weak references to the message/manager pair here and then
      // using them in autoUninstall if they're still defined when it's called;
      // and make EventListener.uninstallApk return an error when APK uninstall
      // fails (which it should be able to detect reliably on Android 4+),
      // which we observe here and use to notify the requester of failure.
    } else {
      // The APK isn't installed, but remove the app from the registry anyway,
      // to ensure the user can always remove an app from the registry (and thus
      // about:apps) even if it's out of sync with installed APKs.
      debug("APK not installed; proceeding directly to removal from registry");
      DOMApplicationRegistry.doUninstall(aData, aMessageManager);
    }

  }),

  autoInstall: function(aData) {
    debug("autoInstall " + aData.manifestURL);

    // If the app is already installed, update the existing installation.
    // We should be able to use DOMApplicationRegistry.getAppByManifestURL,
    // but it returns a mozIApplication, while _autoUpdate needs the original
    // object from DOMApplicationRegistry.webapps in order to modify it.
    for (let [ , app] in Iterator(DOMApplicationRegistry.webapps)) {
      if (app.manifestURL == aData.manifestURL) {
        return this._autoUpdate(aData, app);
      }
    }

    let mm = {
      sendAsyncMessage: function (aMessageName, aData) {
        // TODO hook this back to Java to report errors.
        debug("sendAsyncMessage " + aMessageName + ": " + JSON.stringify(aData));
      }
    };

    let origin = Services.io.newURI(aData.manifestURL, null, null).prePath;

    let message = aData.request || {
      app: {
        origin: origin,
        receipts: [],
      }
    };

    if (aData.updateManifest) {
      if (aData.zipFilePath) {
        aData.updateManifest.package_path = aData.zipFilePath;
      }
      message.app.updateManifest = aData.updateManifest;
    }

    // The manifest url may be subtly different between the
    // time the APK was built and the APK being installed.
    // Thus, we should take the APK as the source of truth.
    message.app.manifestURL = aData.manifestURL;
    message.app.manifest = aData.manifest;
    message.app.apkPackageName = aData.apkPackageName;
    message.profilePath = aData.profilePath;
    message.mm = mm;
    message.apkInstall = true;

    DOMApplicationRegistry.registryReady.then(() => {
      switch (aData.type) { // can be hosted or packaged.
        case "hosted":
          DOMApplicationRegistry.doInstall(message, mm);
          break;

        case "packaged":
          message.isPackage = true;
          DOMApplicationRegistry.doInstallPackage(message, mm);
          break;
      }
    });
  },

  _autoUpdate: function(aData, aOldApp) { return Task.spawn((function*() {
    debug("_autoUpdate app of type " + aData.type);

    if (aOldApp.apkPackageName != aData.apkPackageName) {
      // This happens when the app was installed as a shortcut via the old
      // runtime and is now being updated to an APK.
      debug("update apkPackageName from " + aOldApp.apkPackageName + " to " + aData.apkPackageName);
      aOldApp.apkPackageName = aData.apkPackageName;
    }

    if (aData.type == "hosted") {
      this._deleteAppcachePath(aData.manifest);
      let oldManifest = yield DOMApplicationRegistry.getManifestFor(aData.manifestURL);
      yield DOMApplicationRegistry.updateHostedApp(aData, aOldApp.id, aOldApp, oldManifest, aData.manifest);
    } else {
      yield this._autoUpdatePackagedApp(aData, aOldApp);
    }

    this._postInstall(aData.profilePath, aData.manifest, aOldApp.origin, aOldApp.apkPackageName);
  }).bind(this)); },

  _autoUpdatePackagedApp: Task.async(function*(aData, aOldApp) {
    debug("_autoUpdatePackagedApp: " + aData.manifestURL);

    if (aData.updateManifest && aData.zipFilePath) {
      aData.updateManifest.package_path = aData.zipFilePath;
    }

    // updatePackagedApp just prepares the update, after which we must
    // download the package via the misnamed startDownload and then apply it
    // via applyDownload.
    yield DOMApplicationRegistry.updatePackagedApp(aData, aOldApp.id, aOldApp, aData.updateManifest);

    try {
      yield DOMApplicationRegistry.startDownload(aData.manifestURL);
    } catch (ex if ex.message == "PACKAGE_UNCHANGED") {
      debug("package unchanged");
      // If the package is unchanged, then there's nothing more to do.
      return;
    }

    yield DOMApplicationRegistry.applyDownload(aData.manifestURL);
  }),

  _checkingForUpdates: false,

  checkForUpdates: function(userInitiated) { return Task.spawn((function*() {
    debug("checkForUpdates");

    // Don't start checking for updates if we're already doing so.
    // TODO: Consider cancelling the old one and starting a new one anyway
    // if the user requested this one.
    if (this._checkingForUpdates) {
      debug("already checking for updates");
      return;
    }
    this._checkingForUpdates = true;

    try {
      let installedApps = yield this._getInstalledApps();
      if (installedApps.length === 0) {
        return;
      }

      // Map APK names to APK versions.
      let apkNameToVersion = yield this._getAPKVersions(installedApps.map(app =>
        app.apkPackageName).filter(apkPackageName => !!apkPackageName)
      );

      // Map manifest URLs to APK versions, which is what the service needs
      // in order to tell us which apps are outdated; and also map them to app
      // objects, which the downloader/installer uses to download/install APKs.
      // XXX Will this cause us to update apps without packages, and if so,
      // does that satisfy the legacy migration story?
      let manifestUrlToApkVersion = {};
      let manifestUrlToApp = {};
      for (let app of installedApps) {
        manifestUrlToApkVersion[app.manifestURL] = apkNameToVersion[app.apkPackageName] || 0;
        manifestUrlToApp[app.manifestURL] = app;
      }

      let outdatedApps = yield this._getOutdatedApps(manifestUrlToApkVersion, userInitiated);

      if (outdatedApps.length === 0) {
        // If the user asked us to check for updates, tell 'em we came up empty.
        if (userInitiated) {
          this._notify({
            title: Strings.GetStringFromName("noUpdatesTitle"),
            message: Strings.GetStringFromName("noUpdatesMessage"),
            icon: "drawable://alert_app",
          });
        }
        return;
      }

      let usingLan = function() {
        let network = Cc["@mozilla.org/network/network-link-service;1"].getService(Ci.nsINetworkLinkService);
        return (network.linkType == network.LINK_TYPE_WIFI || network.linkType == network.LINK_TYPE_ETHERNET);
      };

      let updateAllowed = function() {
        let autoUpdatePref = Services.prefs.getCharPref("app.update.autodownload");

        return (autoUpdatePref == "enabled") || (autoUpdatePref == "wifi" && usingLan());
      };

      if (updateAllowed()) {
        yield this._updateApks([manifestUrlToApp[url] for (url of outdatedApps)]);
      } else {
        let names = [manifestUrlToApp[url].name for (url of outdatedApps)].join(", ");
        let accepted = yield this._notify({
          title: PluralForm.get(outdatedApps.length, Strings.GetStringFromName("retrieveUpdateTitle")).
                 replace("#1", outdatedApps.length),
          message: getFormattedPluralForm("retrieveUpdateMessage", [names], outdatedApps.length),
          icon: "drawable://alert_app",
        }).dismissed;

        if (accepted) {
          yield this._updateApks([manifestUrlToApp[url] for (url of outdatedApps)]);
        }
      }
    }
    // There isn't a catch block because we want the error to propagate through
    // the promise chain, so callers can receive it and choose to respond to it.
    finally {
      // Ensure we update the _checkingForUpdates flag even if there's an error;
      // otherwise the process will get stuck and never check for updates again.
      this._checkingForUpdates = false;
    }
  }).bind(this)); },

  _getAPKVersions: function(packageNames) {
    let deferred = Promise.defer();

    sendMessageToJava({
      type: "Webapps:GetApkVersions",
      packageNames: packageNames 
    }, data => deferred.resolve(data.versions));

    return deferred.promise;
  },

  _getInstalledApps: function() {
    let deferred = Promise.defer();
    DOMApplicationRegistry.getAll(apps => deferred.resolve(apps));
    return deferred.promise;
  },

  _getOutdatedApps: function(installedApps, userInitiated) {
    let deferred = Promise.defer();

    let data = JSON.stringify({ installed: installedApps });

    let notification;
    if (userInitiated) {
      notification = this._notify({
        title: Strings.GetStringFromName("checkingForUpdatesTitle"),
        message: Strings.GetStringFromName("checkingForUpdatesMessage"),
        icon: "drawable://alert_app_animation",
        progress: NaN,
      });
    }

    let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                  createInstance(Ci.nsIXMLHttpRequest).
                  QueryInterface(Ci.nsIXMLHttpRequestEventTarget);
    request.mozBackgroundRequest = true;
    request.open("POST", Services.prefs.getCharPref(UPDATE_URL_PREF), true);
    request.channel.loadFlags = Ci.nsIChannel.LOAD_ANONYMOUS |
                                Ci.nsIChannel.LOAD_BYPASS_CACHE |
                                Ci.nsIChannel.INHIBIT_CACHING;
    request.onload = function() {
      if (userInitiated) {
        notification.cancel();
      }
      deferred.resolve(JSON.parse(this.response).outdated);
    };
    request.onerror = function() {
      if (userInitiated) {
        notification.cancel();
      }
      deferred.reject(this.status || this.statusText);
    };
    request.setRequestHeader("Content-Type", "application/json");
    request.setRequestHeader("Content-Length", data.length);

    request.send(data);

    return deferred.promise;
  },

  _updateApks: function(aApps) { return Task.spawn((function*() {
    // Notify the user that we're in the progress of downloading updates.
    let downloadingNames = [app.name for (app of aApps)].join(", ");
    let notification = this._notify({
      title: PluralForm.get(aApps.length, Strings.GetStringFromName("retrievingUpdateTitle")).
             replace("#1", aApps.length),
      message: getFormattedPluralForm("retrievingUpdateMessage", [downloadingNames], aApps.length),
      icon: "drawable://alert_download_animation",
      // TODO: make this a determinate progress indicator once we can determine
      // the sizes of the APKs and observe their progress.
      progress: NaN,
    });

    // Download the APKs for the given apps.  We do this serially to avoid
    // saturating the user's network connection.
    // TODO: download APKs in parallel (or at least more than one at a time)
    // if it seems reasonable.
    let downloadedApks = [];
    let downloadFailedApps = [];
    for (let app of aApps) {
      try {
        let filePath = yield this._downloadApk(app.manifestURL);
        downloadedApks.push({ app: app, filePath: filePath });
      } catch(ex) {
        downloadFailedApps.push(app);
      }
    }

    notification.cancel();

    // Notify the user if any downloads failed, but don't do anything
    // when the user accepts/cancels the notification.
    // In the future, we might prompt the user to retry the download.
    if (downloadFailedApps.length > 0) {
      let downloadFailedNames = [app.name for (app of downloadFailedApps)].join(", ");
      this._notify({
        title: PluralForm.get(downloadFailedApps.length, Strings.GetStringFromName("retrievalFailedTitle")).
               replace("#1", downloadFailedApps.length),
        message: getFormattedPluralForm("retrievalFailedMessage", [downloadFailedNames], downloadFailedApps.length),
        icon: "drawable://alert_app",
      });
    }

    // If we weren't able to download any APKs, then there's nothing more to do.
    if (downloadedApks.length === 0) {
      return;
    }

    // Prompt the user to update the apps for which we downloaded APKs, and wait
    // until they accept/cancel the notification.
    let downloadedNames = [apk.app.name for (apk of downloadedApks)].join(", ");
    let accepted = yield this._notify({
      title: PluralForm.get(downloadedApks.length, Strings.GetStringFromName("installUpdateTitle")).
             replace("#1", downloadedApks.length),
      message: getFormattedPluralForm("installUpdateMessage2", [downloadedNames], downloadedApks.length),
      icon: "drawable://alert_app",
    }).dismissed;

    if (accepted) {
      // The user accepted the notification, so install the downloaded APKs.
      for (let apk of downloadedApks) {
        let msg = {
          app: apk.app,
          // TODO: figure out why Webapps:InstallApk needs the "from" property.
          from: apk.app.installOrigin,
        };
        sendMessageToJava({
          type: "Webapps:InstallApk",
          filePath: apk.filePath,
          data: msg,
        }, (data, error) => {
          if (!!error) {
            // There's no page to report back to so drop the error.
            // TODO: we should notify the user about this failure.
            debug("APK install failed : " + returnError);
          }
        });
      }
    } else {
      // The user cancelled the notification, so remove the downloaded APKs.
      for (let apk of downloadedApks) {
        try {
          yield OS.file.remove(apk.filePath);
        } catch(ex) {
          debug("error removing " + apk.filePath + " for cancelled update: " + ex);
        }
      }
    }

  }).bind(this)); },

  _notify: function(aOptions) {
    dump("_notify: " + aOptions.title);

    // Resolves to true if the notification is "clicked" (i.e. touched)
    // and false if the notification is "cancelled" by swiping it away.
    let dismissed = Promise.defer();

    // TODO: make notifications expandable so users can expand them to read text
    // that gets cut off in standard notifications.
    let id = Notifications.create({
      title: aOptions.title,
      message: aOptions.message,
      icon: aOptions.icon,
      progress: aOptions.progress,
      onClick: function(aId, aCookie) {
        dismissed.resolve(true);
      },
      onCancel: function(aId, aCookie) {
        dismissed.resolve(false);
      },
    });

    // Return an object with a promise that resolves when the notification
    // is dismissed by the user along with a method for cancelling it,
    // so callers who want to wait for user action can do so, while those
    // who want to control the notification's lifecycle can do that instead.
    return {
      dismissed: dismissed.promise,
      cancel: function() {
        Notifications.cancel(id);
      },
    };
  },

  autoUninstall: function(aData) {
    DOMApplicationRegistry.registryReady.then(() => {
      for (let id in DOMApplicationRegistry.webapps) {
        let app = DOMApplicationRegistry.webapps[id];
        if (aData.apkPackageNames.indexOf(app.apkPackageName) > -1) {
          debug("attempting to uninstall " + app.name);
          DOMApplicationRegistry.uninstall(
            app.manifestURL,
            function() {
              debug("success uninstalling " + app.name);
            },
            function(error) {
              debug("error uninstalling " + app.name + ": " + error);
            }
          );
        }
      }
    });
  },

  writeDefaultPrefs: function(aProfile, aManifest) {
      // build any app specific default prefs
      let prefs = [];
      if (aManifest.orientation) {
        let orientation = aManifest.orientation;
        if (Array.isArray(orientation)) {
          orientation = orientation.join(",");
        }
        prefs.push({ name: "app.orientation.default", value: orientation });
      }

      // write them into the app profile
      let defaultPrefsFile = aProfile.clone();
      defaultPrefsFile.append(this.DEFAULT_PREFS_FILENAME);
      this._writeData(defaultPrefsFile, prefs);
  },

  _writeData: function(aFile, aPrefs) {
    if (aPrefs.length > 0) {
      let array = new TextEncoder().encode(JSON.stringify(aPrefs));
      OS.File.writeAtomic(aFile.path, array, { tmpPath: aFile.path + ".tmp" }).then(null, function onError(reason) {
        debug("Error writing default prefs: " + reason);
      });
    }
  },

  DEFAULT_PREFS_FILENAME: "default-prefs.js",

};
