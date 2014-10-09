// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

const APK_MIME_TYPE = "application/vnd.android.package-archive";
const PREF_BD_USEDOWNLOADDIR = "browser.download.useDownloadDir";
const URI_GENERIC_ICON_DOWNLOAD = "drawable://alert_download";

Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/HelperApps.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// -----------------------------------------------------------------------
// HelperApp Launcher Dialog
// -----------------------------------------------------------------------

function HelperAppLauncherDialog() { }

HelperAppLauncherDialog.prototype = {
  classID: Components.ID("{e9d277a0-268a-4ec2-bb8c-10fdf3e44611}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIHelperAppLauncherDialog]),

  getNativeWindow: function () {
    try {
      let win = Services.wm.getMostRecentWindow("navigator:browser");
      if (win && win.NativeWindow) {
        return win.NativeWindow;
      }
    } catch (e) {
    }
    return null;
  },

  /**
   * Returns false if `url` represents a local or special URL that we don't
   * wish to ever download.
   *
   * Returns true otherwise.
   */
  _canDownload: function (url, alreadyResolved=false) {
    // The common case.
    if (url.schemeIs("http") ||
        url.schemeIs("https") ||
        url.schemeIs("ftp")) {
      return true;
    }

    // The less-common opposite case.
    if (url.schemeIs("chrome") ||
        url.schemeIs("jar") ||
        url.schemeIs("resource") ||
        url.schemeIs("wyciwyg")) {
      return false;
    }

    // For all other URIs, try to resolve them to an inner URI, and check that.
    if (!alreadyResolved) {
      let ioSvc = Cc["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
      let innerURI = ioSvc.newChannelFromURI(url).URI;
      if (!url.equals(innerURI)) {
        return this._canDownload(innerURI, true);
      }
    }

    if (url.schemeIs("file")) {
      // If it's in our app directory or profile directory, we never ever
      // want to do anything with it, including saving to disk or passing the
      // file to another application.
      let file = url.QueryInterface(Ci.nsIFileURL).file;

      // Normalize the nsILocalFile in-place. This will ensure that paths
      // can be correctly compared via `contains`, below.
      file.normalize();

      // TODO: pref blacklist?

      let appRoot = FileUtils.getFile("XREExeF", []);
      if (appRoot.contains(file, true)) {
        return false;
      }

      let profileRoot = FileUtils.getFile("ProfD", []);
      if (profileRoot.contains(file, true)) {
        return false;
      }

      return true;
    }

    // Anything else is fine to download.
    return true;
  },

  /**
   * Returns true if `launcher` represents a download for which we wish
   * to prompt.
   */
  _shouldPrompt: function (launcher) {
    let mimeType = this._getMimeTypeFromLauncher(launcher);

    // Straight equality: nsIMIMEInfo normalizes.
    return APK_MIME_TYPE == mimeType;
  },

  show: function hald_show(aLauncher, aContext, aReason) {
    if (!this._canDownload(aLauncher.source)) {
      aLauncher.cancel(Cr.NS_BINDING_ABORTED);

      let win = this.getNativeWindow();
      if (!win) {
        // Oops.
        Services.console.logStringMessage("Refusing download, but can't show a toast.");
        return;
      }

      Services.console.logStringMessage("Refusing download of non-downloadable file.");
      let bundle = Services.strings.createBundle("chrome://browser/locale/handling.properties");
      let failedText = bundle.GetStringFromName("download.blocked");
      win.toast.show(failedText, "long");

      return;
    }

    let bundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");

    let defaultHandler = new Object();
    let apps = HelperApps.getAppsForUri(aLauncher.source, {
      mimeType: aLauncher.MIMEInfo.MIMEType,
    });

    // Add a fake intent for save to disk at the top of the list.
    apps.unshift({
      name: bundle.GetStringFromName("helperapps.saveToDisk"),
      packageName: "org.mozilla.gecko.Download",
      iconUri: "drawable://icon",
      selected: true, // Default to download for files
      launch: function() {
        // Reset the preferredAction here.
        aLauncher.MIMEInfo.preferredAction = Ci.nsIMIMEInfo.saveToDisk;
        aLauncher.saveToDisk(null, false);
        return true;
      }
    });

    // See if the user already marked something as the default for this mimetype,
    // and if that app is still installed.
    let preferredApp = this._getPreferredApp(aLauncher);
    if (preferredApp) {
      let pref = apps.filter(function(app) {
        return app.packageName === preferredApp;
      });

      if (pref.length > 0) {
        pref[0].launch(aLauncher.source);
        return;
      }
    }

    let callback = function(app) {
      aLauncher.MIMEInfo.preferredAction = Ci.nsIMIMEInfo.useHelperApp;
      if (!app.launch(aLauncher.source)) {
        aLauncher.cancel(Cr.NS_BINDING_ABORTED);
      }
    }

    // If there's only one choice, and we don't want to prompt, go right ahead
    // and choose that app automatically.
    if (!this._shouldPrompt(aLauncher) && (apps.length === 1)) {
      callback(apps[0]);
      return;
    }

    // Otherwise, let's go through the prompt.
    HelperApps.prompt(apps, {
      title: bundle.GetStringFromName("helperapps.pick"),
      buttons: [
        bundle.GetStringFromName("helperapps.alwaysUse"),
        bundle.GetStringFromName("helperapps.useJustOnce")
      ]
    }, (data) => {
      if (data.button < 0) {
        return;
      }

      callback(apps[data.icongrid0]);

      if (data.button === 0) {
        this._setPreferredApp(aLauncher, apps[data.icongrid0]);
      }
    });
  },

  _getPrefName: function getPrefName(mimetype) {
    return "browser.download.preferred." + mimetype.replace("\\", ".");
  },

  _getMimeTypeFromLauncher: function (launcher) {
    let mime = launcher.MIMEInfo.MIMEType;
    if (!mime)
      mime = ContentAreaUtils.getMIMETypeForURI(launcher.source) || "";
    return mime;
  },

  _getPreferredApp: function getPreferredApp(launcher) {
    let mime = this._getMimeTypeFromLauncher(launcher);
    if (!mime)
      return;

    try {
      return Services.prefs.getCharPref(this._getPrefName(mime));
    } catch(ex) {
      Services.console.logStringMessage("Error getting pref for " + mime + ".");
    }
    return null;
  },

  _setPreferredApp: function setPreferredApp(launcher, app) {
    let mime = this._getMimeTypeFromLauncher(launcher);
    if (!mime)
      return;

    if (app)
      Services.prefs.setCharPref(this._getPrefName(mime), app.packageName);
    else
      Services.prefs.clearUserPref(this._getPrefName(mime));
  },

  promptForSaveToFile: function hald_promptForSaveToFile(aLauncher, aContext, aDefaultFile, aSuggestedFileExt, aForcePrompt) {
    // Retrieve the user's default download directory
    let dnldMgr = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
    let defaultFolder = dnldMgr.userDownloadsDirectory;

    try {
      file = this.validateLeafName(defaultFolder, aDefaultFile, aSuggestedFileExt);
    } catch (e) { }

    return file;
  },

  validateLeafName: function hald_validateLeafName(aLocalFile, aLeafName, aFileExt) {
    if (!(aLocalFile && this.isUsableDirectory(aLocalFile)))
      return null;

    // Remove any leading periods, since we don't want to save hidden files
    // automatically.
    aLeafName = aLeafName.replace(/^\.+/, "");

    if (aLeafName == "")
      aLeafName = "unnamed" + (aFileExt ? "." + aFileExt : "");
    aLocalFile.append(aLeafName);

    this.makeFileUnique(aLocalFile);
    return aLocalFile;
  },

  makeFileUnique: function hald_makeFileUnique(aLocalFile) {
    try {
      // Note - this code is identical to that in
      //   toolkit/content/contentAreaUtils.js.
      // If you are updating this code, update that code too! We can't share code
      // here since this is called in a js component.
      let collisionCount = 0;
      while (aLocalFile.exists()) {
        collisionCount++;
        if (collisionCount == 1) {
          // Append "(2)" before the last dot in (or at the end of) the filename
          // special case .ext.gz etc files so we don't wind up with .tar(2).gz
          if (aLocalFile.leafName.match(/\.[^\.]{1,3}\.(gz|bz2|Z)$/i))
            aLocalFile.leafName = aLocalFile.leafName.replace(/\.[^\.]{1,3}\.(gz|bz2|Z)$/i, "(2)$&");
          else
            aLocalFile.leafName = aLocalFile.leafName.replace(/(\.[^\.]*)?$/, "(2)$&");
        }
        else {
          // replace the last (n) in the filename with (n+1)
          aLocalFile.leafName = aLocalFile.leafName.replace(/^(.*\()\d+\)/, "$1" + (collisionCount+1) + ")");
        }
      }
      aLocalFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0600);
    }
    catch (e) {
      dump("*** exception in validateLeafName: " + e + "\n");

      if (e.result == Cr.NS_ERROR_FILE_ACCESS_DENIED)
        throw e;

      if (aLocalFile.leafName == "" || aLocalFile.isDirectory()) {
        aLocalFile.append("unnamed");
        if (aLocalFile.exists())
          aLocalFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0600);
      }
    }
  },

  isUsableDirectory: function hald_isUsableDirectory(aDirectory) {
    return aDirectory.exists() && aDirectory.isDirectory() && aDirectory.isWritable();
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([HelperAppLauncherDialog]);
