/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/WebappOSUtils.jsm");

const LINUX = navigator.platform.startsWith("Linux");
const MAC = navigator.platform.startsWith("Mac");
const WIN = navigator.platform.startsWith("Win");
const MAC_106 = navigator.userAgent.contains("Mac OS X 10.6");

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

function TestAppInfo(aApp) {
  this.appProcess = Cc["@mozilla.org/process/util;1"].
                    createInstance(Ci.nsIProcess);

  this.isPackaged = !!aApp.updateManifest;

  if (LINUX) {
    this.installPath = OS.Path.join(OS.Constants.Path.homeDir,
                                    "." + WebappOSUtils.getUniqueName(aApp));
    this.exePath = OS.Path.join(this.installPath, "webapprt-stub");

    this.iconFile = OS.Path.join(this.installPath, "icon.png");

    let xdg_data_home = Cc["@mozilla.org/process/environment;1"].
                        getService(Ci.nsIEnvironment).
                        get("XDG_DATA_HOME");
    if (!xdg_data_home) {
      xdg_data_home = OS.Path.join(OS.Constants.Path.homeDir, ".local", "share");
    }

    let desktopINI = OS.Path.join(xdg_data_home, "applications",
                                  "owa-" + WebappOSUtils.getUniqueName(aApp) + ".desktop");

    this.installedFiles = [
      OS.Path.join(this.installPath, "webapp.json"),
      OS.Path.join(this.installPath, "webapp.ini"),
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
      OS.Path.join(this.installPath, "webapp.ini"),
      this.iconFile,
      desktopINI,
    ];

    if (this.isPackaged) {
      let appZipPath = OS.Path.join(this.installPath, "application.zip");
      this.installedFiles.push(appZipPath);
      this.tempUpdatedFiles.push(appZipPath);
      this.updatedFiles.push(appZipPath);
    }

    this.profilesIni = OS.Path.join(this.installPath, "profiles.ini");

    this.cleanup = Task.async(function*() {
      if (this.appProcess && this.appProcess.isRunning) {
        this.appProcess.kill();
      }

      if (this.profileDir) {
        yield OS.File.removeDir(this.profileDir.parent.path, { ignoreAbsent: true });
      }

      yield OS.File.removeDir(this.installPath, { ignoreAbsent: true });

      yield OS.File.remove(desktopINI, { ignoreAbsent: true });
    });
  } else if (WIN) {
    this.installPath = OS.Path.join(OS.Constants.Path.winAppDataDir,
                                    WebappOSUtils.getUniqueName(aApp));
    this.exePath = OS.Path.join(this.installPath, aApp.name + ".exe");

    this.iconFile = OS.Path.join(this.installPath, "chrome", "icons", "default", "default.ico");

    let desktopShortcut = OS.Path.join(OS.Constants.Path.desktopDir,
                                       aApp.name + ".lnk");
    let startMenuShortcut = OS.Path.join(OS.Constants.Path.winStartMenuProgsDir,
                                         aApp.name + ".lnk");

    this.installedFiles = [
      OS.Path.join(this.installPath, "webapp.json"),
      OS.Path.join(this.installPath, "webapp.ini"),
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
      OS.Path.join(this.installPath, "webapp.ini"),
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

    this.profilesIni = OS.Path.join(this.installPath, "profiles.ini");

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
        if (uninstallKey.hasChild(WebappOSUtils.getUniqueName(aApp))) {
          uninstallKey.removeChild(WebappOSUtils.getUniqueName(aApp));
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
          if (this.profileDir) {
            yield OS.File.removeDir(this.profileDir.parent.parent.path, { ignoreAbsent: true });
          }

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

    let appProfileDir = OS.Path.join(OS.Constants.Path.macUserLibDir,
                                     "Application Support",
                                     WebappOSUtils.getUniqueName(aApp));

    this.installedFiles = [
      OS.Path.join(this.installPath, "Contents", "Info.plist"),
      OS.Path.join(this.installPath, "Contents", "MacOS", "webapp.ini"),
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
      OS.Path.join(this.installPath, "Contents", "MacOS", "webapp.ini"),
      OS.Path.join(appProfileDir, "webapp.json"),
      this.iconFile,
    ];

    if (this.isPackaged) {
      let appZipPath = OS.Path.join(this.installPath, "Contents", "Resources", "application.zip");
      this.installedFiles.push(appZipPath);
      this.tempUpdatedFiles.push(appZipPath);
      this.updatedFiles.push(appZipPath);
    }

    this.profilesIni = OS.Path.join(appProfileDir, "profiles.ini");

    this.cleanup = Task.async(function*() {
      if (this.appProcess && this.appProcess.isRunning) {
        this.appProcess.kill();
      }

      if (this.profileDir) {
        yield OS.File.removeDir(this.profileDir.parent.path, { ignoreAbsent: true });
      }

      yield OS.File.removeDir(this.installPath, { ignoreAbsent: true });

      yield OS.File.removeDir(appProfileDir, { ignoreAbsent: true });
    });
  }
}

function buildAppPackage(aManifest, aIconFile) {
  let zipFile = getFile(OS.Constants.Path.profileDir, "sample.zip");

  let zipWriter = Cc["@mozilla.org/zipwriter;1"].createInstance(Ci.nsIZipWriter);
  zipWriter.open(zipFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
  zipWriter.addEntryFile("index.html",
                         Ci.nsIZipWriter.COMPRESSION_NONE,
                         getFile(getTestFilePath("data/app/index.html")),
                         false);

  var manStream = Cc["@mozilla.org/io/string-input-stream;1"].
                  createInstance(Ci.nsIStringInputStream);
  manStream.setData(aManifest, aManifest.length);
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

function wasAppSJSAccessed() {
  let deferred = Promise.defer();

  var xhr = new XMLHttpRequest();

  xhr.addEventListener("load", function() {
    let ret = (xhr.responseText == "done") ? true : false;
    deferred.resolve(ret);
  });

  xhr.addEventListener("error", aError => deferred.reject(aError));
  xhr.addEventListener("abort", aError => deferred.reject(aError));

  xhr.open('GET', 'http://test/chrome/toolkit/webapps/tests/app.sjs?testreq', true);
  xhr.send();

  return deferred.promise;
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
