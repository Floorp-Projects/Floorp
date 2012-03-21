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
 * The Original Code is HelperApp Launcher Dialog.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
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
const Cr = Components.results;

const PREF_BD_USEDOWNLOADDIR = "browser.download.useDownloadDir";
const URI_GENERIC_ICON_DOWNLOAD = "drawable://alertdownloads";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// -----------------------------------------------------------------------
// HelperApp Launcher Dialog
// -----------------------------------------------------------------------

function HelperAppLauncherDialog() { }

HelperAppLauncherDialog.prototype = {
  classID: Components.ID("{e9d277a0-268a-4ec2-bb8c-10fdf3e44611}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIHelperAppLauncherDialog]),

  show: function hald_show(aLauncher, aContext, aReason) {
    // Check to see if we can open this file or not
    if (aLauncher.MIMEInfo.hasDefaultHandler) {
      aLauncher.MIMEInfo.preferredAction = Ci.nsIMIMEInfo.useSystemDefault;
      aLauncher.launchWithApplication(null, false);
    } else {
      let wasClicked = false;
      let listener = {
        observe: function(aSubject, aTopic, aData) {
          if (aTopic == "alertclickcallback") {
            wasClicked = true;
            let win = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator).getMostRecentWindow("navigator:browser");
            if (win)
              win.BrowserUI.showPanel("downloads-container");
  
            aLauncher.saveToDisk(null, false);
          } else {
            if (!wasClicked)
              aLauncher.cancel(Cr.NS_BINDING_ABORTED);
          }
        }
      };
      this._notify(aLauncher, listener);
    }
  },

  promptForSaveToFile: function hald_promptForSaveToFile(aLauncher, aContext, aDefaultFile, aSuggestedFileExt, aForcePrompt) {
    let file = null;
    let prefs = Services.prefs;

    if (!aForcePrompt) {
      // Check to see if the user wishes to auto save to the default download
      // folder without prompting. Note that preference might not be set.
      let autodownload = true;
      try {
        autodownload = prefs.getBoolPref(PREF_BD_USEDOWNLOADDIR);
      } catch (e) { }

      if (autodownload) {
        // Retrieve the user's default download directory
        let dnldMgr = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
        let defaultFolder = dnldMgr.userDownloadsDirectory;

        try {
          file = this.validateLeafName(defaultFolder, aDefaultFile, aSuggestedFileExt);
        }
        catch (e) {
        }

        // Check to make sure we have a valid directory, otherwise, prompt
        if (file)
          return file;
      }
    }

    // Use file picker to show dialog.
    let picker = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    let windowTitle = "";
    let parent = aContext.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
    picker.init(parent, windowTitle, Ci.nsIFilePicker.modeSave);
    picker.defaultString = aDefaultFile;

    if (aSuggestedFileExt) {
      // aSuggestedFileExtension includes the period, so strip it
      picker.defaultExtension = aSuggestedFileExt.substring(1);
    }
    else {
      try {
        picker.defaultExtension = aLauncher.MIMEInfo.primaryExtension;
      }
      catch (e) { }
    }

    var wildCardExtension = "*";
    if (aSuggestedFileExt) {
      wildCardExtension += aSuggestedFileExt;
      picker.appendFilter(aLauncher.MIMEInfo.description, wildCardExtension);
    }

    picker.appendFilters(Ci.nsIFilePicker.filterAll);

    // Default to lastDir if it is valid, otherwise use the user's default
    // downloads directory.  userDownloadsDirectory should always return a
    // valid directory, so we can safely default to it.
    var dnldMgr = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
    picker.displayDirectory = dnldMgr.userDownloadsDirectory;

    // The last directory preference may not exist, which will throw.
    try {
      let lastDir = prefs.getComplexValue("browser.download.lastDir", Ci.nsILocalFile);
      if (isUsableDirectory(lastDir))
        picker.displayDirectory = lastDir;
    }
    catch (e) { }

    if (picker.show() == Ci.nsIFilePicker.returnCancel) {
      // null result means user cancelled.
      return null;
    }

    // Be sure to save the directory the user chose through the Save As...
    // dialog  as the new browser.download.dir since the old one
    // didn't exist.
    file = picker.file;

    if (file) {
      try {
        // Remove the file so that it's not there when we ensure non-existence later;
        // this is safe because for the file to exist, the user would have had to
        // confirm that he wanted the file overwritten.
        if (file.exists())
          file.remove(false);
      }
      catch (e) { }
      var newDir = file.parent.QueryInterface(Ci.nsILocalFile);
      prefs.setComplexValue("browser.download.lastDir", Ci.nsILocalFile, newDir);
      file = this.validateLeafName(newDir, file.leafName, null);
    }
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
      var collisionCount = 0;
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

  _notify: function hald_notify(aLauncher, aCallback) {
    let bundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");

    let notifier = Cc[aCallback ? "@mozilla.org/alerts-service;1" : "@mozilla.org/toaster-alerts-service;1"].getService(Ci.nsIAlertsService);
    notifier.showAlertNotification(URI_GENERIC_ICON_DOWNLOAD,
                                   bundle.GetStringFromName("alertDownloads"),
                                   bundle.GetStringFromName("alertCantOpenDownload"),
                                   true, "", aCallback, "downloadopen-fail");
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([HelperAppLauncherDialog]);
