/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["WebappManager"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");
Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

function dump(a) {
  Services.console.logStringMessage("* * WebappManager.jsm: " + a);
}

function sendMessageToJava(aMessage) {
  return Services.androidBridge.handleGeckoMessage(JSON.stringify(aMessage));
}

this.WebappManager = {
  __proto__: DOMRequestIpcHelper.prototype,

  downloadApk: function(aMsg) {
    let manifestUrl = aMsg.app.manifestURL;
    dump("downloadApk for " + manifestUrl);

    // Get the endpoint URL and convert it to an nsIURI/nsIURL object.
    const GENERATOR_URL_PREF = "browser.webapps.apkFactoryUrl";
    const GENERATOR_URL_BASE = Services.prefs.getCharPref(GENERATOR_URL_PREF);
    let generatorUrl = NetUtil.newURI(GENERATOR_URL_BASE).QueryInterface(Ci.nsIURL);

    // Populate the query part of the URL with the manifest URL parameter.
    let params = {
      manifestUrl: manifestUrl,
    };
    generatorUrl.query =
      [p + "=" + encodeURIComponent(params[p]) for (p in params)].join("&");
    dump("downloading APK from " + generatorUrl.spec);

    let file = Cc["@mozilla.org/download-manager;1"].
               getService(Ci.nsIDownloadManager).
               defaultDownloadsDirectory.
               clone();
    file.append(manifestUrl.replace(/[^a-zA-Z0-9]/gi, "") + ".apk");
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
    dump("downloading APK to " + file.path);

    let worker = new ChromeWorker("resource://gre/modules/WebappManagerWorker.js");
    worker.onmessage = function(event) {
      let { type, message } = event.data;

      worker.terminate();

      if (type == "success") {
        sendMessageToJava({
          type: "WebApps:InstallApk",
          filePath: file.path,
          data: JSON.stringify(aMsg),
        });
      } else { // type == "failure"
        // TODO: handle error better.
        dump("error downloading APK: " + message);
      }
    }

    // Trigger the download.
    worker.postMessage({ url: generatorUrl.spec, path: file.path });
  },

  askInstall: function(aData) {
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
    file.initWithPath(aData.profilePath);

    // We don't yet support pre-installing an appcache because it isn't clear
    // how to do it without degrading the user experience (since users expect
    // apps to be available after the system tells them they've been installed,
    // which has already happened) and because nsCacheService shuts down
    // when we trigger the native install dialog and doesn't re-init itself
    // afterward (TODO: file bug about this behavior).
    if ("appcache_path" in aData.app.manifest) {
      dump("deleting appcache_path from manifest: " + aData.app.manifest.appcache_path);
      delete aData.app.manifest.appcache_path;
    }

    DOMApplicationRegistry.confirmInstall(aData, file, (function(aManifest) {
      let localeManifest = new ManifestHelper(aManifest, aData.app.origin);

      // aData.app.origin may now point to the app: url that hosts this app.
      sendMessageToJava({
        type: "WebApps:PostInstall",
        packageName: aData.app.packageName,
        origin: aData.app.origin,
      });

      this.writeDefaultPrefs(file, localeManifest);
    }).bind(this));
  },

  launch: function({ manifestURL, origin }) {
    dump("launchWebapp: " + manifestURL);

    sendMessageToJava({
      type: "WebApps:Open",
      manifestURL: manifestURL,
      origin: origin
    });
  },

  uninstall: function(aData) {
    dump("uninstall: " + aData.manifestURL);

    // TODO: uninstall the APK.
  },

  autoInstall: function(aData) {
    let mm = {
      sendAsyncMessage: function (aMessageName, aData) {
        // TODO hook this back to Java to report errors.
        dump("sendAsyncMessage " + aMessageName + ": " + JSON.stringify(aData));
      }
    };

    let origin = Services.io.newURI(aData.manifestUrl, null, null).prePath;

    let message = aData.request || {
      app: {
        origin: origin
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
    message.app.manifestURL = aData.manifestUrl;
    message.app.manifest = aData.manifest;
    message.app.packageName = aData.packageName;
    message.profilePath = aData.profilePath;
    message.autoInstall = true;
    message.mm = mm;

    switch (aData.type) { // can be hosted or packaged.
      case "hosted":
        DOMApplicationRegistry.doInstall(message, mm);
        break;

      case "packaged":
        message.isPackage = true;
        DOMApplicationRegistry.doInstallPackage(message, mm);
        break;
    }
  },

  autoUninstall: function(aData) {
    let mm = {
      sendAsyncMessage: function (aMessageName, aData) {
        // TODO hook this back to Java to report errors.
        dump("autoUninstall sendAsyncMessage " + aMessageName + ": " + JSON.stringify(aData));
      }
    };
    let installedPackages = {};
    DOMApplicationRegistry.doGetAll(installedPackages, mm);

    for (let app in installedPackages.apps) {
      if (aData.packages.indexOf(installedPackages.apps[app].packageName) > -1) {
        let appToRemove = installedPackages.apps[app];
        dump("should remove: " + appToRemove.name);
        DOMApplicationRegistry.uninstall(appToRemove.manifestURL, function() {
          dump(appToRemove.name + " uninstalled");
        }, function() {
          dump(appToRemove.name + " did not uninstall");
        });
      }
    }
  },

  writeDefaultPrefs: function(aProfile, aManifest) {
      // build any app specific default prefs
      let prefs = [];
      if (aManifest.orientation) {
        prefs.push({name:"app.orientation.default", value: aManifest.orientation.join(",") });
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
        dump("Error writing default prefs: " + reason);
      });
    }
  },

  DEFAULT_PREFS_FILENAME: "default-prefs.js",

};
