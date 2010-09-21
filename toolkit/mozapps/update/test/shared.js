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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Strong <robert.bugzilla@gmail.com> (Original Author)
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

/* Shared code for xpcshell and mochitests-chrome */

// const Cc, Ci, and Cr are defined in netwerk/test/httpserver/httpd.js so we
// need to define unique ones.
const AUS_Cc = Components.classes;
const AUS_Ci = Components.interfaces;
const AUS_Cr = Components.results;
const AUS_Cu = Components.utils;

const PREF_APP_UPDATE_BACKGROUNDERRORS    = "app.update.backgroundErrors";
const PREF_APP_UPDATE_BACKGROUNDMAXERRORS = "app.update.backgroundMaxErrors";
const PREF_APP_UPDATE_CERTS_BRANCH        = "app.update.certs.";
const PREF_APP_UPDATE_CERT_CHECKATTRS     = "app.update.cert.checkAttributes";
const PREF_APP_UPDATE_CERT_ERRORS         = "app.update.cert.errors";
const PREF_APP_UPDATE_CERT_MAXERRORS      = "app.update.cert.maxErrors";
const PREF_APP_UPDATE_CERT_REQUIREBUILTIN = "app.update.cert.requireBuiltIn";
const PREF_APP_UPDATE_CHANNEL             = "app.update.channel";
const PREF_APP_UPDATE_ENABLED             = "app.update.enabled";
const PREF_APP_UPDATE_IDLETIME            = "app.update.idletime";
const PREF_APP_UPDATE_LOG                 = "app.update.log";
const PREF_APP_UPDATE_NEVER_BRANCH        = "app.update.never.";
const PREF_APP_UPDATE_PROMPTWAITTIME      = "app.update.promptWaitTime";
const PREF_APP_UPDATE_SHOW_INSTALLED_UI   = "app.update.showInstalledUI";
const PREF_APP_UPDATE_SILENT              = "app.update.silent";
const PREF_APP_UPDATE_URL                 = "app.update.url";
const PREF_APP_UPDATE_URL_DETAILS         = "app.update.url.details";
const PREF_APP_UPDATE_URL_OVERRIDE        = "app.update.url.override";

const PREF_APP_UPDATE_CERT_INVALID_ATTR_NAME = PREF_APP_UPDATE_CERTS_BRANCH +
                                               "1.invalidName";

const PREF_APP_PARTNER_BRANCH             = "app.partner.";
const PREF_DISTRIBUTION_ID                = "distribution.id";
const PREF_DISTRIBUTION_VERSION           = "distribution.version";

const PREF_EXTENSIONS_UPDATE_URL          = "extensions.update.url";

const NS_APP_PROFILE_DIR_STARTUP   = "ProfDS";
const NS_APP_USER_PROFILE_50_DIR   = "ProfD";
const NS_GRE_DIR                   = "GreD";
const NS_XPCOM_CURRENT_PROCESS_DIR = "XCurProcD";
const XRE_UPDATE_ROOT_DIR          = "UpdRootD";

const STATE_NONE            = "null";
const STATE_DOWNLOADING     = "downloading";
const STATE_PENDING         = "pending";
const STATE_APPLYING        = "applying";
const STATE_SUCCEEDED       = "succeeded";
const STATE_DOWNLOAD_FAILED = "download-failed";
const STATE_FAILED          = "failed";

const CRC_ERROR   = 4;
const WRITE_ERROR = 7;

const FILE_BACKUP_LOG     = "backup-update.log";
const FILE_LAST_LOG       = "last-update.log";
const FILE_UPDATES_DB     = "updates.xml";
const FILE_UPDATE_ACTIVE  = "active-update.xml";
const FILE_UPDATE_ARCHIVE = "update.mar";
const FILE_UPDATE_LOG     = "update.log";
const FILE_UPDATE_STATUS  = "update.status";

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;

const PR_RDWR        = 0x04;
const PR_CREATE_FILE = 0x08;
const PR_APPEND      = 0x10;
const PR_TRUNCATE    = 0x20;
const PR_SYNC        = 0x40;
const PR_EXCL        = 0x80;

const PERMS_FILE      = 0644;
const PERMS_DIRECTORY = 0755;

AUS_Cu.import("resource://gre/modules/Services.jsm");
AUS_Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const URI_UPDATES_PROPERTIES = "chrome://mozapps/locale/update/updates.properties";
const gUpdateBundle = Services.strings.createBundle(URI_UPDATES_PROPERTIES);


__defineGetter__("gAUS", function() {
  delete this.gAUS;
  return this.gAUS = AUS_Cc["@mozilla.org/updates/update-service;1"].
                     getService(AUS_Ci.nsIApplicationUpdateService).
                     QueryInterface(AUS_Ci.nsITimerCallback).
                     QueryInterface(AUS_Ci.nsIObserver);
});

__defineGetter__("gUpdateManager", function() {
  delete this.gUpdateManager;
  return this.gUpdateManager = AUS_Cc["@mozilla.org/updates/update-manager;1"].
                               getService(AUS_Ci.nsIUpdateManager);
});

__defineGetter__("gUpdateChecker", function() {
  delete this.gUpdateChecker;
  return this.gUpdateChecker = AUS_Cc["@mozilla.org/updates/update-checker;1"].
                               createInstance(AUS_Ci.nsIUpdateChecker);
});

__defineGetter__("gUP", function() {
  delete this.gUP;
  return this.gUP = AUS_Cc["@mozilla.org/updates/update-prompt;1"].
                    createInstance(AUS_Ci.nsIUpdatePrompt);
});

__defineGetter__("gDefaultPrefBranch", function() {
  delete this.gDefaultPrefBranch;
  return this.gDefaultPrefBranch = Services.prefs.getDefaultBranch(null);
});

__defineGetter__("gZipW", function() {
  delete this.gZipW;
  return this.gZipW = AUS_Cc["@mozilla.org/zipwriter;1"].
                      createInstance(AUS_Ci.nsIZipWriter);
});

/* Initializes the update service stub */
function initUpdateServiceStub() {
  AUS_Cc["@mozilla.org/updates/update-service-stub;1"].
  createInstance(AUS_Ci.nsISupports);
}

/* Reloads the update metadata from disk */
function reloadUpdateManagerData() {
  gUpdateManager.QueryInterface(AUS_Ci.nsIObserver).
  observe(null, "um-reload-update-data", "");
}

/**
 * Sets the app.update.channel preference.
 * @param   aChannel
 *          The update channel. If not specified 'test_channel' will be used.
 */
function setUpdateChannel(aChannel) {
  gDefaultPrefBranch.setCharPref(PREF_APP_UPDATE_CHANNEL,
                                 aChannel ? aChannel : "test_channel");
}

/**
 * Sets the app.update.url.override preference.
 * @param   aURL
 *          The update url. If not specified 'URL_HOST + "update.xml"' will be
 *          used.
 */
function setUpdateURLOverride(aURL) {
  Services.prefs.setCharPref(PREF_APP_UPDATE_URL_OVERRIDE,
                             aURL ? aURL : URL_HOST + "update.xml");
}

/**
 * Constructs a string representing a remote update xml file.
 * @param   aUpdates
 *          The string representing the update elements.
 * @returns The string representing a remote update xml file.
 */
function getRemoteUpdatesXMLString(aUpdates) {
  return "<?xml version=\"1.0\"?>\n" +
         "<updates>\n" +
           aUpdates +
         "</updates>\n";
}

/**
 * Constructs a string representing an update element for a remote update xml
 * file.
 * See getUpdateString
 * @returns The string representing an update element for an update xml file.
 */
function getRemoteUpdateString(aPatches, aType, aName, aDisplayVersion,
                               aAppVersion, aPlatformVersion, aBuildID,
                               aDetailsURL, aBillboardURL, aLicenseURL,
                               aShowPrompt, aShowNeverForVersion, aShowSurvey,
                               aVersion, aExtensionVersion, aCustom1,
                               aCustom2) {
  return  getUpdateString(aType, aName, aDisplayVersion, aAppVersion,
                          aPlatformVersion, aBuildID, aDetailsURL,
                          aBillboardURL, aLicenseURL, aShowPrompt,
                          aShowNeverForVersion, aShowSurvey, aVersion,
                          aExtensionVersion, aCustom1, aCustom2) + ">\n" +
              aPatches + 
         "  </update>\n";
}

/**
 * Constructs a string representing a patch element for a remote update xml
 * file
 * See getPatchString
 * @returns The string representing a patch element for a remote update xml
 *          file.
 */
function getRemotePatchString(aType, aURL, aHashFunction, aHashValue, aSize) {
  return getPatchString(aType, aURL, aHashFunction, aHashValue, aSize) +
         "/>\n";
}

/**
 * Constructs a string representing a local update xml file.
 * @param   aUpdates
 *          The string representing the update elements.
 * @returns The string representing a local update xml file.
 */
function getLocalUpdatesXMLString(aUpdates) {
  if (!aUpdates || aUpdates == "")
    return "<updates xmlns=\"http://www.mozilla.org/2005/app-update\"/>"
  return ("<updates xmlns=\"http://www.mozilla.org/2005/app-update\">" +
           aUpdates +
         "</updates>").replace(/>\s+\n*</g,'><');
}

/**
 * Constructs a string representing an update element for a local update xml
 * file.
 * See getUpdateString
 * @param   aServiceURL
 *          The update's xml url.
 *          If null will default to 'http://test_service/'.
 * @param   aIsCompleteUpdate
 *          The string 'true' if this update was a complete update or the string
 *          'false' if this update was a partial update.
 *          If null will default to 'true'.
 * @param   aChannel
 *          The update channel name.
 *          If null will default to 'test_channel'.
 * @param   aForegroundDownload
 *          The string 'true' if this update was manually downloaded or the
 *          string 'false' if this update was automatically downloaded.
 *          If null will default to 'true'.
 * @param   aPreviousAppVersion
 *          The application version prior to applying the update.
 *          If null will not be present.
 * @returns The string representing an update element for an update xml file.
 */
function getLocalUpdateString(aPatches, aType, aName, aDisplayVersion,
                              aAppVersion, aPlatformVersion, aBuildID,
                              aDetailsURL, aBillboardURL, aLicenseURL,
                              aServiceURL, aInstallDate, aStatusText,
                              aIsCompleteUpdate, aChannel, aForegroundDownload,
                              aShowPrompt, aShowNeverForVersion, aShowSurvey,
                              aVersion, aExtensionVersion, aPreviousAppVersion,
                              aCustom1, aCustom2) {
  var serviceURL = aServiceURL ? aServiceURL : "http://test_service/";
  var installDate = aInstallDate ? aInstallDate : "1238441400314";
  var statusText = aStatusText ? aStatusText : "Install Pending";
  var isCompleteUpdate = typeof(aIsCompleteUpdate) == "string" ? aIsCompleteUpdate : "true";
  var channel = aChannel ? aChannel : "test_channel";
  var foregroundDownload =
    typeof(aForegroundDownload) == "string" ? aForegroundDownload : "true";
  var previousAppVersion = aPreviousAppVersion ? "previousAppVersion=\"" + aPreviousAppVersion + "\" " : "";
  return getUpdateString(aType, aName, aDisplayVersion, aAppVersion,
                         aPlatformVersion, aBuildID, aDetailsURL, aBillboardURL,
                         aLicenseURL, aShowPrompt, aShowNeverForVersion,
                         aShowSurvey, aVersion, aExtensionVersion, aCustom1,
                         aCustom2) +
                   " " +
                   previousAppVersion +
                   "serviceURL=\"" + serviceURL + "\" " +
                   "installDate=\"" + installDate + "\" " +
                   "statusText=\"" + statusText + "\" " +
                   "isCompleteUpdate=\"" + isCompleteUpdate + "\" " +
                   "channel=\"" + channel + "\" " +
                   "foregroundDownload=\"" + foregroundDownload + "\">"  +
              aPatches + 
         "  </update>";
}

/**
 * Constructs a string representing a patch element for a local update xml file.
 * See getPatchString
 * @param   aSelected
 *          Whether this patch is selected represented or not. The string 'true'
 *          denotes selected and the string 'false' denotes not selected.
 *          If null will default to the string 'true'.
 * @param   aState
 *          The patch's state.
 *          If null will default to STATE_SUCCEEDED (e.g. 'succeeded').
 * @returns The string representing a patch element for a local update xml file.
 */
function getLocalPatchString(aType, aURL, aHashFunction, aHashValue, aSize,
                             aSelected, aState) {
  var selected = typeof(aSelected) == "string" ? aSelected : "true";
  var state = aState ? aState : STATE_SUCCEEDED;
  return getPatchString(aType, aURL, aHashFunction, aHashValue, aSize) + " " +
         "selected=\"" + selected + "\" " +
         "state=\"" + state + "\"/>\n";
}

/**
 * Constructs a string representing an update element for a remote update xml
 * file.
 * @param   aType
 *          The update's type which should be major or minor.
 *          If null will default to 'major'.
 * @param   aName
 *          The update's name.
 *          If null will default to 'App Update Test'.
 * @param   aDisplayVersion
 *          The update's display version.
 *          If null will default to 'version 99.0'.
 * @param   aAppVersion
 *          The update's application version.
 *          If null will default to '99.0'.
 * @param   aPlatformVersion
 *          The update's platform version.
 *          If null will default to '99.0'.
 * @param   aBuildID
 *          The update's build id.
 *          If null will default to '20080811053724'.
 * @param   aDetailsURL
 *          The update's details url.
 *          If null will default to 'http://test_details/' due to due to
 *          bug 470244.
 * @param   aBillboardURL
 *          The update's billboard url.
 *          If null it will not be added.
 * @param   aLicenseURL
 *          The update's license url.
 *          If null will not be added.
 * @param   aShowPrompt
 *          Whether to show the prompt for the update when auto update is
 *          enabled.
 *          If null will not be added and the backend will default to false.
 * @param   aShowNeverForVersion
 *          Whether to show the 'No Thanks' button in the update prompt.
 *          If null will not be added and the backend will default to false.
 * @param   aShowSurvey
 *          Whether to show the 'No Thanks' button in the update prompt.
 *          If null will not be added and the backend will default to false.
 * @param   aVersion
 *          The update's application version from 1.9.2.
 *          If null will not be present.
 * @param   aExtensionVersion
 *          The update's application version from 1.9.2.
 *          If null will not be present.
 * @param   aCustom1
 *          A custom attribute name AND attribute value to add to the xml.
 *          Example: custom1_attribute="custom1 value"
 *          If null will not be present.
 * @param   aCustom2
 *          A custom attribute name AND attribute value to add to the xml.
 *          Example: custom2_attribute="custom2 value"
 *          If null will not be present.
 * @returns The string representing an update element for an update xml file.
 */
function getUpdateString(aType, aName, aDisplayVersion, aAppVersion,
                         aPlatformVersion, aBuildID, aDetailsURL, aBillboardURL,
                         aLicenseURL, aShowPrompt, aShowNeverForVersion,
                         aShowSurvey, aVersion, aExtensionVersion, aCustom1,
                         aCustom2) {
  var type = aType ? aType : "major";
  var name = aName ? aName : "App Update Test";
  var displayVersion = "";
  if (aDisplayVersion || !aVersion) {
    displayVersion = "displayVersion=\"" +
                     (aDisplayVersion ? aDisplayVersion
                                      : "version 99.0") + "\" ";
  }
  // version has been deprecated in favor of displayVersion but it still needs
  // to be tested for forward compatibility.
  var version = aVersion ? "version=\"" + aVersion + "\" " : "";
  var appVersion = "";
  if (aAppVersion || !aExtensionVersion) {
    appVersion = "appVersion=\"" + (aAppVersion ? aAppVersion : "99.0") + "\" ";
  }
  // extensionVersion has been deprecated in favor of appVersion but it still
  // needs to be tested for forward compatibility.
  var extensionVersion = aExtensionVersion ? "extensionVersion=\"" + aExtensionVersion + "\" " : "";
  var platformVersion = "";
  if (aPlatformVersion) {
    platformVersion = "platformVersion=\"" + (aPlatformVersion ? aPlatformVersion : "99.0") + "\" ";
  }
  var buildID = aBuildID ? aBuildID : "20080811053724";
  // XXXrstrong - not specifying a detailsURL will cause a leak due to bug 470244
//   var detailsURL = aDetailsURL ? "detailsURL=\"" + aDetailsURL + "\" " : "";
  var detailsURL = "detailsURL=\"" + (aDetailsURL ? aDetailsURL : "http://test_details/") + "\" ";
  var billboardURL = aBillboardURL ? "billboardURL=\"" + aBillboardURL + "\" " : "";
  var licenseURL = aLicenseURL ? "licenseURL=\"" + aLicenseURL + "\" " : "";
  var showPrompt = aShowPrompt ? "showPrompt=\"" + aShowPrompt + "\" " : "";
  var showNeverForVersion = aShowNeverForVersion ? "showNeverForVersion=\"" + aShowNeverForVersion + "\" " : "";
  var showSurvey = aShowSurvey ? "showSurvey=\"" + aShowSurvey + "\" " : "";
  var custom1 = aCustom1 ? aCustom1 + " " : "";
  var custom2 = aCustom2 ? aCustom2 + " " : "";
  return "  <update type=\"" + type + "\" " +
                   "name=\"" + name + "\" " +
                   displayVersion +
                   version +
                   appVersion +
                   extensionVersion +
                   platformVersion +
                   detailsURL +
                   billboardURL +
                   licenseURL +
                   showPrompt +
                   showNeverForVersion +
                   showSurvey +
                   custom1 +
                   custom2 +
                   "buildID=\"" + buildID + "\"";
}

/**
 * Constructs a string representing a patch element for an update xml file.
 * @param   aType
 *          The patch's type which should be complete or partial.
 *          If null will default to 'complete'.
 * @param   aURL
 *          The patch's url to the mar file.
 *          If null will default to 'http://localhost:4444/data/empty.mar'.
 * @param   aHashFunction
 *          The patch's hash function used to verify the mar file.
 *          If null will default to 'MD5'.
 * @param   aHashValue
 *          The patch's hash value used to verify the mar file.
 *          If null will default to '6232cd43a1c77e30191c53a329a3f99d'
 *          which is the MD5 hash value for the empty.mar.
 * @param   aSize
 *          The patch's file size for the mar file.
 *          If null will default to '775' which is the file size for the
 *          empty.mar.
 * @returns The string representing a patch element for an update xml file.
 */
function getPatchString(aType, aURL, aHashFunction, aHashValue, aSize) {
  var type = aType ? aType : "complete";
  var url = aURL ? aURL : URL_HOST + URL_PATH + "/empty.mar";
  var hashFunction = aHashFunction ? aHashFunction : "MD5";
  var hashValue = aHashValue ? aHashValue : "6232cd43a1c77e30191c53a329a3f99d";
  var size = aSize ? aSize : "775";
  return "    <patch type=\"" + type + "\" " +
                     "URL=\"" + url + "\" " +
                     "hashFunction=\"" + hashFunction + "\" " +
                     "hashValue=\"" + hashValue + "\" " +
                     "size=\"" + size + "\"";
}

/**
 * Writes the updates specified to either the active-update.xml or the
 * updates.xml.
 * @param   updates
 *          The updates represented as a string to write to the XML file.
 * @param   isActiveUpdate
 *          If true this will write to the active-update.xml otherwise it will
 *          write to the updates.xml file.
 */
function writeUpdatesToXMLFile(aContent, aIsActiveUpdate) {
  var file = getCurrentProcessDir();
  file.append(aIsActiveUpdate ? FILE_UPDATE_ACTIVE : FILE_UPDATES_DB);
  writeFile(file, aContent);
}

/**
 * Writes the current update operation/state to a file in the patch
 * directory, indicating to the patching system that operations need
 * to be performed.
 * @param   aStatus
 *          The status value to write.
 */
function writeStatusFile(aStatus) {
  var file = getUpdatesDir();
  file.append("0");
  file.append(FILE_UPDATE_STATUS);
  aStatus += "\n";
  writeFile(file, aStatus);
}

/**
 * Gets the updates directory.
 * @returns The updates directory.
 */
function getUpdatesDir() {
  var dir = getCurrentProcessDir();
  dir.append("updates");
  return dir;
}

/**
 * Writes text to a file. This will replace existing text if the file exists
 * and create the file if it doesn't exist.
 * @param   aFile
 *          The file to write to. Will be created if it doesn't exist.
 * @param   aText
 *          The text to write to the file. If there is existing text it will be
 *          replaced.
 */
function writeFile(aFile, aText) {
  var fos = AUS_Cc["@mozilla.org/network/file-output-stream;1"].
            createInstance(AUS_Ci.nsIFileOutputStream);
  if (!aFile.exists())
    aFile.create(AUS_Ci.nsILocalFile.NORMAL_FILE_TYPE, PERMS_FILE);
  fos.init(aFile, MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE, PERMS_FILE, 0);
  fos.write(aText, aText.length);
  fos.close();
}

/**
 * Reads text from a file and returns the string.
 * @param   aFile
 *          The file to read from.
 * @returns The string of text read from the file.
 */
function readFile(aFile) {
  var fis = AUS_Cc["@mozilla.org/network/file-input-stream;1"].
            createInstance(AUS_Ci.nsIFileInputStream);
  if (!aFile.exists())
    return null;
  fis.init(aFile, MODE_RDONLY, PERMS_FILE, 0);
  var sis = AUS_Cc["@mozilla.org/scriptableinputstream;1"].
            createInstance(AUS_Ci.nsIScriptableInputStream);
  sis.init(fis);
  var text = sis.read(sis.available());
  sis.close();
  return text;
}

/**
 * Reads the binary contents of a file and returns is as a string.
 * @param   aFile
 *          The file to read from.
 * @returns The contents of the file as a string.
 */
function readFileBytes(aFile) {
  var fis = AUS_Cc["@mozilla.org/network/file-input-stream;1"].
            createInstance(AUS_Ci.nsIFileInputStream);
  fis.init(aFile, -1, -1, false);
   var bis = AUS_Cc["@mozilla.org/binaryinputstream;1"].
             createInstance(AUS_Ci.nsIBinaryInputStream);
   bis.setInputStream(fis);
   var data = [];
   var count = fis.available();
   while (count > 0) {
     var bytes = bis.readByteArray(Math.min(65535, count));
     data.push(String.fromCharCode.apply(null, bytes));
     count -= bytes.length;
     if (bytes.length == 0)
       do_throw("Nothing read from input stream!");
   }
  data.join('');
  fis.close();
  return data.toString();
}

/* Returns human readable status text from the updates.properties bundle */
function getStatusText(aErrCode) {
  return getString("check_error-" + aErrCode);
}

/* Returns a string from the updates.properties bundle */
function getString(aName) {
  try {
    return gUpdateBundle.GetStringFromName(aName);
  }
  catch (e) {
  }
  return null;
}

/**
 * Gets the file extension for an nsIFile
 * @param   aFile
 *          The file to get the file extension for
 * @returns The file extension
 */
function getFileExtension(aFile) {
  return Services.io.newFileURI(aFile).QueryInterface(AUS_Ci.nsIURL).
         fileExtension;
}

/**
 * Removes the updates.xml file, active-update.xml file, and all files and
 * sub-directories in the updates directory except for the "0" sub-directory.
 * This prevents some tests from failing due to files being left behind when the
 * tests are interrupted.
 */
function removeUpdateDirsAndFiles() {
  var appDir = getCurrentProcessDir();
  var file = appDir.clone();
  file.append(FILE_UPDATE_ACTIVE);
  try {
    if (file.exists())
      file.remove(false);
  }
  catch (e) {
    dump("Unable to remove file\npath: " + file.path +
         "\nException: " + e + "\n");
  }

  file = appDir.clone();
  file.append(FILE_UPDATES_DB);
  try {
    if (file.exists())
      file.remove(false);
  }
  catch (e) {
    dump("Unable to remove file\npath: " + file.path +
         "\nException: " + e + "\n");
  }

  // This fails sporadically on Mac OS X so wrap it in a try catch
  var updatesDir = appDir.clone();
  updatesDir.append("updates");
  try {
    cleanUpdatesDir(updatesDir);
  }
  catch (e) {
    dump("Unable to remove files / directories from directory\npath: " +
         updatesDir.path + "\nException: " + e + "\n");
  }
}

/**
 * Removes all files and sub-directories in the updates directory except for
 * the "0" sub-directory.
 * @param   dir
 *          A nsIFile for the directory to be deleted
 */
function cleanUpdatesDir(aDir) {
  if (!aDir.exists())
    return;

  var dirEntries = aDir.directoryEntries;
  while (dirEntries.hasMoreElements()) {
    var entry = dirEntries.getNext().QueryInterface(AUS_Ci.nsIFile);

    if (entry.isDirectory()) {
      if (entry.leafName == "0" && entry.parent.leafName == "updates") {
        cleanUpdatesDir(entry);
        entry.permissions = PERMS_DIRECTORY;
      }
      else {
        try {
          entry.remove(true);
          return;
        }
        catch (e) {
        }
        cleanUpdatesDir(entry);
        entry.permissions = PERMS_DIRECTORY;
        entry.remove(true);
      }
    }
    else {
      entry.permissions = PERMS_FILE;
      entry.remove(false);
    }
  }
}

/**
 * Deletes a directory and its children. First it tries nsIFile::Remove(true).
 * If that fails it will fall back to recursing, setting the appropriate
 * permissions, and deleting the current entry.
 * @param   dir
 *          A nsIFile for the directory to be deleted
 */
function removeDirRecursive(aDir) {
  if (!aDir.exists())
    return;
  try {
    aDir.remove(true);
    return;
  }
  catch (e) {
  }

  var dirEntries = aDir.directoryEntries;
  while (dirEntries.hasMoreElements()) {
    var entry = dirEntries.getNext().QueryInterface(AUS_Ci.nsIFile);

    if (entry.isDirectory()) {
      removeDirRecursive(entry);
    }
    else {
      entry.permissions = PERMS_FILE;
      entry.remove(false);
    }
  }
  aDir.permissions = PERMS_DIRECTORY;
  aDir.remove(true);
}

/**
 * Returns the directory for the currently running process. This is used to
 * clean up after the tests and to locate the active-update.xml and updates.xml
 * files.
 */
function getCurrentProcessDir() {
  return Services.dirsvc.get(NS_XPCOM_CURRENT_PROCESS_DIR, AUS_Ci.nsIFile);
}

/**
 * Returns the Gecko Runtime Engine directory. This is used to locate the the
 * updater binary (Windows and Linux) or updater package (Mac OS X). For
 * XULRunner applications this is different than the currently running process
 * directory.
 */
function getGREDir() {
  return Services.dirsvc.get(NS_GRE_DIR, AUS_Ci.nsIFile);
}
