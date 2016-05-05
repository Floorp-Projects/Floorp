// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*globals ContentAreaUtils */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

const APK_MIME_TYPE = "application/vnd.android.package-archive";

const OMA_DOWNLOAD_DESCRIPTOR_MIME_TYPE = "application/vnd.oma.dd+xml";
const OMA_DRM_MESSAGE_MIME = "application/vnd.oma.drm.message";
const OMA_DRM_CONTENT_MIME = "application/vnd.oma.drm.content";
const OMA_DRM_RIGHTS_MIME = "application/vnd.oma.drm.rights+wbxml";

const PREF_BD_USEDOWNLOADDIR = "browser.download.useDownloadDir";
const URI_GENERIC_ICON_DOWNLOAD = "drawable://alert_download";

Cu.import("resource://gre/modules/Downloads.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/HelperApps.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "RuntimePermissions", "resource://gre/modules/RuntimePermissions.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Messaging", "resource://gre/modules/Messaging.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Snackbars", "resource://gre/modules/Snackbars.jsm");

// -----------------------------------------------------------------------
// HelperApp Launcher Dialog
// -----------------------------------------------------------------------

XPCOMUtils.defineLazyGetter(this, "ContentAreaUtils", function() {
  let ContentAreaUtils = {};
  Services.scriptloader.loadSubScript("chrome://global/content/contentAreaUtils.js", ContentAreaUtils);
  return ContentAreaUtils;
});

function HelperAppLauncherDialog() { }

HelperAppLauncherDialog.prototype = {
  classID: Components.ID("{e9d277a0-268a-4ec2-bb8c-10fdf3e44611}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIHelperAppLauncherDialog]),

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
        url.schemeIs("wyciwyg") ||
        url.schemeIs("file")) {
      return false;
    }

    // For all other URIs, try to resolve them to an inner URI, and check that.
    if (!alreadyResolved) {
      let innerURI = NetUtil.newChannel({
        uri: url,
        loadUsingSystemPrincipal: true
      }).URI;

      if (!url.equals(innerURI)) {
        return this._canDownload(innerURI, true);
      }
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
    return APK_MIME_TYPE == mimeType || OMA_DOWNLOAD_DESCRIPTOR_MIME_TYPE == mimeType;
  },

  /**
   * Returns true if `launcher` represents a download for which we wish to
   * offer a "Save to disk" option.
   */
  _shouldAddSaveToDiskIntent: function(launcher) {
      let mimeType = this._getMimeTypeFromLauncher(launcher);

      // We can't handle OMA downloads. So don't even try. (Bug 1219078)
      return mimeType != OMA_DOWNLOAD_DESCRIPTOR_MIME_TYPE;
  },

  /**
   * Returns true if `launcher`represents a download that should not be handled by Firefox
   * or a third-party app and instead be forwarded to Android's download manager.
   */
  _shouldForwardToAndroidDownloadManager: function(aLauncher) {
    let forwardDownload = Services.prefs.getBoolPref('browser.download.forward_oma_android_download_manager');
    if (!forwardDownload) {
      return false;
    }

    let mimeType = aLauncher.MIMEInfo.MIMEType;
    if (!mimeType) {
      mimeType = ContentAreaUtils.getMIMETypeForURI(aLauncher.source) || "";
    }

    return [
      OMA_DOWNLOAD_DESCRIPTOR_MIME_TYPE,
      OMA_DRM_MESSAGE_MIME,
      OMA_DRM_CONTENT_MIME,
      OMA_DRM_RIGHTS_MIME
    ].indexOf(mimeType) != -1;
  },

  show: function hald_show(aLauncher, aContext, aReason) {
    if (!this._canDownload(aLauncher.source)) {
      this._refuseDownload(aLauncher);
      return;
    }

    if (this._shouldForwardToAndroidDownloadManager(aLauncher)) {
      this._downloadWithAndroidDownloadManager(aLauncher);
      aLauncher.cancel(Cr.NS_BINDING_ABORTED);
      return;
    }

    let bundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");

    let defaultHandler = new Object();
    let apps = HelperApps.getAppsForUri(aLauncher.source, {
      mimeType: aLauncher.MIMEInfo.MIMEType,
    });

    if (this._shouldAddSaveToDiskIntent(aLauncher)) {
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
    }

    // We do not handle this download and there are no apps that want to do it
    if (apps.length === 0) {
      this._refuseDownload(aLauncher);
      return;
    }

    let callback = function(app) {
      aLauncher.MIMEInfo.preferredAction = Ci.nsIMIMEInfo.useHelperApp;
      if (!app.launch(aLauncher.source)) {
        // Once the app is done we need to get rid of the temp file. This shouldn't
        // get run in the saveToDisk case.
        aLauncher.cancel(Cr.NS_BINDING_ABORTED);
      }
    }

    // See if the user already marked something as the default for this mimetype,
    // and if that app is still installed.
    let preferredApp = this._getPreferredApp(aLauncher);
    if (preferredApp) {
      let pref = apps.filter(function(app) {
        return app.packageName === preferredApp;
      });

      if (pref.length > 0) {
        callback(pref[0]);
        return;
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
      ],
      // Tapping an app twice should choose "Just once".
      doubleTapButton: 1
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

  _refuseDownload: function(aLauncher) {
    aLauncher.cancel(Cr.NS_BINDING_ABORTED);

    Services.console.logStringMessage("Refusing download of non-downloadable file.");

    let bundle = Services.strings.createBundle("chrome://browser/locale/handling.properties");
    let failedText = bundle.GetStringFromName("download.blocked");

    Snackbars.show(failedText, Snackbars.LENGTH_LONG);
  },

  _downloadWithAndroidDownloadManager(aLauncher) {
    let mimeType = aLauncher.MIMEInfo.MIMEType;
    if (!mimeType) {
      mimeType = ContentAreaUtils.getMIMETypeForURI(aLauncher.source) || "";
    }

    Messaging.sendRequest({
      'type': 'Download:AndroidDownloadManager',
      'uri': aLauncher.source.spec,
      'mimeType': mimeType,
      'filename': aLauncher.suggestedFileName
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

  promptForSaveToFileAsync: function (aLauncher, aContext, aDefaultFile,
                                      aSuggestedFileExt, aForcePrompt) {
    Task.spawn(function* () {
      let file = null;
      try {
        let hasPermission = yield RuntimePermissions.waitForPermissions(RuntimePermissions.WRITE_EXTERNAL_STORAGE);
        if (hasPermission) {
          // If we do have the STORAGE permission then pick the public downloads directory as destination
          // for this file. Without the permission saveDestinationAvailable(null) will be called which
          // will effectively cancel the download.
          let preferredDir = yield Downloads.getPreferredDownloadsDirectory();
          file = this.validateLeafName(new FileUtils.File(preferredDir),
                                       aDefaultFile, aSuggestedFileExt);
        }
      } finally {
        // The file argument will be null in case any exception occurred.
        aLauncher.saveDestinationAvailable(file);
      }
    }.bind(this)).catch(Cu.reportError);
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
      aLocalFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);
    }
    catch (e) {
      dump("*** exception in validateLeafName: " + e + "\n");

      if (e.result == Cr.NS_ERROR_FILE_ACCESS_DENIED)
        throw e;

      if (aLocalFile.leafName == "" || aLocalFile.isDirectory()) {
        aLocalFile.append("unnamed");
        if (aLocalFile.exists())
          aLocalFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);
      }
    }
  },

  isUsableDirectory: function hald_isUsableDirectory(aDirectory) {
    return aDirectory.exists() && aDirectory.isDirectory() && aDirectory.isWritable();
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([HelperAppLauncherDialog]);
