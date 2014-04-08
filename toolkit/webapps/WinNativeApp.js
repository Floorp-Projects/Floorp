/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const PROGS_DIR = OS.Constants.Path.winStartMenuProgsDir;
const APP_DATA_DIR = OS.Constants.Path.winAppDataDir;

/*************************************
 * Windows app installer
 *
 * The Windows installation process will generate the following files:
 *
 * ${FolderName} = sanitized app name + "-" + manifest url hash
 *
 * %APPDATA%/${FolderName}
 *   - webapp.ini
 *   - webapp.json
 *   - ${AppName}.exe
 *   - ${AppName}.lnk
 *   / uninstall
 *     - webapp-uninstaller.exe
 *     - shortcuts_log.ini
 *     - uninstall.log
 *   / chrome/icons/default/
 *     - default.ico
 *
 * After the app runs for the first time, a profiles/ folder will also be
 * created which will host the user profile for this app.
 */

/**
 * Constructor for the Windows native app shell
 *
 * @param aApp {Object} the app object provided to the install function
 * @param aManifest {Object} the manifest data provided by the web app
 * @param aCategories {Array} array of app categories
 * @param aRegistryDir {String} (optional) path to the registry
 */
function NativeApp(aApp, aManifest, aCategories, aRegistryDir) {
  CommonNativeApp.call(this, aApp, aManifest, aCategories, aRegistryDir);

  if (this.isPackaged) {
    this.size = aApp.updateManifest.size / 1024;
  }

  this.webapprt = this.appNameAsFilename + ".exe";
  this.configJson = "webapp.json";
  this.webappINI = "webapp.ini";
  this.iconPath = OS.Path.join("chrome", "icons", "default", "default.ico");
  this.uninstallDir = "uninstall";
  this.uninstallerFile = OS.Path.join(this.uninstallDir,
                                      "webapp-uninstaller.exe");
  this.shortcutLogsINI = OS.Path.join(this.uninstallDir, "shortcuts_log.ini");
  this.zipFile = "application.zip";

  this.backupFiles = [ "chrome", this.configJson, this.webappINI, "uninstall" ];
  if (this.isPackaged) {
    this.backupFiles.push(this.zipFile);
  }

  this.uninstallSubkeyStr = this.uniqueName;
}

NativeApp.prototype = {
  __proto__: CommonNativeApp.prototype,
  size: null,

  /**
   * Creates a native installation of the web app in the OS
   *
   * @param aManifest {Object} the manifest data provided by the web app
   * @param aZipPath {String} path to the zip file for packaged apps (undefined
   *                          for hosted apps)
   */
  install: Task.async(function*(aManifest, aZipPath) {
    if (this._dryRun) {
      return;
    }

    // If the application is already installed, this is a reinstallation.
    if (WebappOSUtils.getInstallPath(this.app)) {
      return yield this.prepareUpdate(aManifest, aZipPath);
    }

    this._setData(aManifest);

    let installDir = OS.Path.join(APP_DATA_DIR, this.uniqueName);

    // Create a temporary installation directory.
    let dir = getFile(TMP_DIR, this.uniqueName);
    dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
    let tmpDir = dir.path;

    // Perform the installation in the temp directory.
    try {
      yield this._createDirectoryStructure(tmpDir);
      yield this._getShortcutName(installDir);
      yield this._copyWebapprt(tmpDir);
      yield this._copyUninstaller(tmpDir);
      yield this._createConfigFiles(tmpDir);

      if (aZipPath) {
        yield OS.File.move(aZipPath, OS.Path.join(tmpDir, this.zipFile));
      }

      yield this._getIcon(tmpDir);
    } catch (ex) {
      yield OS.File.removeDir(tmpDir, { ignoreAbsent: true });
      throw ex;
    }

    // Apply the installation.
    this._removeInstallation(true, installDir);

    try {
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
  prepareUpdate: Task.async(function*(aManifest, aZipPath) {
    if (this._dryRun) {
      return;
    }

    this._setData(aManifest);

    let installDir = WebappOSUtils.getInstallPath(this.app);
    if (!installDir) {
      throw ERR_NOT_INSTALLED;
    }

    if (this.uniqueName != OS.Path.basename(installDir)) {
      // Bug 919799: If the app is still in the registry, migrate its data to
      // the new format.
      throw ERR_UPDATES_UNSUPPORTED_OLD_NAMING_SCHEME;
    }

    let updateDir = OS.Path.join(installDir, "update");
    yield OS.File.removeDir(updateDir, { ignoreAbsent: true });
    yield OS.File.makeDir(updateDir);

    // Perform the update in the "update" subdirectory.
    try {
      yield this._createDirectoryStructure(updateDir);
      yield this._getShortcutName(installDir);
      yield this._copyUninstaller(updateDir);
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
  applyUpdate: Task.async(function*() {
    if (this._dryRun) {
      return;
    }

    let installDir = WebappOSUtils.getInstallPath(this.app);
    let updateDir = OS.Path.join(installDir, "update");

    yield this._getShortcutName(installDir);

    let backupDir = yield this._backupInstallation(installDir);

    try {
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
    yield moveDirectory(aTmpDir, aInstallDir);

    this._createShortcutFiles(aInstallDir);
    this._writeSystemKeys(aInstallDir);
  }),

  _getShortcutName: Task.async(function*(aInstallDir) {
    let shortcutLogsINIfile = getFile(aInstallDir, this.shortcutLogsINI);

    if (shortcutLogsINIfile.exists()) {
      // If it's a reinstallation (or an update) get the shortcut names
      // from the shortcut_log.ini file
      let parser = Cc["@mozilla.org/xpcom/ini-processor-factory;1"].
                   getService(Ci.nsIINIParserFactory).
                   createINIParser(shortcutLogsINIfile);
      this.shortcutName = parser.getString("STARTMENU", "Shortcut0");
    } else {
      // Check in both directories to see if a shortcut with the same name
      // already exists.
      this.shortcutName = yield getAvailableFileName([ PROGS_DIR, DESKTOP_DIR ],
                                                     this.appNameAsFilename,
                                                     ".lnk");
    }
  }),

  /**
   * Remove the current installation
   */
  _removeInstallation: function(keepProfile, aInstallDir) {
    let uninstallKey;
    try {
      uninstallKey = Cc["@mozilla.org/windows-registry-key;1"].
                     createInstance(Ci.nsIWindowsRegKey);
      uninstallKey.open(uninstallKey.ROOT_KEY_CURRENT_USER,
                        "SOFTWARE\\Microsoft\\Windows\\" +
                        "CurrentVersion\\Uninstall",
                        uninstallKey.ACCESS_WRITE);
      if (uninstallKey.hasChild(this.uninstallSubkeyStr)) {
        uninstallKey.removeChild(this.uninstallSubkeyStr);
      }
    } catch (e) {
    } finally {
      if (uninstallKey) {
        uninstallKey.close();
      }
    }

    let filesToRemove = [ OS.Path.join(DESKTOP_DIR, this.shortcutName),
                          OS.Path.join(PROGS_DIR, this.shortcutName) ];

    if (keepProfile) {
      for (let filePath of this.backupFiles) {
        filesToRemove.push(OS.Path.join(aInstallDir, filePath));
      }

      filesToRemove.push(OS.Path.join(aInstallDir, this.webapprt));
    } else {
      filesToRemove.push(aInstallDir);
    }

    removeFiles(filesToRemove);
  },

  _backupInstallation: Task.async(function*(aInstallDir) {
    let backupDir = OS.Path.join(aInstallDir, "backup");
    yield OS.File.removeDir(backupDir, { ignoreAbsent: true });
    yield OS.File.makeDir(backupDir);

    for (let filePath of this.backupFiles) {
      yield OS.File.move(OS.Path.join(aInstallDir, filePath),
                         OS.Path.join(backupDir, filePath));
    }

    return backupDir;
  }),

  _restoreInstallation: function(aBackupDir, aInstallDir) {
    return moveDirectory(aBackupDir, aInstallDir);
  },

  /**
   * Creates the main directory structure.
   */
  _createDirectoryStructure: Task.async(function*(aDir) {
    yield OS.File.makeDir(OS.Path.join(aDir, this.uninstallDir));

    yield OS.File.makeDir(OS.Path.join(aDir, OS.Path.dirname(this.iconPath)),
                          { from: aDir });
  }),

  /**
   * Copy the webrt executable into the installation directory.
   */
  _copyWebapprt: function(aDir) {
    return OS.File.copy(OS.Path.join(this.runtimeFolder, "webapprt-stub.exe"),
                        OS.Path.join(aDir, this.webapprt));
  },

  /**
   * Copy the uninstaller executable into the installation directory.
   */
  _copyUninstaller: function(aDir) {
    return OS.File.copy(OS.Path.join(this.runtimeFolder, "webapp-uninstaller.exe"),
                        OS.Path.join(aDir, this.uninstallerFile));
  },

  /**
   * Creates the configuration files into their destination folders.
   */
  _createConfigFiles: function(aDir) {
    // ${InstallDir}/webapp.json
    yield writeToFile(OS.Path.join(aDir, this.configJson),
                      JSON.stringify(this.webappJson));

    let factory = Cc["@mozilla.org/xpcom/ini-processor-factory;1"].
                  getService(Ci.nsIINIParserFactory);

    // ${InstallDir}/webapp.ini
    let webappINIfile = getFile(aDir, this.webappINI);

    let writer = factory.createINIParser(webappINIfile)
                        .QueryInterface(Ci.nsIINIParserWriter);
    writer.setString("Webapp", "Name", this.appName);
    writer.setString("Webapp", "Profile", this.uniqueName);
    writer.setString("Webapp", "Executable", this.appNameAsFilename);
    writer.setString("WebappRT", "InstallDir", this.runtimeFolder);
    writer.writeFile(null, Ci.nsIINIParserWriter.WRITE_UTF16);

    let shortcutLogsINIfile = getFile(aDir, this.shortcutLogsINI);

    writer = factory.createINIParser(shortcutLogsINIfile)
                    .QueryInterface(Ci.nsIINIParserWriter);
    writer.setString("STARTMENU", "Shortcut0", this.shortcutName);
    writer.setString("DESKTOP", "Shortcut0", this.shortcutName);
    writer.setString("TASKBAR", "Migrated", "true");
    writer.writeFile(null, Ci.nsIINIParserWriter.WRITE_UTF16);

    // ${UninstallDir}/uninstall.log
    let uninstallContent =
      "File: \\webapp.ini\r\n" +
      "File: \\webapp.json\r\n" +
      "File: \\webapprt.old\r\n" +
      "File: \\chrome\\icons\\default\\default.ico";
    if (this.isPackaged) {
      uninstallContent += "\r\nFile: \\application.zip";
    }

    yield writeToFile(OS.Path.join(aDir, this.uninstallDir, "uninstall.log"),
                      uninstallContent);
  },

  /**
   * Writes the keys to the system registry that are necessary for the app
   * operation and uninstall process.
   */
  _writeSystemKeys: function(aInstallDir) {
    let parentKey;
    let uninstallKey;
    let subKey;

    try {
      parentKey = Cc["@mozilla.org/windows-registry-key;1"].
                  createInstance(Ci.nsIWindowsRegKey);
      parentKey.open(parentKey.ROOT_KEY_CURRENT_USER,
                     "SOFTWARE\\Microsoft\\Windows\\CurrentVersion",
                     parentKey.ACCESS_WRITE);
      uninstallKey = parentKey.createChild("Uninstall", parentKey.ACCESS_WRITE)
      subKey = uninstallKey.createChild(this.uninstallSubkeyStr,
                                        uninstallKey.ACCESS_WRITE);

      subKey.writeStringValue("DisplayName", this.appName);

      let uninstallerPath = OS.Path.join(aInstallDir, this.uninstallerFile);

      subKey.writeStringValue("UninstallString", '"' + uninstallerPath + '"');
      subKey.writeStringValue("InstallLocation", '"' + aInstallDir + '"');
      subKey.writeStringValue("AppFilename", this.appNameAsFilename);
      subKey.writeStringValue("DisplayIcon", OS.Path.join(aInstallDir,
                                                          this.iconPath));

      let date = new Date();
      let year = date.getYear().toString();
      let month = date.getMonth();
      if (month < 10) {
        month = "0" + month;
      }
      let day = date.getDate();
      if (day < 10) {
        day = "0" + day;
      }
      subKey.writeStringValue("InstallDate", year + month + day);
      if (this.version) {
        subKey.writeStringValue("DisplayVersion", this.version);
      }
      if (this.developerName) {
        subKey.writeStringValue("Publisher", this.developerName);
      }
      subKey.writeStringValue("URLInfoAbout", this.developerUrl);
      if (this.size) {
        subKey.writeIntValue("EstimatedSize", this.size);
      }

      subKey.writeIntValue("NoModify", 1);
      subKey.writeIntValue("NoRepair", 1);
    } catch(ex) {
      throw ex;
    } finally {
      if(subKey) subKey.close();
      if(uninstallKey) uninstallKey.close();
      if(parentKey) parentKey.close();
    }
  },

  /**
   * Creates a shortcut file inside the app installation folder and makes
   * two copies of it: one into the desktop and one into the start menu.
   */
  _createShortcutFiles: function(aInstallDir) {
    let shortcut = getFile(aInstallDir, this.shortcutName).
                      QueryInterface(Ci.nsILocalFileWin);

    /* function nsILocalFileWin.setShortcut(targetFile, workingDir, args,
                                            description, iconFile, iconIndex) */

    shortcut.setShortcut(getFile(aInstallDir, this.webapprt),
                         getFile(aInstallDir),
                         null,
                         this.shortDescription,
                         getFile(aInstallDir, this.iconPath),
                         0);

    shortcut.copyTo(getFile(DESKTOP_DIR), this.shortcutName);
    shortcut.copyTo(getFile(PROGS_DIR), this.shortcutName);

    shortcut.followLinks = false;
    shortcut.remove(false);
  },

  /**
   * Process the icon from the imageStream as retrieved from
   * the URL by getIconForApp(). This will save the icon to the
   * topwindow.ico file.
   *
   * @param aMimeType     the icon mimetype
   * @param aImageStream  the stream for the image data
   * @param aDir          the directory where the icon should be stored
   */
  _processIcon: function(aMimeType, aImageStream, aDir) {
    let deferred = Promise.defer();

    let imgTools = Cc["@mozilla.org/image/tools;1"].
                   createInstance(Ci.imgITools);

    let imgContainer = imgTools.decodeImage(aImageStream, aMimeType);
    let iconStream = imgTools.encodeImage(imgContainer,
                                          "image/vnd.microsoft.icon",
                                          "format=bmp;bpp=32");

    let tmpIconFile = getFile(aDir, this.iconPath);

    let outputStream = FileUtils.openSafeFileOutputStream(tmpIconFile);
    NetUtil.asyncCopy(iconStream, outputStream, function(aResult) {
      if (Components.isSuccessCode(aResult)) {
        deferred.resolve();
      } else {
        deferred.reject("Failure copying icon: " + aResult);
      }
    });

    return deferred.promise;
  }
}
