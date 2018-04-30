/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Shared code for xpcshell and mochitests-chrome */
/* eslint-disable no-undef */

ChromeUtils.import("resource://gre/modules/FileUtils.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const PREF_APP_UPDATE_AUTO                       = "app.update.auto";
const PREF_APP_UPDATE_BACKGROUNDERRORS           = "app.update.backgroundErrors";
const PREF_APP_UPDATE_BACKGROUNDMAXERRORS        = "app.update.backgroundMaxErrors";
const PREF_APP_UPDATE_CANCELATIONS               = "app.update.cancelations";
const PREF_APP_UPDATE_CHANNEL                    = "app.update.channel";
const PREF_APP_UPDATE_DOORHANGER                 = "app.update.doorhanger";
const PREF_APP_UPDATE_DOWNLOADPROMPTATTEMPTS     = "app.update.download.attempts";
const PREF_APP_UPDATE_DOWNLOADPROMPTMAXATTEMPTS  = "app.update.download.maxAttempts";
const PREF_APP_UPDATE_DOWNLOADBACKGROUNDINTERVAL = "app.update.download.backgroundInterval";
const PREF_APP_UPDATE_ENABLED                    = "app.update.enabled";
const PREF_APP_UPDATE_IDLETIME                   = "app.update.idletime";
const PREF_APP_UPDATE_LOG                        = "app.update.log";
const PREF_APP_UPDATE_NOTIFIEDUNSUPPORTED        = "app.update.notifiedUnsupported";
const PREF_APP_UPDATE_PROMPTWAITTIME             = "app.update.promptWaitTime";
const PREF_APP_UPDATE_RETRYTIMEOUT               = "app.update.socket.retryTimeout";
const PREF_APP_UPDATE_SERVICE_ENABLED            = "app.update.service.enabled";
const PREF_APP_UPDATE_SILENT                     = "app.update.silent";
const PREF_APP_UPDATE_SOCKET_MAXERRORS           = "app.update.socket.maxErrors";
const PREF_APP_UPDATE_STAGING_ENABLED            = "app.update.staging.enabled";
const PREF_APP_UPDATE_URL                        = "app.update.url";
const PREF_APP_UPDATE_URL_DETAILS                = "app.update.url.details";
const PREF_APP_UPDATE_URL_MANUAL                 = "app.update.url.manual";

const PREFBRANCH_APP_PARTNER         = "app.partner.";
const PREF_DISTRIBUTION_ID           = "distribution.id";
const PREF_DISTRIBUTION_VERSION      = "distribution.version";

const NS_APP_PROFILE_DIR_STARTUP   = "ProfDS";
const NS_APP_USER_PROFILE_50_DIR   = "ProfD";
const NS_GRE_DIR                   = "GreD";
const NS_GRE_BIN_DIR               = "GreBinD";
const NS_XPCOM_CURRENT_PROCESS_DIR = "XCurProcD";
const XRE_EXECUTABLE_FILE          = "XREExeF";
const XRE_UPDATE_ROOT_DIR          = "UpdRootD";

const DIR_PATCH        = "0";
const DIR_TOBEDELETED  = "tobedeleted";
const DIR_UPDATES      = "updates";
const DIR_UPDATED      = IS_MACOSX ? "Updated.app" : "updated";

const FILE_ACTIVE_UPDATE_XML         = "active-update.xml";
const FILE_APPLICATION_INI           = "application.ini";
const FILE_BACKUP_UPDATE_LOG         = "backup-update.log";
const FILE_LAST_UPDATE_LOG           = "last-update.log";
const FILE_UPDATE_SETTINGS_INI       = "update-settings.ini";
const FILE_UPDATE_SETTINGS_INI_BAK   = "update-settings.ini.bak";
const FILE_UPDATER_INI               = "updater.ini";
const FILE_UPDATES_XML               = "updates.xml";
const FILE_UPDATE_LOG                = "update.log";
const FILE_UPDATE_MAR                = "update.mar";
const FILE_UPDATE_STATUS             = "update.status";
const FILE_UPDATE_TEST               = "update.test";
const FILE_UPDATE_VERSION            = "update.version";

const UPDATE_SETTINGS_CONTENTS = "[Settings]\n" +
                                 "ACCEPTED_MAR_CHANNEL_IDS=xpcshell-test\n";

const PR_RDWR        = 0x04;
const PR_CREATE_FILE = 0x08;
const PR_TRUNCATE    = 0x20;

var gChannel;

/* import-globals-from ../data/sharedUpdateXML.js */
Services.scriptloader.loadSubScript(DATA_URI_SPEC + "sharedUpdateXML.js", this);

const PERMS_FILE      = FileUtils.PERMS_FILE;
const PERMS_DIRECTORY = FileUtils.PERMS_DIRECTORY;

const MODE_WRONLY   = FileUtils.MODE_WRONLY;
const MODE_CREATE   = FileUtils.MODE_CREATE;
const MODE_APPEND   = FileUtils.MODE_APPEND;
const MODE_TRUNCATE = FileUtils.MODE_TRUNCATE;

const URI_UPDATES_PROPERTIES = "chrome://mozapps/locale/update/updates.properties";
const gUpdateBundle = Services.strings.createBundle(URI_UPDATES_PROPERTIES);

XPCOMUtils.defineLazyGetter(this, "gAUS", function test_gAUS() {
  return Cc["@mozilla.org/updates/update-service;1"].
         getService(Ci.nsIApplicationUpdateService).
         QueryInterface(Ci.nsITimerCallback).
         QueryInterface(Ci.nsIObserver).
         QueryInterface(Ci.nsIUpdateCheckListener);
});

XPCOMUtils.defineLazyServiceGetter(this, "gUpdateManager",
                                   "@mozilla.org/updates/update-manager;1",
                                   "nsIUpdateManager");

XPCOMUtils.defineLazyGetter(this, "gUpdateChecker", function test_gUC() {
  return Cc["@mozilla.org/updates/update-checker;1"].
         createInstance(Ci.nsIUpdateChecker);
});

XPCOMUtils.defineLazyGetter(this, "gUP", function test_gUP() {
  return Cc["@mozilla.org/updates/update-prompt;1"].
         createInstance(Ci.nsIUpdatePrompt);
});

XPCOMUtils.defineLazyGetter(this, "gDefaultPrefBranch", function test_gDPB() {
  return Services.prefs.getDefaultBranch(null);
});

XPCOMUtils.defineLazyGetter(this, "gPrefRoot", function test_gPR() {
  return Services.prefs.getBranch(null);
});

XPCOMUtils.defineLazyServiceGetter(this, "gEnv",
                                   "@mozilla.org/process/environment;1",
                                   "nsIEnvironment");

/* Triggers post-update processing */
function testPostUpdateProcessing() {
  gAUS.observe(null, "test-post-update-processing", "");
}

/* Initializes the update service stub */
function initUpdateServiceStub() {
  Cc["@mozilla.org/updates/update-service-stub;1"].
  createInstance(Ci.nsISupports);
}

/* Reloads the update metadata from disk */
function reloadUpdateManagerData() {
  gUpdateManager.QueryInterface(Ci.nsIObserver).
  observe(null, "um-reload-update-data", "");
}

const observer = {
  observe(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed" && aData == PREF_APP_UPDATE_CHANNEL) {
      let channel = gDefaultPrefBranch.getCharPref(PREF_APP_UPDATE_CHANNEL);
      if (channel != gChannel) {
        debugDump("Changing channel from " + channel + " to " + gChannel);
        gDefaultPrefBranch.setCharPref(PREF_APP_UPDATE_CHANNEL, gChannel);
      }
    }
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver])
};

/**
 * Sets the app.update.channel preference.
 *
 * @param  aChannel
 *         The update channel.
 */
function setUpdateChannel(aChannel) {
  gChannel = aChannel;
  debugDump("setting default pref " + PREF_APP_UPDATE_CHANNEL + " to " + gChannel);
  gDefaultPrefBranch.setCharPref(PREF_APP_UPDATE_CHANNEL, gChannel);
  gPrefRoot.addObserver(PREF_APP_UPDATE_CHANNEL, observer);
}

/**
 * Sets the app.update.url default preference.
 *
 * @param  aURL
 *         The update url. If not specified 'URL_HOST + "/update.xml"' will be
 *         used.
 */
function setUpdateURL(aURL) {
  let url = aURL ? aURL : URL_HOST + "/update.xml";
  debugDump("setting " + PREF_APP_UPDATE_URL + " to " + url);
  gDefaultPrefBranch.setCharPref(PREF_APP_UPDATE_URL, url);
}

/**
 * Returns either the active or regular update database XML file.
 *
 * @param  isActiveUpdate
 *         If true this will return the active-update.xml otherwise it will
 *         return the updates.xml file.
 */
function getUpdatesXMLFile(aIsActiveUpdate) {
  let file = getUpdatesRootDir();
  file.append(aIsActiveUpdate ? FILE_ACTIVE_UPDATE_XML : FILE_UPDATES_XML);
  return file;
}

/**
 * Writes the updates specified to either the active-update.xml or the
 * updates.xml.
 *
 * @param  aContent
 *         The updates represented as a string to write to the XML file.
 * @param  isActiveUpdate
 *         If true this will write to the active-update.xml otherwise it will
 *         write to the updates.xml file.
 */
function writeUpdatesToXMLFile(aContent, aIsActiveUpdate) {
  writeFile(getUpdatesXMLFile(aIsActiveUpdate), aContent);
}

/**
 * Writes the current update operation/state to a file in the patch
 * directory, indicating to the patching system that operations need
 * to be performed.
 *
 * @param  aStatus
 *         The status value to write.
 */
function writeStatusFile(aStatus) {
  let file = getUpdatesPatchDir();
  file.append(FILE_UPDATE_STATUS);
  writeFile(file, aStatus + "\n");
}

/**
 * Writes the current update version to a file in the patch directory,
 * indicating to the patching system the version of the update.
 *
 * @param  aVersion
 *         The version value to write.
 */
function writeVersionFile(aVersion) {
  let file = getUpdatesPatchDir();
  file.append(FILE_UPDATE_VERSION);
  writeFile(file, aVersion + "\n");
}

/**
 * Gets the root directory for the updates directory.
 *
 * @return nsIFile for the updates root directory.
 */
function getUpdatesRootDir() {
  return Services.dirsvc.get(XRE_UPDATE_ROOT_DIR, Ci.nsIFile);
}

/**
 * Gets the updates directory.
 *
 * @return nsIFile for the updates directory.
 */
function getUpdatesDir() {
  let dir = getUpdatesRootDir();
  dir.append(DIR_UPDATES);
  return dir;
}

/**
 * Gets the directory for update patches.
 *
 * @return nsIFile for the updates directory.
 */
function getUpdatesPatchDir() {
  let dir = getUpdatesDir();
  dir.append(DIR_PATCH);
  return dir;
}

/**
 * Writes text to a file. This will replace existing text if the file exists
 * and create the file if it doesn't exist.
 *
 * @param  aFile
 *         The file to write to. Will be created if it doesn't exist.
 * @param  aText
 *         The text to write to the file. If there is existing text it will be
 *         replaced.
 */
function writeFile(aFile, aText) {
  let fos = Cc["@mozilla.org/network/file-output-stream;1"].
            createInstance(Ci.nsIFileOutputStream);
  if (!aFile.exists()) {
    aFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
  }
  fos.init(aFile, MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE, PERMS_FILE, 0);
  fos.write(aText, aText.length);
  fos.close();
}

/**
 * Reads the current update operation/state in the status file in the patch
 * directory including the error code if it is present.
 *
 * @return The status value.
 */
function readStatusFile() {
  let file = getUpdatesPatchDir();
  file.append(FILE_UPDATE_STATUS);

  if (!file.exists()) {
    debugDump("update status file does not exists! Path: " + file.path);
    return STATE_NONE;
  }

  return readFile(file).split("\n")[0];
}

/**
 * Reads the current update operation/state in the status file in the patch
 * directory without the error code if it is present.
 *
 * @return The state value.
 */
function readStatusState() {
  return readStatusFile().split(": ")[0];
}

/**
 * Reads the current update operation/state in the status file in the patch
 * directory with the error code.
 *
 * @return The state value.
 */
function readStatusFailedCode() {
  return readStatusFile().split(": ")[1];
}

/**
 * Reads text from a file and returns the string.
 *
 * @param  aFile
 *         The file to read from.
 * @return The string of text read from the file.
 */
function readFile(aFile) {
  let fis = Cc["@mozilla.org/network/file-input-stream;1"].
            createInstance(Ci.nsIFileInputStream);
  if (!aFile.exists()) {
    return null;
  }
  // Specifying -1 for ioFlags will open the file with the default of PR_RDONLY.
  // Specifying -1 for perm will open the file with the default of 0.
  fis.init(aFile, -1, -1, Ci.nsIFileInputStream.CLOSE_ON_EOF);
  let sis = Cc["@mozilla.org/scriptableinputstream;1"].
            createInstance(Ci.nsIScriptableInputStream);
  sis.init(fis);
  let text = sis.read(sis.available());
  sis.close();
  return text;
}

/**
 * Reads the binary contents of a file and returns it as a string.
 *
 * @param  aFile
 *         The file to read from.
 * @return The contents of the file as a string.
 */
function readFileBytes(aFile) {
  debugDump("attempting to read file, path: " + aFile.path);
  let fis = Cc["@mozilla.org/network/file-input-stream;1"].
            createInstance(Ci.nsIFileInputStream);
  // Specifying -1 for ioFlags will open the file with the default of PR_RDONLY.
  // Specifying -1 for perm will open the file with the default of 0.
  fis.init(aFile, -1, -1, Ci.nsIFileInputStream.CLOSE_ON_EOF);
  let bis = Cc["@mozilla.org/binaryinputstream;1"].
            createInstance(Ci.nsIBinaryInputStream);
  bis.setInputStream(fis);
  let data = [];
  let count = fis.available();
  while (count > 0) {
    let bytes = bis.readByteArray(Math.min(65535, count));
    data.push(String.fromCharCode.apply(null, bytes));
    count -= bytes.length;
    if (bytes.length == 0) {
      throw "Nothing read from input stream!";
    }
  }
  data.join("");
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
  } catch (e) {
  }
  return null;
}

/**
 * Gets the file extension for an nsIFile.
 *
 * @param  aFile
 *         The file to get the file extension for.
 * @return The file extension.
 */
function getFileExtension(aFile) {
  return Services.io.newFileURI(aFile).QueryInterface(Ci.nsIURL).
         fileExtension;
}

/**
 * Removes the updates.xml file, active-update.xml file, and all files and
 * sub-directories in the updates directory except for the "0" sub-directory.
 * This prevents some tests from failing due to files being left behind when the
 * tests are interrupted.
 */
function removeUpdateDirsAndFiles() {
  let file = getUpdatesXMLFile(true);
  try {
    if (file.exists()) {
      file.remove(false);
    }
  } catch (e) {
    logTestInfo("Unable to remove file. Path: " + file.path +
                ", Exception: " + e);
  }

  file = getUpdatesXMLFile(false);
  try {
    if (file.exists()) {
      file.remove(false);
    }
  } catch (e) {
    logTestInfo("Unable to remove file. Path: " + file.path +
                ", Exception: " + e);
  }

  // This fails sporadically on Mac OS X so wrap it in a try catch
  let updatesDir = getUpdatesDir();
  try {
    cleanUpdatesDir(updatesDir);
  } catch (e) {
    logTestInfo("Unable to remove files / directories from directory. Path: " +
                updatesDir.path + ", Exception: " + e);
  }
}

/**
 * Removes all files and sub-directories in the updates directory except for
 * the "0" sub-directory.
 *
 * @param  aDir
 *         nsIFile for the directory to be deleted.
 */
function cleanUpdatesDir(aDir) {
  if (!aDir.exists()) {
    return;
  }

  let dirEntries = aDir.directoryEntries;
  while (dirEntries.hasMoreElements()) {
    let entry = dirEntries.getNext().QueryInterface(Ci.nsIFile);

    if (entry.isDirectory()) {
      if (entry.leafName == DIR_PATCH && entry.parent.leafName == DIR_UPDATES) {
        cleanUpdatesDir(entry);
        entry.permissions = PERMS_DIRECTORY;
      } else {
        try {
          entry.remove(true);
          return;
        } catch (e) {
        }
        cleanUpdatesDir(entry);
        entry.permissions = PERMS_DIRECTORY;
        try {
          entry.remove(true);
        } catch (e) {
          logTestInfo("cleanUpdatesDir: unable to remove directory. Path: " +
                      entry.path + ", Exception: " + e);
          throw (e);
        }
      }
    } else {
      entry.permissions = PERMS_FILE;
      try {
        entry.remove(false);
      } catch (e) {
        logTestInfo("cleanUpdatesDir: unable to remove file. Path: " +
                    entry.path + ", Exception: " + e);
        throw (e);
      }
    }
  }
}

/**
 * Deletes a directory and its children. First it tries nsIFile::Remove(true).
 * If that fails it will fall back to recursing, setting the appropriate
 * permissions, and deleting the current entry.
 *
 * @param  aDir
 *         nsIFile for the directory to be deleted.
 */
function removeDirRecursive(aDir) {
  if (!aDir.exists()) {
    return;
  }

  try {
    debugDump("attempting to remove directory. Path: " + aDir.path);
    aDir.remove(true);
    return;
  } catch (e) {
    logTestInfo("non-fatal error removing directory. Exception: " + e);
  }

  let dirEntries = aDir.directoryEntries;
  while (dirEntries.hasMoreElements()) {
    let entry = dirEntries.getNext().QueryInterface(Ci.nsIFile);

    if (entry.isDirectory()) {
      removeDirRecursive(entry);
    } else {
      entry.permissions = PERMS_FILE;
      try {
        debugDump("attempting to remove file. Path: " + entry.path);
        entry.remove(false);
      } catch (e) {
        logTestInfo("error removing file. Exception: " + e);
        throw (e);
      }
    }
  }

  aDir.permissions = PERMS_DIRECTORY;
  try {
    debugDump("attempting to remove directory. Path: " + aDir.path);
    aDir.remove(true);
  } catch (e) {
    logTestInfo("error removing directory. Exception: " + e);
    throw (e);
  }
}

/**
 * Returns the directory for the currently running process. This is used to
 * clean up after the tests and to locate the active-update.xml and updates.xml
 * files.
 *
 * @return nsIFile for the current process directory.
 */
function getCurrentProcessDir() {
  return Services.dirsvc.get(NS_XPCOM_CURRENT_PROCESS_DIR, Ci.nsIFile);
}

/**
 * Gets the application base directory.
 *
 * @return  nsIFile object for the application base directory.
 */
function getAppBaseDir() {
  return Services.dirsvc.get(XRE_EXECUTABLE_FILE, Ci.nsIFile).parent;
}

/**
 * Returns the Gecko Runtime Engine directory where files other than executable
 * binaries are located. On Mac OS X this will be <bundle>/Contents/Resources/
 * and the installation directory on all other platforms.
 *
 * @return nsIFile for the Gecko Runtime Engine directory.
 */
function getGREDir() {
  return Services.dirsvc.get(NS_GRE_DIR, Ci.nsIFile);
}

/**
 * Returns the Gecko Runtime Engine Binary directory where the executable
 * binaries are located such as the updater binary (Windows and Linux) or
 * updater package (Mac OS X). On Mac OS X this will be
 * <bundle>/Contents/MacOS/ and the installation directory on all other
 * platforms.
 *
 * @return nsIFile for the Gecko Runtime Engine Binary directory.
 */
function getGREBinDir() {
  return Services.dirsvc.get(NS_GRE_BIN_DIR, Ci.nsIFile);
}

/**
 * Logs TEST-INFO messages.
 *
 * @param  aText
 *         The text to log.
 * @param  aCaller (optional)
 *         An optional Components.stack.caller. If not specified
 *         Components.stack.caller will be used.
 */
function logTestInfo(aText, aCaller) {
  let caller = aCaller ? aCaller : Components.stack.caller;
  let now = new Date();
  let hh = now.getHours();
  let mm = now.getMinutes();
  let ss = now.getSeconds();
  let ms = now.getMilliseconds();
  let time = (hh < 10 ? "0" + hh : hh) + ":" +
             (mm < 10 ? "0" + mm : mm) + ":" +
             (ss < 10 ? "0" + ss : ss) + ":";
  if (ms < 10) {
    time += "00";
  } else if (ms < 100) {
    time += "0";
  }
  time += ms;
  let msg = time + " | TEST-INFO | " + caller.filename + " | [" + caller.name +
            " : " + caller.lineNumber + "] " + aText;
  LOG_FUNCTION(msg);
}

/**
 * Logs TEST-INFO messages when DEBUG_AUS_TEST evaluates to true.
 *
 * @param  aText
 *         The text to log.
 * @param  aCaller (optional)
 *         An optional Components.stack.caller. If not specified
 *         Components.stack.caller will be used.
 */
function debugDump(aText, aCaller) {
  if (DEBUG_AUS_TEST) {
    let caller = aCaller ? aCaller : Components.stack.caller;
    logTestInfo(aText, caller);
  }
}
