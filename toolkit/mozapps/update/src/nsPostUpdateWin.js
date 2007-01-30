/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Update Service.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net> (original author)
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

/**
 * This file contains an implementation of nsIRunnable, which may be invoked
 * to perform post-update modifications to the windows registry and uninstall
 * logs required to complete an update of the application.  This code is very
 * specific to the xpinstall wizard for windows.
 */

const URI_BRAND_PROPERTIES     = "chrome://branding/locale/brand.properties";

const KEY_APPDIR          = "XCurProcD";
const KEY_TMPDIR          = "TmpD";
const KEY_LOCALDATA       = "DefProfLRt";
const KEY_PROGRAMFILES    = "ProgF";
const KEY_UAPPDATA        = "UAppData";

// see prio.h
const PR_RDONLY      = 0x01;
const PR_WRONLY      = 0x02;
const PR_APPEND      = 0x10;

const PERMS_FILE     = 0644;
const PERMS_DIR      = 0700;

const nsIWindowsRegKey = Components.interfaces.nsIWindowsRegKey;

var gConsole = null;
var gAppUpdateLogPostUpdate = false;

//-----------------------------------------------------------------------------

/**
 * Console logging support
 */
function LOG(s) {
  if (gAppUpdateLogPostUpdate) {
    dump("*** PostUpdateWin: " + s + "\n");
    gConsole.logStringMessage(s);
  }
}

/**
 * This function queries the XPCOM directory service.
 */
function getFile(key) {
  var dirSvc =
      Components.classes["@mozilla.org/file/directory_service;1"].
      getService(Components.interfaces.nsIProperties);
  return dirSvc.get(key, Components.interfaces.nsIFile);
}

/**
 * Return the full path given a relative path and a base directory.
 */
function getFileRelativeTo(dir, relPath) {
  var file = dir.clone().QueryInterface(Components.interfaces.nsILocalFile);
  file.setRelativeDescriptor(dir, relPath);
  return file;
}

/**
 * Creates a new file object given a native file path.
 * @param   path
 *          The native file path.
 * @return  nsILocalFile object for the given native file path.
 */
function newFile(path) {
  var file = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  file.initWithPath(path);
  return file;
}

/**
 * This function returns a file input stream.
 */
function openFileInputStream(file) {
  var stream =
      Components.classes["@mozilla.org/network/file-input-stream;1"].
      createInstance(Components.interfaces.nsIFileInputStream);
  stream.init(file, PR_RDONLY, 0, 0);
  return stream;
}

/**
 * This function returns a file output stream.
 */
function openFileOutputStream(file, flags) {
  var stream =
      Components.classes["@mozilla.org/network/file-output-stream;1"].
      createInstance(Components.interfaces.nsIFileOutputStream);
  stream.init(file, flags, 0644, 0);
  return stream;
}

//-----------------------------------------------------------------------------

const PREFIX_FILE = "File: ";

function InstallLogWriter() {
}
InstallLogWriter.prototype = {
  _outputStream: null,  // nsIOutputStream to the install wizard log file

  /**
   * Write a single line to the output stream.
   */
  _writeLine: function(s) {
    s = s + "\r\n";
    this._outputStream.write(s, s.length);
  },

  /**
   * This function creates an empty uninstall update log file if it doesn't
   * exist and returns a reference to the resulting nsIFile.
   */
  _getUninstallLogFile: function() {
    var file = getFile(KEY_APPDIR); 
    file.append("uninstall");
    if (!file.exists())
      return null;

    file.append("uninstall.log");
    if (!file.exists())
      file.create(Components.interfaces.nsILocalFile.NORMAL_FILE_TYPE, PERMS_FILE);

    return file;
  },

  /**
   * Return the update.log file.  Use last-update.log file in case the
   * updates/0 directory has already been cleaned out (see bug 311302).
   */
  _getUpdateLogFile: function() {
    function appendUpdateLogPath(root) {
      var file = root.clone();
      file.append("updates");
      file.append("0");
      file.append("update.log");
      if (file.exists())
        return file;

      file = root; 
      file.append("updates");
      file.append("last-update.log");
      if (file.exists())
        return file;

      return null;
    }

    // See the local appdata first if app dir is under Program Files.
    var file = null;
    var updRoot = getFile(KEY_APPDIR); 
    var fileLocator = Components.classes["@mozilla.org/file/directory_service;1"]
                                .getService(Components.interfaces.nsIProperties);
    var programFilesDir = fileLocator.get(KEY_PROGRAMFILES,
        Components.interfaces.nsILocalFile);
    if (programFilesDir.contains(updRoot, true)) {
      var relativePath = updRoot.QueryInterface(Components.interfaces.nsILocalFile).
          getRelativeDescriptor(programFilesDir);
      var userLocalDir = fileLocator.get(KEY_LOCALDATA,
          Components.interfaces.nsILocalFile).parent;
      updRoot.setRelativeDescriptor(userLocalDir, relativePath);
      file = appendUpdateLogPath(updRoot);

      // When updating from Fx 2.0.0.1 to 2.0.0.3 (or later) on Vista,
      // we will have to see also user app data (see bug 351949).
      if (!file)
        file = appendUpdateLogPath(getFile(KEY_UAPPDATA));
    }

    // See the app dir if not found or app dir is out of Program Files.
    if (!file)
      file = appendUpdateLogPath(getFile(KEY_APPDIR));

    return file;
  },

  /**
   * Read update.log to extract information about files that were
   * newly added for this update.
   */
  _readUpdateLog: function(logFile, entries) {
    var stream;
    try {
      stream = openFileInputStream(logFile).
          QueryInterface(Components.interfaces.nsILineInputStream);

      var line = {};
      while (stream.readLine(line)) {
        var data = line.value.split(" ");
        if (data[0] == "EXECUTE" && data[1] == "ADD") {
          // The uninstaller requires the path separator to be "\" and
          // relative paths to start with a "\".
          var relPath = "\\" + data[2].replace(/\//g, "\\");
          entries[relPath] = null;
        }
      }
    } finally {
      if (stream)
        stream.close();
    }
  },

  /**
   * Read install_wizard log files to extract information about files that were
   * previously added by the xpinstall installer and software update.
   */
  _readXPInstallLog: function(logFile, entries) {
    var stream;
    try {
      stream = openFileInputStream(logFile).
          QueryInterface(Components.interfaces.nsILineInputStream);

      function fixPath(path, offset) {
        return path.substr(offset).replace(appDirPath, "");
      }

      var appDir = getFile(KEY_APPDIR);
      var appDirPath = appDir.path;
      var line = {};
      while (stream.readLine(line)) {
        var entry = line.value;
        // This works with both the entries from xpinstall (e.g. Installing: )
        // and from update (e.g. installing: )
        var searchStr = "nstalling: ";
        var index = entry.indexOf(searchStr);
        if (index != -1) {
          entries[fixPath(entry, index + searchStr.length)] = null;
          continue;
        }

        searchStr = "Replacing: ";
        index = entry.indexOf(searchStr);
        if (index != -1) {
          entries[fixPath(entry, index + searchStr.length)] = null;
          continue;
        }

        searchStr = "Windows Shortcut: ";
        index = entry.indexOf(searchStr);
        if (index != -1) {
          entries[fixPath(entry + ".lnk", index + searchStr.length)] = null;
          continue;
        }
      }
    } finally {
      if (stream)
        stream.close();
    }
  },

  _readUninstallLog: function(logFile, entries) {
    var stream;
    try {
      stream = openFileInputStream(logFile).
          QueryInterface(Components.interfaces.nsILineInputStream);

      var line = {};
      var searchStr = "File: ";
      while (stream.readLine(line)) {
        var index = line.value.indexOf(searchStr);
        if (index != -1) {
          var str = line.value.substr(index + searchStr.length);
          entries.push(str);
        }
      }
    } finally {
      if (stream)
        stream.close();
    }
  },

  /**
   * This function initializes the log writer and is responsible for
   * translating 'update.log' and the 'install_wizard' logs to the NSIS format.
   */
  begin: function() {
    var updateLog = this._getUpdateLogFile();
    if (!updateLog)
      return;

    var newEntries = { };
    this._readUpdateLog(updateLog, newEntries);

    try {
      const nsIDirectoryEnumerator = Components.interfaces.nsIDirectoryEnumerator;
      const nsILocalFile = Components.interfaces.nsILocalFile;
      var prefixWizLog = "install_wizard";
      var uninstallDir = getFile(KEY_APPDIR); 
      uninstallDir.append("uninstall");
      var entries = uninstallDir.directoryEntries.QueryInterface(nsIDirectoryEnumerator);
      while (true) {
        var wizLog = entries.nextFile;
        if (!wizLog)
          break;
        if (wizLog instanceof nsILocalFile && !wizLog.isDirectory() &&
            wizLog.leafName.indexOf(prefixWizLog) == 0) {
          this._readXPInstallLog(wizLog, newEntries);
          wizLog.remove(false);
        }
      }
    }
    catch (e) {}
    if (entries)
      entries.close();

    var uninstallLog = this._getUninstallLogFile();
    var oldEntries = [];
    this._readUninstallLog(uninstallLog, oldEntries);

    // Prevent writing duplicate entries in the log file
    for (var relPath in newEntries) {
      if (oldEntries.indexOf(relPath) != -1)
        delete newEntries[relPath];
    }

    if (newEntries.length == 0)
      return;

    // since we are not running with elevated privs, we can't write out
    // the log file (at least, not on Vista).  So, write the output to
    // temp, and then later, we'll pass the file (gCopiedLog) to
    // the post update clean up process, which can copy it to
    // the desired location (because it will have elevated privs)
    gCopiedLog = getFile(KEY_TMPDIR);
    gCopiedLog.append("uninstall");
    gCopiedLog.createUnique(gCopiedLog.DIRECTORY_TYPE, PERMS_DIR);
    if (uninstallLog)
      uninstallLog.copyTo(gCopiedLog, "uninstall.log");
    gCopiedLog.append("uninstall.log");
    
    LOG("uninstallLog = " + uninstallLog.path);
    LOG("copiedLog = " + gCopiedLog.path);
    
    if (!gCopiedLog.exists())
      gCopiedLog.create(Components.interfaces.nsILocalFile.NORMAL_FILE_TYPE, 
                        PERMS_FILE);
      
    this._outputStream =
        openFileOutputStream(gCopiedLog, PR_WRONLY | PR_APPEND);

    // The NSIS uninstaller deletes all directories where the installer has
    // added a file if the directory is empty after the files have been removed
    // so there is no need to log directories.
    for (var relPath in newEntries)
      this._writeLine(PREFIX_FILE + relPath);
  },

  end: function() {
    if (!this._outputStream)
      return;
    this._outputStream.close();
    this._outputStream = null;
  }
};

var installLogWriter;
var gCopiedLog;

//-----------------------------------------------------------------------------

/**
 * A thin wrapper around nsIWindowsRegKey
 * note, only the "read" methods are exposed.  If you want to write
 * to the registry on Vista, you need to be a priveleged app.
 * We've moved that code into the uninstaller.
 */
function RegKey() {
  // Internally, we may pass parameters to this constructor.
  if (arguments.length == 3) {
    this._key = arguments[0];
    this._root = arguments[1];
    this._path = arguments[2];
  } else {
    this._key =
        Components.classes["@mozilla.org/windows-registry-key;1"].
        createInstance(nsIWindowsRegKey);
  }
}
RegKey.prototype = {
  _key: null,
  _root: null,
  _path: null,

  ACCESS_READ:  nsIWindowsRegKey.ACCESS_READ,

  ROOT_KEY_CURRENT_USER: nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
  ROOT_KEY_LOCAL_MACHINE: nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
  ROOT_KEY_CLASSES_ROOT: nsIWindowsRegKey.ROOT_KEY_CLASSES_ROOT,
  
  close: function() {
    this._key.close();
    this._root = null;
    this._path = null;
  },

  open: function(rootKey, path, mode) {
    this._key.open(rootKey, path, mode);
    this._root = rootKey;
    this._path = path;
  },

  openChild: function(path, mode) {
    var child = this._key.openChild(path, mode);
    return new RegKey(child, this._root, this._path + "\\" + path);
  },

  readStringValue: function(name) {
    return this._key.readStringValue(name);
  },

  hasValue: function(name) {
    return this._key.hasValue(name);
  },

  hasChild: function(name) {
    return this._key.hasChild(name);
  },

  get childCount() {
    return this._key.childCount;
  },

  getChildName: function(index) {
    return this._key.getChildName(index);
  },

  toString: function() {
    var root;
    switch (this._root) {
    case this.ROOT_KEY_CLASSES_ROOT:
      root = "HKEY_KEY_CLASSES_ROOT";
      break;
    case this.ROOT_KEY_LOCAL_MACHINE:
      root = "HKEY_LOCAL_MACHINE";
      break;
    case this.ROOT_KEY_CURRENT_USER:
      root = "HKEY_CURRENT_USER";
      break;
    default:
      LOG("unknown root key");
      return "";
    }
    return root + "\\" + this._path;
  }
};

/**
 * This method walks the registry looking for the registry keys of
 * the previous version of the application.
 */
function haveOldInstall(key, brandFullName, version) {
  var ourInstallDir = getFile(KEY_APPDIR);
  var result = false;
  var childKey, productKey, mainKey;
  try {
    for (var i = 0; i < key.childCount; ++i) {
      var childName = key.getChildName(i);
      childKey = key.openChild(childName, key.ACCESS_READ);
      if (childKey.hasValue("CurrentVersion")) {
        for (var j = 0; j < childKey.childCount; ++j) {
          var productVer = childKey.getChildName(j); 
          productKey = childKey.openChild(productVer, key.ACCESS_READ);
          if (productKey.hasChild("Main")) {
            mainKey = productKey.openChild("Main", key.ACCESS_READ);
            var installDir = mainKey.readStringValue("Install Directory");
            mainKey.close();
            LOG("old install? " + installDir + " vs " + ourInstallDir.path);
            LOG("old install? " + childName + " vs " + brandFullName);
            LOG("old install? " + productVer.split(" ")[0] + " vs " + version);
            if (newFile(installDir).equals(ourInstallDir) &&
                (childName != brandFullName ||
                productVer.split(" ")[0] != version)) {
              result = true;
            }
          }
          productKey.close();
          if (result)
            break;
        }
      }
      childKey.close();
      if (result)
        break;
    }
  } catch (e) {
    result = false;
    if (childKey)
      childKey.close();
    if (productKey)
      productKey.close();
    if (mainKey)
      mainKey.close();
  }
  return result;
}

function checkRegistry()
{
  // XXX todo
  // this is firefox specific
  // figure out what to do about tbird and sunbird, etc   
  LOG("checkRegistry");

  var result = false;
  try {
    var key = new RegKey();
    key.open(RegKey.prototype.ROOT_KEY_CLASSES_ROOT, "FirefoxHTML\\shell\\open\\command", key.ACCESS_READ);
    var commandKey = key.readStringValue("");
    LOG("commandKey = " + commandKey);
    // if "-requestPending" is not found, we need to do the cleanup
    result = (commandKey.indexOf("-requestPending") == -1);
  } catch (e) {
    LOG("failed to open command key for FirefoxHTML: " + e);
  }
  key.close();
  return result;
}

function checkOldInstall(rootKey, vendorShortName, brandFullName, version)
{
  var key = new RegKey();
  var result = false;

  try {
    key.open(rootKey, "SOFTWARE\\" + vendorShortName, key.ACCESS_READ);
    LOG("checkOldInstall: " + key + " " + brandFullName + " " + version);
    result = haveOldInstall(key, brandFullName, version);
  } catch (e) {
    LOG("failed trying to find old install: " + e);
  }
  key.close();
  return result;
}

//-----------------------------------------------------------------------------

function nsPostUpdateWin() {
  gConsole = Components.classes["@mozilla.org/consoleservice;1"]
                       .getService(Components.interfaces.nsIConsoleService);
  var prefs = Components.classes["@mozilla.org/preferences-service;1"].
              getService(Components.interfaces.nsIPrefBranch);
  try {
    gAppUpdateLogPostUpdate = prefs.getBoolPref("app.update.log.all");
  }
  catch (ex) {
  }
  try {
    if (!gAppUpdateLogPostUpdate) 
      gAppUpdateLogPostUpdate = prefs.getBoolPref("app.update.log.PostUpdate");
  }
  catch (ex) {
  }
}

nsPostUpdateWin.prototype = {
  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIRunnable) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  run: function() {
    try {
      installLogWriter = new InstallLogWriter();
      try {
        installLogWriter.begin();
      } finally {
        installLogWriter.end();
        installLogWriter = null;
      }
    } catch (e) {
      LOG(e);
    } 
    
    var app =
      Components.classes["@mozilla.org/xre/app-info;1"].
        getService(Components.interfaces.nsIXULAppInfo).
        QueryInterface(Components.interfaces.nsIXULRuntime);

    var sbs =
      Components.classes["@mozilla.org/intl/stringbundle;1"].
      getService(Components.interfaces.nsIStringBundleService);
    var brandBundle = sbs.createBundle(URI_BRAND_PROPERTIES);

    var vendorShortName;
    try {
      // The Thunderbird vendorShortName is "Mozilla Thunderbird", but we
      // just want "Thunderbird", so allow it to be overridden in prefs.

      var prefs =
        Components.classes["@mozilla.org/preferences-service;1"].
        getService(Components.interfaces.nsIPrefBranch);

      vendorShortName = prefs.getCharPref("app.update.vendorName.override");
    }
    catch (e) {
      vendorShortName = brandBundle.GetStringFromName("vendorShortName");
    }
    var brandFullName = brandBundle.GetStringFromName("brandFullName");

    if (!gCopiedLog && 
        !checkRegistry() &&
        !checkOldInstall(RegKey.prototype.ROOT_KEY_LOCAL_MACHINE, 
                         vendorShortName, brandFullName, app.version) &&
        !checkOldInstall(RegKey.prototype.ROOT_KEY_CURRENT_USER, 
                         vendorShortName, brandFullName, app.version)) {
      LOG("nothing to do, so don't launch the helper");
      return;
    }

    try {
      var winAppHelper = 
        app.QueryInterface(Components.interfaces.nsIWinAppHelper);

      // note, gCopiedLog could be null
      if (gCopiedLog)
        LOG("calling postUpdate with: " + gCopiedLog.path);
      else
        LOG("calling postUpdate without a log");

      winAppHelper.postUpdate(gCopiedLog);
    } catch (e) {
      LOG("failed to launch the helper to do the post update cleanup: " + e); 
    }
  }
};

//-----------------------------------------------------------------------------

var gModule = {
  registerSelf: function(compMgr, fileSpec, location, type) {
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    
    for (var key in this._objects) {
      var obj = this._objects[key];
      compMgr.registerFactoryLocation(obj.CID, obj.className, obj.contractID,
                                      fileSpec, location, type);
    }
  },
  
  getClassObject: function(compMgr, cid, iid) {
    if (!iid.equals(Components.interfaces.nsIFactory))
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    for (var key in this._objects) {
      if (cid.equals(this._objects[key].CID))
        return this._objects[key].factory;
    }
    
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  
  _makeFactory: #1= function(ctor) {
    function ci(outer, iid) {
      if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      return (new ctor()).QueryInterface(iid);
    } 
    return { createInstance: ci };
  },
  
  _objects: {
    manager: { CID        : Components.ID("{d15b970b-5472-40df-97e8-eb03a04baa82}"),
               contractID : "@mozilla.org/updates/post-update;1",
               className  : "nsPostUpdateWin",
               factory    : #1#(nsPostUpdateWin)
             },
  },
  
  canUnload: function(compMgr) {
    return true;
  }
};

function NSGetModule(compMgr, fileSpec) {
  return gModule;
}
