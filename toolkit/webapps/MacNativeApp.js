/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const USER_LIB_DIR = OS.Constants.Path.macUserLibDir;
const LOCAL_APP_DIR = OS.Constants.Path.macLocalApplicationsDir;

/**
 * Constructor for the Mac native app shell
 *
 * @param aApp {Object} the app object provided to the install function
 * @param aManifest {Object} the manifest data provided by the web app
 * @param aCategories {Array} array of app categories
 * @param aRegistryDir {String} (optional) path to the registry
 */
function NativeApp(aApp, aManifest, aCategories, aRegistryDir) {
  CommonNativeApp.call(this, aApp, aManifest, aCategories, aRegistryDir);

  // The ${ProfileDir} is: sanitized app name + "-" + manifest url hash
  this.appProfileDir = OS.Path.join(USER_LIB_DIR, "Application Support",
                                    this.uniqueName);
  this.configJson = "webapp.json";

  this.contentsDir = "Contents";
  this.macOSDir = OS.Path.join(this.contentsDir, "MacOS");
  this.resourcesDir = OS.Path.join(this.contentsDir, "Resources");
  this.iconFile = OS.Path.join(this.resourcesDir, "appicon.icns");
  this.zipFile = OS.Path.join(this.resourcesDir, "application.zip");
}

NativeApp.prototype = {
  __proto__: CommonNativeApp.prototype,
  /*
   * The _rootInstallDir property is the path of the directory where we install
   * apps. In production code, it's "/Applications". In tests, it's
   * "~/Applications" because on build machines we don't have enough privileges
   * to write to the global "/Applications" directory.
   */
  _rootInstallDir: LOCAL_APP_DIR,

  /**
   * Creates a native installation of the web app in the OS
   *
   * @param aManifest {Object} the manifest data provided by the web app
   * @param aZipPath {String} path to the zip file for packaged apps (undefined
   *                          for hosted apps)
   */
  install: Task.async(function*(aApp, aManifest, aZipPath) {
    if (this._dryRun) {
      return;
    }

    // If the application is already installed, this is a reinstallation.
    if (WebappOSUtils.getInstallPath(aApp)) {
      return yield this.prepareUpdate(aApp, aManifest, aZipPath);
    }

    this._setData(aApp, aManifest);

    let localAppDir = getFile(this._rootInstallDir);
    if (!localAppDir.isWritable()) {
      throw("Not enough privileges to install apps");
    }

    let destinationName = yield getAvailableFileName([ this._rootInstallDir ],
                                                     this.appNameAsFilename,
                                                     ".app");

    let installDir = OS.Path.join(this._rootInstallDir, destinationName);

    let dir = getFile(TMP_DIR, this.appNameAsFilename + ".app");
    dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
    let tmpDir = dir.path;

    try {
      yield this._createDirectoryStructure(tmpDir);
      this._copyPrebuiltFiles(tmpDir);
      yield this._createConfigFiles(tmpDir);

      if (aZipPath) {
        yield OS.File.move(aZipPath, OS.Path.join(tmpDir, this.zipFile));
      }

      yield this._getIcon(tmpDir);
    } catch (ex) {
      yield OS.File.removeDir(tmpDir, { ignoreAbsent: true });
      throw ex;
    }

    this._removeInstallation(true, installDir);

    try {
      // Move the temp installation directory to the /Applications directory
      yield this._applyTempInstallation(tmpDir, installDir);
    } catch (ex) {
      this._removeInstallation(false, installDir);
      yield OS.File.removeDir(tmpDir, { ignoreAbsent: true });
      throw ex;
    }
  }),

  /**
   * Creates an update in a temporary directory to be applied later.
   *
   * @param aManifest {Object} the manifest data provided by the web app
   * @param aZipPath {String} path to the zip file for packaged apps (undefined
   *                          for hosted apps)
   */
  prepareUpdate: Task.async(function*(aApp, aManifest, aZipPath) {
    if (this._dryRun) {
      return;
    }

    this._setData(aApp, aManifest);

    let [ oldUniqueName, installDir ] = WebappOSUtils.getLaunchTarget(aApp);
    if (!installDir) {
      throw ERR_NOT_INSTALLED;
    }

    if (this.uniqueName != oldUniqueName) {
      // Bug 919799: If the app is still in the registry, migrate its data to
      // the new format.
      throw ERR_UPDATES_UNSUPPORTED_OLD_NAMING_SCHEME;
    }

    let updateDir = OS.Path.join(installDir, "update");
    yield OS.File.removeDir(updateDir, { ignoreAbsent: true });
    yield OS.File.makeDir(updateDir);

    try {
      yield this._createDirectoryStructure(updateDir);
      this._copyPrebuiltFiles(updateDir);
      yield this._createConfigFiles(updateDir);

      if (aZipPath) {
        yield OS.File.move(aZipPath, OS.Path.join(updateDir, this.zipFile));
      }

      yield this._getIcon(updateDir);
    } catch (ex) {
      yield OS.File.removeDir(updateDir, { ignoreAbsent: true });
      throw ex;
    }
  }),

  /**
   * Applies an update.
   */
  applyUpdate: Task.async(function*(aApp) {
    if (this._dryRun) {
      return;
    }

    let installDir = WebappOSUtils.getInstallPath(aApp);
    let updateDir = OS.Path.join(installDir, "update");

    let backupDir = yield this._backupInstallation(installDir);

    try {
      // Move the update directory to the /Applications directory
      yield this._applyTempInstallation(updateDir, installDir);
    } catch (ex) {
      yield this._restoreInstallation(backupDir, installDir);
      throw ex;
    } finally {
      yield OS.File.removeDir(backupDir, { ignoreAbsent: true });
      yield OS.File.removeDir(updateDir, { ignoreAbsent: true });
    }
  }),

  _applyTempInstallation: Task.async(function*(aTmpDir, aInstallDir) {
    yield OS.File.move(OS.Path.join(aTmpDir, this.configJson),
                       OS.Path.join(this.appProfileDir, this.configJson));

    yield moveDirectory(aTmpDir, aInstallDir);
  }),

  _removeInstallation: function(keepProfile, aInstallDir) {
    let filesToRemove = [ aInstallDir ];

    if (!keepProfile) {
      filesToRemove.push(this.appProfileDir);
    }

    removeFiles(filesToRemove);
  },

  _backupInstallation: Task.async(function*(aInstallDir) {
    let backupDir = OS.Path.join(aInstallDir, "backup");
    yield OS.File.removeDir(backupDir, { ignoreAbsent: true });
    yield OS.File.makeDir(backupDir);

    yield moveDirectory(OS.Path.join(aInstallDir, this.contentsDir),
                        backupDir);
    yield OS.File.move(OS.Path.join(this.appProfileDir, this.configJson),
                       OS.Path.join(backupDir, this.configJson));

    return backupDir;
  }),

  _restoreInstallation: Task.async(function*(aBackupDir, aInstallDir) {
    yield OS.File.move(OS.Path.join(aBackupDir, this.configJson),
                       OS.Path.join(this.appProfileDir, this.configJson));
    yield moveDirectory(aBackupDir,
                        OS.Path.join(aInstallDir, this.contentsDir));
  }),

  _createDirectoryStructure: Task.async(function*(aDir) {
    yield OS.File.makeDir(this.appProfileDir,
                          { unixMode: PERMS_DIRECTORY, ignoreExisting: true });

    yield OS.File.makeDir(OS.Path.join(aDir, this.contentsDir),
                          { unixMode: PERMS_DIRECTORY, ignoreExisting: true });

    yield OS.File.makeDir(OS.Path.join(aDir, this.macOSDir),
                          { unixMode: PERMS_DIRECTORY, ignoreExisting: true });

    yield OS.File.makeDir(OS.Path.join(aDir, this.resourcesDir),
                          { unixMode: PERMS_DIRECTORY, ignoreExisting: true });
  }),

  _copyPrebuiltFiles: function(aDir) {
    let destDir = getFile(aDir, this.macOSDir);
    let stub = getFile(this.runtimeFolder, "webapprt-stub");
    stub.copyTo(destDir, "webapprt");
  },

  _createConfigFiles: function(aDir) {
    // ${ProfileDir}/webapp.json
    yield writeToFile(OS.Path.join(aDir, this.configJson),
                      JSON.stringify(this.webappJson));

    // ${InstallDir}/Contents/MacOS/webapp.ini
    let applicationINI = getFile(aDir, this.macOSDir, "webapp.ini");

    let writer = Cc["@mozilla.org/xpcom/ini-processor-factory;1"].
                 getService(Ci.nsIINIParserFactory).
                 createINIParser(applicationINI).
                 QueryInterface(Ci.nsIINIParserWriter);
    writer.setString("Webapp", "Name", this.appLocalizedName);
    writer.setString("Webapp", "Profile", this.uniqueName);
    writer.writeFile();
    applicationINI.permissions = PERMS_FILE;

    // ${InstallDir}/Contents/Info.plist
    let infoPListContent = '<?xml version="1.0" encoding="UTF-8"?>\n\
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">\n\
<plist version="1.0">\n\
  <dict>\n\
    <key>CFBundleDevelopmentRegion</key>\n\
    <string>English</string>\n\
    <key>CFBundleDisplayName</key>\n\
    <string>' + escapeXML(this.appLocalizedName) + '</string>\n\
    <key>CFBundleExecutable</key>\n\
    <string>webapprt</string>\n\
    <key>CFBundleIconFile</key>\n\
    <string>appicon</string>\n\
    <key>CFBundleIdentifier</key>\n\
    <string>' + escapeXML(this.uniqueName) + '</string>\n\
    <key>CFBundleInfoDictionaryVersion</key>\n\
    <string>6.0</string>\n\
    <key>CFBundleName</key>\n\
    <string>' + escapeXML(this.appLocalizedName) + '</string>\n\
    <key>CFBundlePackageType</key>\n\
    <string>APPL</string>\n\
    <key>CFBundleVersion</key>\n\
    <string>0</string>\n\
    <key>NSHighResolutionCapable</key>\n\
    <true/>\n\
    <key>NSPrincipalClass</key>\n\
    <string>GeckoNSApplication</string>\n\
    <key>FirefoxBinary</key>\n\
#expand     <string>__MOZ_MACBUNDLE_ID__</string>\n\
  </dict>\n\
</plist>';

    yield writeToFile(OS.Path.join(aDir, this.contentsDir, "Info.plist"),
                      infoPListContent);
  },

  /**
   * Process the icon from the imageStream as retrieved from
   * the URL by getIconForApp(). This will bundle the icon to the
   * app package at Contents/Resources/appicon.icns.
   *
   * @param aMimeType     the icon mimetype
   * @param aImageStream  the stream for the image data
   * @param aDir          the directory where the icon should be stored
   */
  _processIcon: function(aMimeType, aIcon, aDir) {
    let deferred = Promise.defer();

    let tmpIconPath = OS.Path.join(aDir, this.iconFile);

    function conversionDone(aSubject, aTopic) {
      if (aTopic != "process-finished") {
        deferred.reject("Failure converting icon, exit code: " + aSubject.exitValue);
        return;
      }

      // SIPS silently fails to convert the icon, so we need to verify if the
      // icon was successfully converted.
      OS.File.exists(tmpIconPath).then((aExists) => {
        if (aExists) {
          deferred.resolve();
        } else {
          deferred.reject("Failure converting icon, unrecognized image format");
        }
      });
    }

    let process = Cc["@mozilla.org/process/util;1"].
                  createInstance(Ci.nsIProcess);
    let sipsFile = getFile("/usr/bin/sips");

    process.init(sipsFile);
    process.runAsync(["-s", "format", "icns",
                      aIcon.path,
                      "--out", tmpIconPath,
                      "-z", "128", "128"],
                      9, conversionDone);

    return deferred.promise;
  }
}
