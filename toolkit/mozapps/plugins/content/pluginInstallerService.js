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
 * The Original Code is Plugin Finder Service.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the IBM Corporation are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Doron Rosenberg <doronr@us.ibm.com>
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

const nsIXPIProgressDialog = Components.interfaces.nsIXPIProgressDialog;

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

      this._fireNotification(nsIXPIProgressDialog.DOWNLOAD_START, null);

      channel.asyncOpen(this._downloader, null);
    }
    catch (e) {
      Components.utils.reportError(e);
      this._fireNotification(nsIXPIProgressDialog.INSTALL_DONE,
                             getLocalizedError("error-228"));
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

    if (aStatus == nsIXPIProgressDialog.INSTALL_DONE) {
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
      this._fireNotification(nsIXPIProgressDialog.INSTALL_DONE,
                             getLocalizedError("error-228"));
      result.remove(false);
      return;
    }

    this._fireNotification(nsIXPIProgressDialog.DOWNLOAD_DONE);

    if (this._plugin.InstallerHash &&
        !verifyHash(result, this._plugin.InstallerHash)) {
      // xpinstall error 261 is "Invalid file hash..."
      this._fireNotification(nsIXPIProgressDialog.INSTALL_DONE,
                             getLocalizedError("error-261"));
      result.remove(false);
      return;
    }

    this._fireNotification(nsIXPIProgressDialog.INSTALL_START);

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
            self._fireNotification(nsIXPIProgressDialog.INSTALL_DONE,
                                   getLocalizedError("error-207"));
          }
          else if (process.exitValue != 0) {
            Components.utils.reportError("Installer returned exit code " + process.exitValue);
            self._fireNotification(nsIXPIProgressDialog.INSTALL_DONE,
                                   getLocalizedError("error-203"));
          }
          else {
            self._fireNotification(nsIXPIProgressDialog.INSTALL_DONE, null);
          }
          result.remove(false);
        }
      });
    }
    catch (e) {
      Components.utils.reportError(e);
      this._fireNotification(nsIXPIProgressDialog.INSTALL_DONE,
                             getLocalizedError("error-207"));
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

    if (this._xpiPlugins.length > 0) {
      this._xpisDone = false;

      var xpiManager = Components.classes["@mozilla.org/xpinstall/install-manager;1"]
                                 .createInstance(Components.interfaces.nsIXPInstallManager);
      xpiManager.initManagerWithHashes(
        [plugin.XPILocation for each (plugin in this._xpiPlugins)],
        [plugin.XPIHash for each (plugin in this._xpiPlugins)],
        this._xpiPlugins.length, this);
    }
    else {
      this._xpisDone = true;
    }

    // InstallerObserver may finish immediately so we must initialise the
    // installers after setting the number of installers and xpis pending
    this._installersPending = aInstallerPlugins.length;
    this._installerPlugins = [new InstallerObserver(plugin)
                              for each (plugin in aInstallerPlugins)];
  },

  _fireFinishedNotification: function()
  {
    if (this._installersPending == 0 && this._xpisDone)
      gPluginInstaller.
        pluginInstallationProgress(null, nsIXPIProgressDialog.DIALOG_CLOSE,
                                   null);
  },

  // XPI progress listener stuff
  onStateChange: function (aIndex, aState, aValue)
  {
    // get the pid to return to the wizard
    var pid = this._xpiPlugins[aIndex].pid;
    var errorMsg;

    if (aState == nsIXPIProgressDialog.INSTALL_DONE) {
      if (aValue != 0) {
        var xpinstallStrings = document.getElementById("xpinstallStrings");
        try {
          errorMsg = xpinstallStrings.getString("error" + aValue);
        }
        catch (e) {
          errorMsg = xpinstallStrings.getFormattedString("unknown.error", [aValue]);
        }
      }
    }

    if (aState == nsIXPIProgressDialog.DIALOG_CLOSE) {
      this._xpisDone = true;
      this._fireFinishedNotification();
    }
    else {
      gPluginInstaller.pluginInstallationProgress(pid, aState, errorMsg);
    }
  },

  onProgress: function (aIndex, aValue, aMaxValue)
  {
    // get the pid to return to the wizard
    var pid = this._xpiPlugins[aIndex].pid;
    gPluginInstaller.pluginInstallationProgressMeter(pid, aValue, aMaxValue);
  }
}
