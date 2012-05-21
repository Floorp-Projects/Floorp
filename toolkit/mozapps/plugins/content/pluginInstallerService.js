/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/AddonManager.jsm");

const DOWNLOAD_STARTED = 0;
const DOWNLOAD_FINISHED = 1;
const INSTALL_STARTED = 2;
const INSTALL_FINISHED = 3;
const INSTALLS_COMPLETE = 4;

function getLocalizedError(key)
{
  return document.getElementById("xpinstallStrings").getString(key);
}

function binaryToHex(input)
{
  return [('0' + input.charCodeAt(i).toString(16)).slice(-2)
          for (i in input)].join('');
}

function verifyHash(aFile, aHash)
{
  try {
    var [, method, hash] = /^([A-Za-z0-9]+):(.*)$/.exec(aHash);

    var fis = Components.classes['@mozilla.org/network/file-input-stream;1'].
      createInstance(Components.interfaces.nsIFileInputStream);
    fis.init(aFile, -1, -1, 0);

    var hasher = Components.classes['@mozilla.org/security/hash;1'].
      createInstance(Components.interfaces.nsICryptoHash);
    hasher.initWithString(method);
    hasher.updateFromStream(fis, -1);
    dlhash = binaryToHex(hasher.finish(false));
    return dlhash == hash;
  }
  catch (e) {
    Components.utils.reportError(e);
    return false;
  }
}

function InstallerObserver(aPlugin)
{
  this._plugin = aPlugin;
  this._init();
}

InstallerObserver.prototype = {
  _init: function()
  {
    try {
      var ios = Components.classes["@mozilla.org/network/io-service;1"].
        getService(Components.interfaces.nsIIOService);
      var uri = ios.newURI(this._plugin.InstallerLocation, null, null);
      uri.QueryInterface(Components.interfaces.nsIURL);

      // Use a local filename appropriate for the OS
      var leafName = uri.fileName;
      var os = Components.classes["@mozilla.org/xre/app-info;1"]
                         .getService(Components.interfaces.nsIXULRuntime)
                         .OS;
      if (os == "WINNT" && leafName.indexOf(".") < 0)
        leafName += ".exe";

      var dirs = Components.classes["@mozilla.org/file/directory_service;1"].
        getService(Components.interfaces.nsIProperties);

      var resultFile = dirs.get("TmpD", Components.interfaces.nsIFile);
      resultFile.append(leafName);
      resultFile.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE,
                              0770);

      var channel = ios.newChannelFromURI(uri);
      this._downloader =
        Components.classes["@mozilla.org/network/downloader;1"].
          createInstance(Components.interfaces.nsIDownloader);
      this._downloader.init(this, resultFile);
      channel.notificationCallbacks = this;

      this._fireNotification(DOWNLOAD_STARTED, null);

      channel.asyncOpen(this._downloader, null);
    }
    catch (e) {
      Components.utils.reportError(e);
      this._fireNotification(INSTALL_FINISHED, getLocalizedError("error-228"));
      if (resultFile && resultFile.exists())
        resultfile.remove(false);
    }
  },

  /**
   * Inform the gPluginInstaller about what's going on.
   */
  _fireNotification: function(aStatus, aErrorMsg)
  {
    gPluginInstaller.pluginInstallationProgress(this._plugin.pid,
                                                aStatus, aErrorMsg);

    if (aStatus == INSTALL_FINISHED) {
      --PluginInstallService._installersPending;
      PluginInstallService._fireFinishedNotification();
    }
  },

  QueryInterface: function(iid)
  {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIInterfaceRequestor) ||
        iid.equals(Components.interfaces.nsIDownloadObserver) ||
        iid.equals(Components.interfaces.nsIProgressEventSink))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  getInterface: function(iid)
  {
    if (iid.equals(Components.interfaces.nsIProgressEventSink))
      return this;

    return null;
  },

  onDownloadComplete: function(downloader, request, ctxt, status, result)
  {
    if (!Components.isSuccessCode(status)) {
      // xpinstall error 228 is "Download Error"
      this._fireNotification(INSTALL_FINISHED, getLocalizedError("error-228"));
      result.remove(false);
      return;
    }

    this._fireNotification(DOWNLOAD_FINISHED);

    if (this._plugin.InstallerHash &&
        !verifyHash(result, this._plugin.InstallerHash)) {
      // xpinstall error 261 is "Invalid file hash..."
      this._fireNotification(INSTALL_FINISHED, getLocalizedError("error-261"));
      result.remove(false);
      return;
    }

    this._fireNotification(INSTALL_STARTED);

    result.QueryInterface(Components.interfaces.nsILocalFile);
    try {
      // Make sure the file is executable
      result.permissions = 0770;
      var process = Components.classes["@mozilla.org/process/util;1"]
                              .createInstance(Components.interfaces.nsIProcess);
      process.init(result);
      var self = this;
      process.runAsync([], 0, {
        observe: function(subject, topic, data) {
          if (topic != "process-finished") {
            Components.utils.reportError("Failed to launch installer");
            self._fireNotification(INSTALL_FINISHED,
                                   getLocalizedError("error-207"));
          }
          else if (process.exitValue != 0) {
            Components.utils.reportError("Installer returned exit code " + process.exitValue);
            self._fireNotification(INSTALL_FINISHED,
                                   getLocalizedError("error-203"));
          }
          else {
            self._fireNotification(INSTALL_FINISHED, null);
          }
          result.remove(false);
        }
      });
    }
    catch (e) {
      Components.utils.reportError(e);
      this._fireNotification(INSTALL_FINISHED, getLocalizedError("error-207"));
      result.remove(false);
    }
  },

  onProgress: function(aRequest, aContext, aProgress, aProgressMax)
  {
    gPluginInstaller.pluginInstallationProgressMeter(this._plugin.pid,
                                                     aProgress,
                                                     aProgressMax);
  },

  onStatus: function(aRequest, aContext, aStatus, aStatusArg)
  {
    /* pass */
  }
};

var PluginInstallService = {

  /**
   * Start installation of installers and XPI plugins.
   * @param aInstallerPlugins An array of objects which should have the
   *                          properties "pid", "InstallerLocation",
   *                          and "InstallerHash"
   * @param aXPIPlugins       An array of objects which should have the
   *                          properties "pid", "XPILocation",
   *                          and "XPIHash"
   */
  startPluginInstallation: function (aInstallerPlugins,
                                     aXPIPlugins)
  {
    this._xpiPlugins = aXPIPlugins;
    this._xpisPending = aXPIPlugins.length;

    aXPIPlugins.forEach(function(plugin) {
      AddonManager.getInstallForURL(plugin.XPILocation, function(install) {
        install.addListener(PluginInstallService);
        install.install();
      }, "application/x-xpinstall", plugin.XPIHash);
    });

    // InstallerObserver may finish immediately so we must initialise the
    // installers after setting the number of installers and xpis pending
    this._installersPending = aInstallerPlugins.length;
    this._installerPlugins = [new InstallerObserver(plugin)
                              for each (plugin in aInstallerPlugins)];
  },

  _fireFinishedNotification: function()
  {
    if (this._installersPending == 0 && this._xpisPending == 0)
      gPluginInstaller.pluginInstallationProgress(null, INSTALLS_COMPLETE, null);
  },

  getPidForInstall: function(install) {
    for (let i = 0; i < this._xpiPlugins.length; i++) {
      if (install.sourceURI.spec == this._xpiPlugins[i].XPILocation)
        return this._xpiPlugins[i].pid;
    }
    return -1;
  },

  // InstallListener interface
  onDownloadStarted: function(install) {
    var pid = this.getPidForInstall(install);
    gPluginInstaller.pluginInstallationProgress(pid, DOWNLOAD_STARTED, null);
  },

  onDownloadProgress: function(install) {
    var pid = this.getPidForInstall(install);
    gPluginInstaller.pluginInstallationProgressMeter(pid, install.progress,
                                                     install.maxProgress);
  },

  onDownloadEnded: function(install) {
    var pid = this.getPidForInstall(install);
    gPluginInstaller.pluginInstallationProgress(pid, DOWNLOAD_FINISHED, null);
  },

  onDownloadFailed: function(install) {
    var pid = this.getPidForInstall(install);
    switch (install.error) {
    case AddonManager.ERROR_NETWORK_FAILURE:
      var errorMsg = getLocalizedError("error-228");
      break;
    case AddonManager.ERROR_INCORRECT_HASH:
      var errorMsg = getLocalizedError("error-261");
      break;
    case AddonManager.ERROR_CORRUPT_FILE:
      var errorMsg = getLocalizedError("error-207");
      break;
    }
    gPluginInstaller.pluginInstallationProgress(pid, INSTALL_FINISHED, errorMsg);

    this._xpisPending--;
    this._fireFinishedNotification();
  },

  onInstallStarted: function(install) {
    var pid = this.getPidForInstall(install);
    gPluginInstaller.pluginInstallationProgress(pid, INSTALL_STARTED, null);
  },

  onInstallEnded: function(install, addon) {
    var pid = this.getPidForInstall(install);
    gPluginInstaller.pluginInstallationProgress(pid, INSTALL_FINISHED, null);

    this._xpisPending--;
    this._fireFinishedNotification();
  },

  onInstallFailed: function(install) {
    var pid = this.getPidForInstall(install);
    gPluginInstaller.pluginInstallationProgress(pid, INSTALL_FINISHED,
                                                getLocalizedError("error-203"));

    this._xpisPending--;
    this._fireFinishedNotification();
  }
}
