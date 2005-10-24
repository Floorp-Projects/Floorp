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
const URI_UNINSTALL_PROPERTIES = "chrome://branding/content/uninstall.properties";

const KEY_APPDIR          = "XCurProcD";
const KEY_WINDIR          = "WinD";
const KEY_COMPONENTS_DIR  = "ComsD";
const KEY_PLUGINS_DIR     = "APlugns";
const KEY_EXECUTABLE_FILE = "XREExeF";

// see prio.h
const PR_RDONLY      = 0x01;
const PR_WRONLY      = 0x02;
const PR_APPEND      = 0x10;

const nsIWindowsRegKey = Components.interfaces.nsIWindowsRegKey;

//-----------------------------------------------------------------------------

/**
 * Console logging support
 */
function LOG(s) {
  dump("*** PostUpdateWin: " + s + "\n");
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

/**
 * Gets the current value of the locale.  It's possible for this preference to
 * be localized, so we have to do a little extra work here.  Similar code
 * exists in nsHttpHandler.cpp when building the UA string.
 */
function getLocale() {
  const prefName = "general.useragent.locale";
  var prefs =
      Components.classes["@mozilla.org/preferences-service;1"].
      getService(Components.interfaces.nsIPrefBranch);
  try {
    return prefs.getComplexValue(prefName,
               Components.interfaces.nsIPrefLocalizedString).data;
  } catch (e) {}

  return prefs.getCharPref(prefName);
}

//-----------------------------------------------------------------------------

const PREFIX_INSTALLING = "installing: ";
const PREFIX_CREATE_FOLDER = "create folder: ";
const PREFIX_CREATE_REGISTRY_KEY = "create registry key: ";
const PREFIX_STORE_REGISTRY_VALUE_STRING = "store registry value string: ";

function InstallLogWriter() {
}
InstallLogWriter.prototype = {
  _outputStream: null,  // nsIOutputStream to the install wizard log file

  /**
   * Write a single line to the output stream.
   */
  _writeLine: function(s) {
    s = s + "\n";
    this._outputStream.write(s, s.length);
  },

  /**
   * This function creates an empty install wizard log file and returns
   * a reference to the resulting nsIFile.
   */
  _getInstallLogFile: function() {
    function makeLeafName(index) {
      return "install_wizard" + index + ".log"
    }

    var file = getFile(KEY_APPDIR); 
    file.append("uninstall");
    if (!file.exists())
      return null;

    file.append("x");
    var n = 0;
    do {
      file.leafName = makeLeafName(++n);
    } while (file.exists());

    if (n == 1)
      return null;  // What, no install wizard log file?

    file.leafName = makeLeafName(n - 1);
    return file;
  },

  /**
   * Return the update.log file.  Use last-update.log file in case the
   * updates/0 directory has already been cleaned out (see bug 311302).
   */
  _getUpdateLogFile: function() {
    var file = getFile(KEY_APPDIR); 
    file.append("updates");
    file.append("0");
    file.append("update.log");
    if (file.exists())
      return file;

    file = getFile(KEY_APPDIR); 
    file.append("updates");
    file.append("last-update.log");
    if (file.exists())
      return file;

    return null;
  },

  /**
   * Read update.log to extract information about files that were
   * newly added for this update.
   */
  _readUpdateLog: function(logFile, entries) {
    var appDir = getFile(KEY_APPDIR);
    var stream;
    try {
      stream = openFileInputStream(logFile).
          QueryInterface(Components.interfaces.nsILineInputStream);

      var dirs = {};
      var line = {};
      while (stream.readLine(line)) {
        var data = line.value.split(" ");
        if (data[0] == "EXECUTE" && data[1] == "ADD") {
          var file = getFileRelativeTo(appDir, data[2]);
          
          // remember all parent directories
          var parent = file.parent;
          while (!parent.equals(appDir)) {
            dirs[parent.path] = true;
            parent = parent.parent;
          }

          // remember the file
          entries.files.push(file.path);
        }
      }
      for (var d in dirs)
        entries.dirs.push(d);
      // Sort the directories so that subdirectories are deleted first.
      entries.dirs.sort();
    } finally {
      if (stream)
        stream.close();
    }
  },

  /**
   * This function initializes the log writer and is responsible for
   * translating 'update.log' to the install wizard format.
   */
  begin: function() {
    var installLog = this._getInstallLogFile();
    var updateLog = this._getUpdateLogFile();
    if (!installLog || !updateLog)
      return;

    var entries = { dirs: [], files: [] };
    this._readUpdateLog(updateLog, entries);

    this._outputStream =
        openFileOutputStream(installLog, PR_WRONLY | PR_APPEND);
    this._writeLine("\nUPDATE [" + new Date().toUTCString() + "]");

    var i;
    // The log file is processed in reverse order, so list directories before
    // files to ensure that the directories will be deleted.
    for (i = 0; i < entries.dirs.length; ++i)
      this._writeLine(PREFIX_CREATE_FOLDER + entries.dirs[i]);
    for (i = 0; i < entries.files.length; ++i)
      this._writeLine(PREFIX_INSTALLING + entries.files[i]);
  },

  /**
   * This function records the creation of a registry key.
   */
  registryKeyCreated: function(keyPath) {
    if (!this._outputStream)
      return;
    this._writeLine(PREFIX_CREATE_REGISTRY_KEY + keyPath + " []");
  },

  /**
   * This function records the creation of a registry key value.
   */
  registryKeyValueSet: function(keyPath, valueName) {
    if (!this._outputStream)
      return;
    this._writeLine(
        PREFIX_STORE_REGISTRY_VALUE_STRING + keyPath + " [" + valueName + "]");
  },

  end: function() {
    if (!this._outputStream)
      return;
    this._outputStream.close();
    this._outputStream = null;
  }
};

var installLogWriter;

//-----------------------------------------------------------------------------

/**
 * A thin wrapper around nsIWindowsRegKey that keeps track of its path
 * and notifies the installLogWriter when modifications are made.
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
  ACCESS_WRITE: nsIWindowsRegKey.ACCESS_WRITE,
  ACCESS_ALL:   nsIWindowsRegKey.ACCESS_ALL,

  ROOT_KEY_CURRENT_USER: nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
  ROOT_KEY_LOCAL_MACHINE: nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,

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

  createChild: function(path, mode) {
    var child = this._key.createChild(path, mode);
    var key = new RegKey(child, this._root, this._path + "\\" + path);
    if (installLogWriter)
      installLogWriter.registryKeyCreated(key.toString());
    return key;
  },

  readStringValue: function(name) {
    return this._key.readStringValue(name);
  },

  writeStringValue: function(name, value) {
    this._key.writeStringValue(name, value);
    if (installLogWriter)
      installLogWriter.registryKeyValueSet(this.toString(), name);
  },

  writeIntValue: function(name, value) {
    this._key.writeIntValue(name, value);
    if (installLogWriter)
      installLogWriter.registryKeyValueSet(this.toString(), name);
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

  removeChild: function(name) {
    this._key.removeChild(name);
  },

  toString: function() {
    var root;
    switch (this._root) {
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

//-----------------------------------------------------------------------------

/*
RegKey.prototype = {
  // The name of the registry key
  name: "";

  // An array of strings, where each even-indexed string names a value,
  // and the subsequent string provides data for the value.
  values: [];

  // An array of RegKey objects.
  children: [];
}
*/

/**
 * This function creates a heirarchy of registry keys.  If any of the
 * keys or values already exist, then they will be updated instead.
 * @param rootKey
 *        The root registry key from which to create the new registry
 *        keys.
 * @param data
 *        A JS object with properties: "name", "values", and "children"
 *        as defined above.  All children of this key will be created.
 */
function createRegistryKeys(rootKey, data) {
  var key;
  try {
    key = rootKey.createChild(data.name, rootKey.ACCESS_WRITE);
    var i;
    if ("values" in data) {
      for (i = 0; i < data.values.length; i += 2)
        key.writeStringValue(data.values[i], data.values[i + 1]); 
    }
    if ("children" in data) {
      for (i = 0; i < data.children.length; ++i)
        createRegistryKeys(key, data.children[i]);
    }
    key.close();
  } catch (e) {
    LOG(e);
    if (key)
      key.close();
  }
}

/**
 * This function deletes the specified registry key and optionally all of its
 * children.
 * @param rootKey
 *        The parent nsIwindowRegKey of the key to delete.
 * @param name
 *        The name of the key to delete.
 * @param recurse
 *        Pass true to also delete all children of the named key.  Take care!
 */
function deleteRegistryKey(rootKey, name, recurse) {
  if (!rootKey.hasChild(name)) {
    LOG("deleteRegistryKey: rootKey does not have child: \"" + name + "\"");
    return;
  }
  if (recurse) {
    var key = rootKey.openChild(name, rootKey.ACCESS_ALL);
    try {
      for (var i = key.childCount - 1; i >= 0; --i)
        deleteRegistryKey(key, key.getChildName(i), true);
    } finally {
      key.close();
    }
  }
  rootKey.removeChild(name);
}

/**
 * This method walks the registry looking for the registry keys of
 * the previous version of the application.
 */
function locateOldInstall(key, ourInstallDir) {
  var result, childKey, productKey, mainKey;
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
            var menuPath = mainKey.readStringValue("Program Folder Path");
            mainKey.close();
            if (newFile(installDir).equals(ourInstallDir)) {
              result = new Object();
              result.fullName = childName;
              result.versionWithLocale = productVer;
              result.version = productVer.split(" ")[0];
              result.menuPath = menuPath;
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
    result = null;
    if (childKey)
      childKey.close();
    if (productKey)
      productKey.close();
    if (mainKey)
      mainKey.close();
  }
  return result;
}

/**
 * Delete registry keys left-over from the previous version of the app
 * installed at our location.
 */
function deleteOldRegKeys(key, info) {
  deleteRegistryKey(key, info.fullName + " " + info.version, true);
  var productKey = key.openChild(info.fullName, key.ACCESS_ALL);
  var productCount;
  try {
    deleteRegistryKey(productKey, info.versionWithLocale, true);
    productCount = productKey.childCount;
  } finally {
    productKey.close();
  }
  if (productCount == 0)
    key.removeChild(info.fullName);  
}

/**
 * The installer sets various registry keys and values that may need to be
 * updated.
 *
 * This operation is a bit tricky since we do not know the previous value of
 * brandFullName.  As a result, we must walk the registry looking for an
 * existing key that references the same install directory.  We assume that
 * the value of vendorShortName does not change across updates.
 */
function updateRegistry(rootKey) {
  LOG("updateRegistry");

  var ourInstallDir = getFile(KEY_APPDIR);

  var app =
    Components.classes["@mozilla.org/xre/app-info;1"].
    getService(Components.interfaces.nsIXULAppInfo).
    QueryInterface(Components.interfaces.nsIXULRuntime);

  var sbs =
      Components.classes["@mozilla.org/intl/stringbundle;1"].
      getService(Components.interfaces.nsIStringBundleService);
  var brandBundle = sbs.createBundle(URI_BRAND_PROPERTIES);
  var brandFullName = brandBundle.GetStringFromName("brandFullName");
  var vendorShortName = brandBundle.GetStringFromName("vendorShortName");

  var key = new RegKey();

  var oldInstall;
  try {
    key.open(rootKey, "SOFTWARE\\" + vendorShortName, key.ACCESS_READ);
    oldInstall = locateOldInstall(key, ourInstallDir);
  } finally {
    key.close();
  }

  if (!oldInstall) {
    LOG("no existing registry keys found");
    return;
  }

  // Maybe nothing needs to be changed...
  if (oldInstall.fullName == brandFullName &&
      oldInstall.version == app.version) {
    LOG("registry is up-to-date");
    return;
  }

  // Delete the old keys:
  try {
    key.open(rootKey, "SOFTWARE\\" + vendorShortName, key.ACCESS_READ);
    deleteOldRegKeys(key, oldInstall);
  } finally {
    key.close();
  }

  // Create the new keys:

  var versionWithLocale = app.version + " (" + getLocale() + ")";
  var installPath = ourInstallDir.path + "\\";
  var pathToExe = getFile(KEY_EXECUTABLE_FILE).path;

  var Key_bin = {
    name: "bin",
    values: [
      "PathToExe", pathToExe
    ]
  };
  var Key_extensions = {
    name: "Extensions",
    values: [
      "Components", getFile(KEY_COMPONENTS_DIR).path + "\\",
      "Plugins", getFile(KEY_PLUGINS_DIR).path + "\\"
    ]
  };
  var Key_nameWithVersion = {
    name: brandFullName + " " + app.version,
    values: [
      "GeckoVer", app.platformVersion
    ],
    children: [
      Key_bin,
      Key_extensions
    ]
  };
  var Key_main = {
    name: "Main",
    values: [
      "Install Directory", installPath,
      "PathToExe", pathToExe,
      "Program Folder Path", oldInstall.menuPath
    ]
  };
  var Key_uninstall = {
    name: "Uninstall",
    values: [
      "Description", brandFullName + " (" + app.version + ")",
      "Uninstall Log Folder", installPath + "uninstall"
    ]
  };
  var Key_versionWithLocale = {
    name: versionWithLocale,
    children: [
      Key_main,
      Key_uninstall
    ]
  };
  var Key_name = {
    name: brandFullName,
    values: [
      "CurrentVersion", versionWithLocale
    ],
    children: [
      Key_versionWithLocale
    ]
  };
  var Key_brand = {
    name: vendorShortName,
    children: [
      Key_name,
      Key_nameWithVersion
    ]
  };

  try {
    key.open(rootKey, "SOFTWARE", key.ACCESS_READ);
    createRegistryKeys(key, Key_brand);
  } finally {
    key.close();
  }

  if (rootKey != RegKey.prototype.ROOT_KEY_LOCAL_MACHINE)
    return;

  // Now, do the same thing for the Add/Remove Programs control panel:

  const uninstallRoot =
      "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

  try {
    key.open(rootKey, uninstallRoot, key.ACCESS_READ);
    var oldName = oldInstall.fullName + " (" + oldInstall.version + ")";
    deleteRegistryKey(key, oldName, false);
  } finally {
    key.close();
  }

  var uninstallBundle = sbs.createBundle(URI_UNINSTALL_PROPERTIES);

  var nameWithVersion = brandFullName + " (" + app.version + ")";
  var uninstaller = getFile(KEY_WINDIR);
  uninstaller.append(uninstallBundle.GetStringFromName("fileUninstall"));
  // XXX copy latest uninstaller to this location

  var uninstallString =
      uninstaller.path + " /ua \"" + versionWithLocale + "\"";

  Key_uninstall = {
    name: nameWithVersion,
    values: [
      "Comment", brandFullName,
      "DisplayIcon", pathToExe + ",0",        // XXX don't hardcode me!
      "DisplayName", nameWithVersion,
      "DisplayVersion", versionWithLocale, 
      "InstallLocation", ourInstallDir.path,  // no trailing slash
      "Publisher", vendorShortName,
      "UninstallString", uninstallString,
      "URLInfoAbout", uninstallBundle.GetStringFromName("URLInfoAbout"),
      "URLUpdateInfo", uninstallBundle.GetStringFromName("URLUpdateInfo") 
    ]
  };

  var child;
  try {
    key.open(rootKey, uninstallRoot, key.ACCESS_READ);
    createRegistryKeys(key, Key_uninstall);

    // Create additional DWORD keys for NoModify and NoRepair:
    child = key.openChild(nameWithVersion, key.ACCESS_WRITE);
    child.writeIntValue("NoModify", 1);
    child.writeIntValue("NoRepair", 1);
  } finally {
    if (child)
      child.close();
    key.close();
  }
}

//-----------------------------------------------------------------------------

function nsPostUpdateWin() {
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
      // We use a global object here for the install log writer, so that the
      // registry updating code can easily inform it of registry keys that it
      // may create.
      installLogWriter = new InstallLogWriter();
      try {
        installLogWriter.begin();
        updateRegistry(RegKey.prototype.ROOT_KEY_CURRENT_USER);
        updateRegistry(RegKey.prototype.ROOT_KEY_LOCAL_MACHINE);
      } finally {
        installLogWriter.end();
        installLogWriter = null;
      }
    } catch (e) {
      LOG(e);
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
