/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Constructor for the Linux native app shell
 *
 * @param aApp {Object} the app object provided to the install function
 * @param aManifest {Object} the manifest data provided by the web app
 * @param aCategories {Array} array of app categories
 * @param aRegistryDir {String} (optional) path to the registry
 */
function NativeApp(aApp, aManifest, aCategories, aRegistryDir) {
  CommonNativeApp.call(this, aApp, aManifest, aCategories, aRegistryDir);

  this.iconFile = "icon.png";
  this.webapprt = "webapprt-stub";
  this.configJson = "webapp.json";
  this.webappINI = "webapp.ini";
  this.zipFile = "application.zip";

  this.backupFiles = [ this.iconFile, this.configJson, this.webappINI ];
  if (this.isPackaged) {
    this.backupFiles.push(this.zipFile);
  }

  let xdg_data_home = Cc["@mozilla.org/process/environment;1"].
                      getService(Ci.nsIEnvironment).
                      get("XDG_DATA_HOME");
  if (!xdg_data_home) {
    xdg_data_home = OS.Path.join(HOME_DIR, ".local", "share");
  }

  // The desktop file name is: "owa-" + sanitized app name +
  // "-" + manifest url hash.
  this.desktopINI = OS.Path.join(xdg_data_home, "applications",
                                 "owa-" + this.uniqueName + ".desktop");
}

NativeApp.prototype = {
  __proto__: CommonNativeApp.prototype,

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

    // The installation directory name is: sanitized app name +
    // "-" + manifest url hash.
    let installDir = OS.Path.join(HOME_DIR, "." + this.uniqueName);

    let dir = getFile(TMP_DIR, this.uniqueName);
    dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
    let tmpDir = dir.path;

    // Create the installation in a temporary directory.
    try {
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
  prepareUpdate: Task.async(function*(aApp, aManifest, aZipPath) {
    if (this._dryRun) {
      return;
    }

    this._setData(aApp, aManifest);

    let installDir = WebappOSUtils.getInstallPath(aApp);
    if (!installDir) {
      throw ERR_NOT_INSTALLED;
    }

    let baseName = OS.Path.basename(installDir)
    let oldUniqueName = baseName.substring(1, baseName.length);
    if (this.uniqueName != oldUniqueName) {
      // Bug 919799: If the app is still in the registry, migrate its data to
      // the new format.
      throw ERR_UPDATES_UNSUPPORTED_OLD_NAMING_SCHEME;
    }

    let updateDir = OS.Path.join(installDir, "update");
    yield OS.File.removeDir(updateDir, { ignoreAbsent: true });
    yield OS.File.makeDir(updateDir);

    try {
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

    yield this._createSystemFiles(aInstallDir);
  }),

  _removeInstallation: function(keepProfile, aInstallDir) {
    let filesToRemove = [this.desktopINI];

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

  _copyPrebuiltFiles: function(aDir) {
    let destDir = getFile(aDir);
    let stub = getFile(this.runtimeFolder, this.webapprt);
    stub.copyTo(destDir, null);
  },

  /**
   * Translate marketplace categories to freedesktop.org categories.
   *
   * @link http://standards.freedesktop.org/menu-spec/menu-spec-latest.html#category-registry
   *
   * @return an array of categories
   */
  _translateCategories: function() {
    let translations = {
      "books": "Education;Literature",
      "business": "Finance",
      "education": "Education",
      "entertainment": "Amusement",
      "sports": "Sports",
      "games": "Game",
      "health-fitness": "MedicalSoftware",
      "lifestyle": "Amusement",
      "music": "Audio;Music",
      "news-weather": "News",
      "photo-video": "Video;AudioVideo;Photography",
      "productivity": "Office",
      "shopping": "Amusement",
      "social": "Chat",
      "travel": "Amusement",
      "reference": "Science;Education;Documentation",
      "maps-navigation": "Maps",
      "utilities": "Utility"
    };

    // The trailing semicolon is needed as written in the freedesktop specification
    let categories = "";
    for (let category of this.categories) {
      let catLower = category.toLowerCase();
      if (catLower in translations) {
        categories += translations[catLower] + ";";
      }
    }

    return categories;
  },

  _createConfigFiles: function(aDir) {
    // ${InstallDir}/webapp.json
    yield writeToFile(OS.Path.join(aDir, this.configJson),
                      JSON.stringify(this.webappJson));

    let webappsBundle = Services.strings.createBundle("chrome://global/locale/webapps.properties");

    // ${InstallDir}/webapp.ini
    let webappINIfile = getFile(aDir, this.webappINI);

    let writer = Cc["@mozilla.org/xpcom/ini-processor-factory;1"].
                 getService(Ci.nsIINIParserFactory).
                 createINIParser(webappINIfile).
                 QueryInterface(Ci.nsIINIParserWriter);
    writer.setString("Webapp", "Name", this.appLocalizedName);
    writer.setString("Webapp", "Profile", this.uniqueName);
    writer.setString("Webapp", "UninstallMsg",
                     webappsBundle.formatStringFromName("uninstall.notification",
                                                        [this.appLocalizedName],
                                                        1));
    writer.setString("WebappRT", "InstallDir", this.runtimeFolder);
    writer.writeFile();
  },

  _createSystemFiles: Task.async(function*(aInstallDir) {
    let webappsBundle = Services.strings.createBundle("chrome://global/locale/webapps.properties");

    let webapprtPath = OS.Path.join(aInstallDir, this.webapprt);

    // $XDG_DATA_HOME/applications/owa-<webappuniquename>.desktop
    let desktopINIfile = getFile(this.desktopINI);
    if (desktopINIfile.parent && !desktopINIfile.parent.exists()) {
      desktopINIfile.parent.create(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
    }

    let writer = Cc["@mozilla.org/xpcom/ini-processor-factory;1"].
                 getService(Ci.nsIINIParserFactory).
                 createINIParser(desktopINIfile).
                 QueryInterface(Ci.nsIINIParserWriter);
    writer.setString("Desktop Entry", "Name", this.appLocalizedName);
    writer.setString("Desktop Entry", "Comment", this.shortDescription);
    writer.setString("Desktop Entry", "Exec", '"' + webapprtPath + '"');
    writer.setString("Desktop Entry", "Icon", OS.Path.join(aInstallDir,
                                                           this.iconFile));
    writer.setString("Desktop Entry", "Type", "Application");
    writer.setString("Desktop Entry", "Terminal", "false");

    let categories = this._translateCategories();
    if (categories)
      writer.setString("Desktop Entry", "Categories", categories);

    writer.setString("Desktop Entry", "Actions", "Uninstall;");
    writer.setString("Desktop Action Uninstall", "Name", webappsBundle.GetStringFromName("uninstall.label"));
    writer.setString("Desktop Action Uninstall", "Exec", webapprtPath + " -remove");

    writer.writeFile();

    yield OS.File.setPermissions(desktopINIfile.path, { unixMode: PERMS_FILE | OS.Constants.libc.S_IXUSR });
  }),

  /**
   * Process the icon from the imageStream as retrieved from
   * the URL by getIconForApp().
   *
   * @param aMimeType     ahe icon mimetype
   * @param aImageStream  the stream for the image data
   * @param aDir          the directory where the icon should be stored
   */
  _processIcon: function(aMimeType, aImageStream, aDir) {
    let deferred = Promise.defer();

    let imgTools = Cc["@mozilla.org/image/tools;1"].
                   createInstance(Ci.imgITools);

    let imgContainer = imgTools.decodeImage(aImageStream, aMimeType);
    let iconStream = imgTools.encodeImage(imgContainer, "image/png");

    let iconFile = getFile(aDir, this.iconFile);
    let outputStream = FileUtils.openSafeFileOutputStream(iconFile);
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
