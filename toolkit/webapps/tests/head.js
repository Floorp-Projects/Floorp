/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/WebappOSUtils.jsm");
Cu.import("resource://gre/modules/NativeApp.jsm");

const LINUX = navigator.platform.startsWith("Linux");
const MAC = navigator.platform.startsWith("Mac");
const WIN = navigator.platform.startsWith("Win");

const PR_RDWR        = 0x04;
const PR_CREATE_FILE = 0x08;
const PR_TRUNCATE    = 0x20;

function checkFiles(files) {
  return Task.spawn(function*() {
    for (let file of files) {
      if (!(yield OS.File.exists(file))) {
        info("File doesn't exist: " + file);
        return false;
      }
    }

    return true;
  });
}

function checkDateHigherThan(files, date) {
  return Task.spawn(function*() {
    for (let file of files) {
      if (!(yield OS.File.exists(file))) {
        info("File doesn't exist: " + file);
        return false;
      }

      let stat = yield OS.File.stat(file);
      if (!(stat.lastModificationDate > date)) {
        info("File not newer: " + file);
        return false;
      }
    }

    return true;
  });
}

function dirContainsOnly(dir, expectedFiles) {
  return Task.spawn(function*() {
    let iterator = new OS.File.DirectoryIterator(dir);

    let entries;
    try {
      entries = yield iterator.nextBatch();
    } finally {
      iterator.close();
    }

    let ret = true;

    // Find unexpected files
    for each (let {path} in entries) {
      if (expectedFiles.indexOf(path) == -1) {
        info("Unexpected file: " + path);
        ret = false;
      }
    }

    // Find missing files
    for each (let expectedPath in expectedFiles) {
      if (entries.findIndex(({path}) => path == expectedPath) == -1) {
        info("Missing file: " + expectedPath);
        ret = false;
      }
    }

    return ret;
  });
}

let dirSize = Task.async(function*(aDir) {
  let iterator = new OS.File.DirectoryIterator(aDir);

  let entries;
  try {
    entries = yield iterator.nextBatch();
  } finally {
    iterator.close();
  }

  let size = 0;

  for each (let entry in entries) {
    if (entry.isDir) {
      size += yield dirSize(entry.path);
    } else {
      let stat = yield OS.File.stat(entry.path);
      size += stat.size;
    }
  }

  return size;
});

function wait(time) {
  let deferred = Promise.defer();

  setTimeout(function() {
    deferred.resolve();
  }, time);

  return deferred.promise;
}

// Helper to create a nsIFile from a set of path components
function getFile() {
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(OS.Path.join.apply(OS.Path, arguments));
  return file;
}

function setDryRunPref() {
  let old_dry_run;
  try {
    old_dry_run = Services.prefs.getBoolPref("browser.mozApps.installer.dry_run");
  } catch (ex) {}

  Services.prefs.setBoolPref("browser.mozApps.installer.dry_run", false);

  SimpleTest.registerCleanupFunction(function() {
    if (old_dry_run === undefined) {
      Services.prefs.clearUserPref("browser.mozApps.installer.dry_run");
    } else {
      Services.prefs.setBoolPref("browser.mozApps.installer.dry_run", old_dry_run);
    }
  });
}

function TestAppInfo(aApp, aIsPackaged) {
  this.appProcess = Cc["@mozilla.org/process/util;1"].
                    createInstance(Ci.nsIProcess);

  this.isPackaged = aIsPackaged;

  this.uniqueName = WebappOSUtils.getUniqueName(aApp);

  if (LINUX) {
    this.installPath = OS.Path.join(OS.Constants.Path.homeDir,
                                    "." + this.uniqueName);
    this.exePath = OS.Path.join(this.installPath, "webapprt-stub");

    this.iconFile = OS.Path.join(this.installPath, "icon.png");

    this.webappINI = OS.Path.join(this.installPath, "webapp.ini");

    let xdg_data_home = Cc["@mozilla.org/process/environment;1"].
                        getService(Ci.nsIEnvironment).
                        get("XDG_DATA_HOME");
    if (!xdg_data_home) {
      xdg_data_home = OS.Path.join(OS.Constants.Path.homeDir, ".local", "share");
    }

    let desktopINI = OS.Path.join(xdg_data_home, "applications",
                                  "owa-" + this.uniqueName + ".desktop");

    this.installedFiles = [
      OS.Path.join(this.installPath, "webapp.json"),
      this.webappINI,
      this.iconFile,
      this.exePath,
      desktopINI,
    ];
    this.tempUpdatedFiles = [
      OS.Path.join(this.installPath, "update", "icon.png"),
      OS.Path.join(this.installPath, "update", "webapp.json"),
      OS.Path.join(this.installPath, "update", "webapp.ini"),
    ];
    this.updatedFiles = [
      OS.Path.join(this.installPath, "webapp.json"),
      this.webappINI,
      this.iconFile,
      desktopINI,
    ];

    if (this.isPackaged) {
      let appZipPath = OS.Path.join(this.installPath, "application.zip");
      this.installedFiles.push(appZipPath);
      this.tempUpdatedFiles.push(appZipPath);
      this.updatedFiles.push(appZipPath);
    }

    this.profileRoot = this.installPath;
    this.cacheRoot = OS.Path.join(OS.Constants.Path.homeDir, ".cache",
                                  this.uniqueName);

    this.cleanup = Task.async(function*() {
      if (this.appProcess && this.appProcess.isRunning) {
        this.appProcess.kill();
      }

      yield OS.File.removeDir(this.cacheRoot, { ignoreAbsent: true });

      yield OS.File.removeDir(this.profileRoot, { ignoreAbsent: true });

      yield OS.File.removeDir(this.installPath, { ignoreAbsent: true });

      yield OS.File.remove(desktopINI, { ignoreAbsent: true });
    });
  } else if (WIN) {
    this.installPath = OS.Path.join(OS.Constants.Path.winAppDataDir,
                                    this.uniqueName);
    this.exePath = OS.Path.join(this.installPath, aApp.name + ".exe");

    this.iconFile = OS.Path.join(this.installPath, "chrome", "icons", "default", "default.ico");

    this.webappINI = OS.Path.join(this.installPath, "webapp.ini");

    let desktopShortcut = OS.Path.join(OS.Constants.Path.desktopDir,
                                       aApp.name + ".lnk");
    let startMenuShortcut = OS.Path.join(OS.Constants.Path.winStartMenuProgsDir,
                                         aApp.name + ".lnk");

    this.installedFiles = [
      OS.Path.join(this.installPath, "webapp.json"),
      this.webappINI,
      OS.Path.join(this.installPath, "uninstall", "shortcuts_log.ini"),
      OS.Path.join(this.installPath, "uninstall", "uninstall.log"),
      OS.Path.join(this.installPath, "uninstall", "webapp-uninstaller.exe"),
      this.iconFile,
      this.exePath,
      desktopShortcut,
      startMenuShortcut,
    ];
    this.tempUpdatedFiles = [
      OS.Path.join(this.installPath, "update", "chrome", "icons", "default", "default.ico"),
      OS.Path.join(this.installPath, "update", "webapp.json"),
      OS.Path.join(this.installPath, "update", "webapp.ini"),
      OS.Path.join(this.installPath, "update", "uninstall", "shortcuts_log.ini"),
      OS.Path.join(this.installPath, "update", "uninstall", "uninstall.log"),
      OS.Path.join(this.installPath, "update", "uninstall", "webapp-uninstaller.exe"),
    ];
    this.updatedFiles = [
      OS.Path.join(this.installPath, "webapp.json"),
      this.webappINI,
      OS.Path.join(this.installPath, "uninstall", "shortcuts_log.ini"),
      OS.Path.join(this.installPath, "uninstall", "uninstall.log"),
      this.iconFile,
      desktopShortcut,
      startMenuShortcut,
    ];

    if (this.isPackaged) {
      let appZipPath = OS.Path.join(this.installPath, "application.zip");
      this.installedFiles.push(appZipPath);
      this.tempUpdatedFiles.push(appZipPath);
      this.updatedFiles.push(appZipPath);
    }

    this.profileRoot = this.installPath;
    this.cacheRoot = OS.Path.join(Services.dirsvc.get("LocalAppData", Ci.nsIFile).path,
                                  this.uniqueName);

    this.cleanup = Task.async(function*() {
      if (this.appProcess && this.appProcess.isRunning) {
        this.appProcess.kill();
      }

      let uninstallKey;
      try {
        uninstallKey = Cc["@mozilla.org/windows-registry-key;1"].
                       createInstance(Ci.nsIWindowsRegKey);
        uninstallKey.open(uninstallKey.ROOT_KEY_CURRENT_USER,
                          "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                          uninstallKey.ACCESS_WRITE);
        if (uninstallKey.hasChild(this.uniqueName)) {
          uninstallKey.removeChild(this.uniqueName);
        }
      } catch (e) {
      } finally {
        if (uninstallKey) {
          uninstallKey.close();
        }
      }

      let removed = false;
      do {
        try {
          yield OS.File.removeDir(this.cacheRoot, { ignoreAbsent: true });

          yield OS.File.removeDir(this.profileRoot, { ignoreAbsent: true });

          yield OS.File.removeDir(this.installPath, { ignoreAbsent: true });

          yield OS.File.remove(desktopShortcut, { ignoreAbsent: true });
          yield OS.File.remove(startMenuShortcut, { ignoreAbsent: true });

          removed = true;
        } catch (ex if ex instanceof OS.File.Error &&
                 (ex.winLastError == OS.Constants.Win.ERROR_ACCESS_DENIED ||
                  ex.winLastError == OS.Constants.Win.ERROR_SHARING_VIOLATION ||
                  ex.winLastError == OS.Constants.Win.ERROR_DIR_NOT_EMPTY)) {
          // Wait 100 ms before attempting to remove again.
          yield wait(100);
        }
      } while (!removed);
    });
  } else if (MAC) {
    this.installPath = OS.Path.join(OS.Constants.Path.homeDir,
                                    "Applications",
                                    aApp.name + ".app");
    this.exePath = OS.Path.join(this.installPath, "Contents", "MacOS", "webapprt");

    this.iconFile = OS.Path.join(this.installPath, "Contents", "Resources", "appicon.icns");

    this.webappINI = OS.Path.join(this.installPath, "Contents", "MacOS", "webapp.ini");

    let appProfileDir = OS.Path.join(OS.Constants.Path.macUserLibDir,
                                     "Application Support",
                                     this.uniqueName);

    this.installedFiles = [
      OS.Path.join(this.installPath, "Contents", "Info.plist"),
      this.webappINI,
      OS.Path.join(appProfileDir, "webapp.json"),
      this.iconFile,
      this.exePath,
    ];
    this.tempUpdatedFiles = [
      OS.Path.join(this.installPath, "update", "Contents", "Info.plist"),
      OS.Path.join(this.installPath, "update", "Contents", "MacOS", "webapp.ini"),
      OS.Path.join(this.installPath, "update", "Contents", "Resources", "appicon.icns"),
      OS.Path.join(this.installPath, "update", "webapp.json")
    ];
    this.updatedFiles = [
      OS.Path.join(this.installPath, "Contents", "Info.plist"),
      this.webappINI,
      OS.Path.join(appProfileDir, "webapp.json"),
      this.iconFile,
    ];

    if (this.isPackaged) {
      let appZipPath = OS.Path.join(this.installPath, "Contents", "Resources", "application.zip");
      this.installedFiles.push(appZipPath);
      this.tempUpdatedFiles.push(appZipPath);
      this.updatedFiles.push(appZipPath);
    }

    this.profileRoot = appProfileDir;
    this.cacheRoot = OS.Path.join(OS.Constants.Path.macUserLibDir, "Caches",
                                  this.uniqueName);

    this.cleanup = Task.async(function*() {
      if (this.appProcess && this.appProcess.isRunning) {
        this.appProcess.kill();
      }

      yield OS.File.removeDir(this.cacheRoot, { ignoreAbsent: true });

      yield OS.File.removeDir(this.profileRoot, { ignoreAbsent: true });

      if (this.trashDir) {
        yield OS.File.removeDir(this.trashDir, { ignoreAbsent: true });
      }

      yield OS.File.removeDir(this.installPath, { ignoreAbsent: true });

      yield OS.File.removeDir(appProfileDir, { ignoreAbsent: true });
    });
  }

  this.profilesIni = OS.Path.join(this.profileRoot, "profiles.ini");

  let profileDir;

  Object.defineProperty(this, "profileDir", {
    get: function() {
      if (!profileDir && this.profileRelPath) {
        return getFile.apply(null, [this.profileRoot].concat(this.profileRelPath.split("/")));
      }

      return profileDir;
    },
    set: function(aVal) {
      profileDir = aVal;
    },
  });

  Object.defineProperty(this, "cacheDir", {
    get: function() {
      if (!this.profileRelPath) {
        return null;
      }

      return getFile.apply(null, [this.cacheRoot].concat(this.profileRelPath.split("/")));
    },
  });

  Object.defineProperty(this, "profileRelPath", {
    get: function() {
      // If the profileDir was set by someone else, use its leafName
      // as the profile name.
      if (profileDir) {
        return profileDir.leafName;
      }

      // Otherwise, read profiles.ini to get the profile directory
      try {
        let iniParser = Cc["@mozilla.org/xpcom/ini-processor-factory;1"].
                        getService(Ci.nsIINIParserFactory).
                        createINIParser(getFile(this.profilesIni));
        return iniParser.getString("Profile0", "Path");
      } catch (e) {
        return null;
      }
    }
  });
}

function buildAppPackage(aManifest, aIconFile) {
  let zipFile = getFile(OS.Constants.Path.profileDir, "sample.zip");

  let zipWriter = Cc["@mozilla.org/zipwriter;1"].createInstance(Ci.nsIZipWriter);
  zipWriter.open(zipFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
  zipWriter.addEntryFile("index.html",
                         Ci.nsIZipWriter.COMPRESSION_NONE,
                         getFile(getTestFilePath("data/app/index.html")),
                         false);

  let manifestJSON = JSON.stringify(aManifest);
  let manStream = Cc["@mozilla.org/io/string-input-stream;1"].
                  createInstance(Ci.nsIStringInputStream);
  manStream.setData(manifestJSON, manifestJSON.length);
  zipWriter.addEntryStream("manifest.webapp", Date.now(),
                           Ci.nsIZipWriter.COMPRESSION_NONE,
                           manStream, false);

  if (aIconFile) {
    zipWriter.addEntryFile(aIconFile.leafName,
                           Ci.nsIZipWriter.COMPRESSION_NONE,
                           aIconFile,
                           false);
  }

  zipWriter.close();

  return zipFile.path;
}

function xhrRequest(aQueryString) {
  let deferred = Promise.defer();

  var xhr = new XMLHttpRequest();

  xhr.addEventListener("load", function() {
    deferred.resolve(xhr.responseText);
  });

  xhr.addEventListener("error", aError => deferred.reject(aError));
  xhr.addEventListener("abort", aError => deferred.reject(aError));

  xhr.open('GET', 'http://test/chrome/toolkit/webapps/tests/app.sjs' + aQueryString, true);
  xhr.send();

  return deferred.promise;
}

function wasAppSJSAccessed() {
  return xhrRequest('?testreq').then((aResponseText) => {
    return (aResponseText == 'done') ? true : false;
  });
}

function setState(aVar, aState) {
  return xhrRequest('?set' + aVar + '=' + aState).then((aResponseText) => {
    is(aResponseText, "OK", "set" + aVar + " OK");
  });
}

function getState(aVar) {
  return xhrRequest('?get' + aVar);
}

function generateDataURI(aFile) {
  var contentType = Cc["@mozilla.org/mime;1"].
                    getService(Ci.nsIMIMEService).
                    getTypeFromFile(aFile);

  var inputStream = Cc["@mozilla.org/network/file-input-stream;1"].
                    createInstance(Ci.nsIFileInputStream);
  inputStream.init(aFile, -1, -1, Ci.nsIFileInputStream.CLOSE_ON_EOF);

  var stream = Cc["@mozilla.org/binaryinputstream;1"].
               createInstance(Ci.nsIBinaryInputStream);
  stream.setInputStream(inputStream);

  return "data:" + contentType + ";base64," +
         btoa(stream.readBytes(stream.available()));
}

function confirmNextInstall() {
  let popupPanel = window.top.QueryInterface(Ci.nsIInterfaceRequestor).
                              getInterface(Ci.nsIWebNavigation).
                              QueryInterface(Ci.nsIDocShell).
                              chromeEventHandler.ownerDocument.defaultView.
                              PopupNotifications.panel;

  popupPanel.addEventListener("popupshown", function onPopupShown() {
    popupPanel.removeEventListener("popupshown", onPopupShown, false);
    this.childNodes[0].button.doCommand();
  }, false);
}

let readJSON = Task.async(function*(aPath) {
  let decoder = new TextDecoder();
  let data = yield OS.File.read(aPath);
  return JSON.parse(decoder.decode(data));
});

let setMacRootInstallDir = Task.async(function*(aPath) {
  let oldRootInstallDir = NativeApp.prototype._rootInstallDir;

  NativeApp.prototype._rootInstallDir = OS.Path.join(OS.Constants.Path.homeDir,
                                                     "Applications");
  yield OS.File.makeDir(NativeApp.prototype._rootInstallDir,
                        { ignoreExisting: true });

  SimpleTest.registerCleanupFunction(function() {
    NativeApp.prototype._rootInstallDir = oldRootInstallDir;
  });
});

let writeToFile = Task.async(function*(aPath, aData) {
  let data = new TextEncoder().encode(aData);

  let file;
  try {
    file = yield OS.File.open(aPath, { truncate: true, write: true }, { unixMode: 0o777 });
    yield file.write(data);
  } finally {
    yield file.close();
  }
});

// We need to mock the Alerts service, otherwise the alert that is shown
// at the end of an installation makes the test leak the app's icon.

const CID = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID();
const ALERTS_SERVICE_CONTRACT_ID = "@mozilla.org/alerts-service;1";
const ALERTS_SERVICE_CID = Components.ID(Cc[ALERTS_SERVICE_CONTRACT_ID].number);

let AlertsService = {
  classID: Components.ID(CID),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory,
                                         Ci.nsIAlertsService]),

  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }

    return this.QueryInterface(aIID);
  },

  init: function() {
    Components.manager.nsIComponentRegistrar.registerFactory(this.classID,
      "", ALERTS_SERVICE_CONTRACT_ID, this);
  },

  restore: function() {
    Components.manager.nsIComponentRegistrar.registerFactory(ALERTS_SERVICE_CID,
      "", ALERTS_SERVICE_CONTRACT_ID, null);
  },

  showAlertNotification: function() {
  },
};

AlertsService.init();

SimpleTest.registerCleanupFunction(() => {
  AlertsService.restore();
});
