/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["WebappsInstaller"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/WebappOSUtils.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

// 0755
const PERMS_DIRECTORY = OS.Constants.libc.S_IRWXU |
                        OS.Constants.libc.S_IRGRP | OS.Constants.libc.S_IXGRP |
                        OS.Constants.libc.S_IROTH | OS.Constants.libc.S_IXOTH;

// 0644
const PERMS_FILE = OS.Constants.libc.S_IRUSR | OS.Constants.libc.S_IWUSR |
                   OS.Constants.libc.S_IRGRP |
                   OS.Constants.libc.S_IROTH;

const DESKTOP_DIR = OS.Constants.Path.desktopDir;
const HOME_DIR = OS.Constants.Path.homeDir;
const TMP_DIR = OS.Constants.Path.tmpDir;

this.WebappsInstaller = {
  shell: null,

  /**
   * Initializes the app object that takes care of the installation
   * and creates the profile directory for an application
   *
   * @param aData the data provided to the install function
   *
   * @returns NativeApp on success, null on error
   */
  init: function(aData) {
#ifdef XP_WIN
    this.shell = new WinNativeApp(aData);
#elifdef XP_MACOSX
    this.shell = new MacNativeApp(aData);
#elifdef XP_UNIX
    this.shell = new LinuxNativeApp(aData);
#else
    return null;
#endif

    try {
      if (Services.prefs.getBoolPref("browser.mozApps.installer.dry_run")) {
        return this.shell;
      }
    } catch (ex) {}

    try {
      this.shell.createAppProfile();
    } catch (ex) {
      Cu.reportError("Error installing app: " + ex);
      return null;
    }

    return this.shell;
  },

  /**
   * Creates a native installation of the web app in the OS
   *
   * @param aData the data provided to the install function
   * @param aManifest the manifest data provided by the web app
   * @param aZipPath path to the zip file for packaged apps (undefined for
   *                 hosted apps)
   */
  install: function(aData, aManifest, aZipPath) {
    try {
      if (Services.prefs.getBoolPref("browser.mozApps.installer.dry_run")) {
        return Promise.resolve();
      }
    } catch (ex) {}

    this.shell.init(aData, aManifest);

    return this.shell.install(aZipPath).then(() => {
      let data = {
        "installDir": this.shell.installDir,
        "app": {
          "manifest": aManifest,
          "origin": aData.app.origin
        }
      };

      Services.obs.notifyObservers(null, "webapp-installed", JSON.stringify(data));
    });
  }
}

/**
 * This function implements the common constructor for
 * the Windows, Mac and Linux native app shells. It sets
 * the app unique name. It's meant to be called as
 * NativeApp.call(this, aData) from the platform-specific
 * constructor.
 *
 * @param aData the data object provided to the install function
 *
 */
function NativeApp(aData) {
  let jsonManifest = aData.isPackage ? aData.app.updateManifest : aData.app.manifest;
  let manifest = new ManifestHelper(jsonManifest, aData.app.origin);

  aData.app.name = manifest.name;
  this.uniqueName = WebappOSUtils.getUniqueName(aData.app);

  this.appName = sanitize(manifest.name);
  this.appNameAsFilename = stripStringForFilename(this.appName);
}

NativeApp.prototype = {
  uniqueName: null,
  appName: null,
  appNameAsFilename: null,
  iconURI: null,
  developerName: null,
  shortDescription: null,
  categories: null,
  webappJson: null,
  runtimeFolder: null,
  manifest: null,

  /**
   * This function reads and parses the data from the app
   * manifest and stores it in the NativeApp object.
   *
   * @param aData the data object provided to the install function
   * @param aManifest the manifest data provided by the web app
   *
   */
  init: function(aData, aManifest) {
    let app = this.app = aData.app;
    let manifest = this.manifest = new ManifestHelper(aManifest,
                                                      app.origin);

    let origin = Services.io.newURI(app.origin, null, null);

    let biggestIcon = getBiggestIconURL(manifest.icons);
    try {
      let iconURI = Services.io.newURI(biggestIcon, null, null);
      if (iconURI.scheme == "data") {
        this.iconURI = iconURI;
      }
    } catch (ex) {}

    if (!this.iconURI) {
      try {
        this.iconURI = Services.io.newURI(origin.resolve(biggestIcon), null, null);
      }
      catch (ex) {}
    }

    if (manifest.developer) {
      if (manifest.developer.name) {
        let devName = sanitize(manifest.developer.name.substr(0, 128));
        if (devName) {
          this.developerName = devName;
        }
      }

      if (manifest.developer.url) {
        this.developerUrl = manifest.developer.url;
      }
    }

    if (manifest.description) {
      let firstLine = manifest.description.split("\n")[0];
      let shortDesc = firstLine.length <= 256
                      ? firstLine
                      : firstLine.substr(0, 253) + "…";
      this.shortDescription = sanitize(shortDesc);
    } else {
      this.shortDescription = this.appName;
    }

    this.categories = app.categories.slice(0);

    this.webappJson = {
      // The app registry is the Firefox profile from which the app
      // was installed.
      "registryDir": OS.Constants.Path.profileDir,
      "app": {
        "manifest": aManifest,
        "origin": app.origin,
        "manifestURL": app.manifestURL,
        "installOrigin": app.installOrigin,
        "categories": app.categories,
        "receipts": app.receipts,
        "installTime": app.installTime,
      }
    };

    if (app.etag) {
      this.webappJson.app.etag = app.etag;
    }

    if (app.packageEtag) {
      this.webappJson.app.packageEtag = app.packageEtag;
    }

    if (app.updateManifest) {
      this.webappJson.app.updateManifest = app.updateManifest;
    }

    this.runtimeFolder = OS.Constants.Path.libDir;
  },

  /**
   * This function retrieves the icon for an app.
   * If the retrieving fails, it uses the default chrome icon.
   */
  getIcon: function() {
    try {
      // If the icon is in the zip package, we should modify the url
      // to point to the zip file (we can't use the app protocol yet
      // because the app isn't installed yet).
      if (this.iconURI.scheme == "app") {
        let zipFile = getFile(this.tmpInstallDir, "application.zip");
        let zipUrl = Services.io.newFileURI(zipFile).spec;

        let filePath = this.iconURI.QueryInterface(Ci.nsIURL).filePath;

        this.iconURI = Services.io.newURI("jar:" + zipUrl + "!" + filePath,
                                          null, null);
      }


      let [ mimeType, icon ] = yield downloadIcon(this.iconURI);
      yield this.processIcon(mimeType, icon);
    }
    catch(e) {
      Cu.reportError("Failure retrieving icon: " + e);

      let iconURI = Services.io.newURI(DEFAULT_ICON_URL, null, null);

      let [ mimeType, icon ] = yield downloadIcon(iconURI);
      yield this.processIcon(mimeType, icon);

      // Set the iconURI property so that the user notification will have the
      // correct icon.
      this.iconURI = iconURI;
    }
  },

  /**
   * Creates the profile to be used for this app.
   */
  createAppProfile: function() {
    let profSvc = Cc["@mozilla.org/toolkit/profile-service;1"]
                    .getService(Ci.nsIToolkitProfileService);

    try {
      this.appProfile = profSvc.createDefaultProfileForApp(this.uniqueName,
                                                           null, null);
    } catch (ex if ex.result == Cr.NS_ERROR_ALREADY_INITIALIZED) {}
  },
};

#ifdef XP_WIN

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
 * @param aData the data object provided to the install function
 */
function WinNativeApp(aData) {
  NativeApp.call(this, aData);

  if (aData.isPackage) {
    this.size = aData.app.updateManifest.size / 1024;
  }

  let filenameRE = new RegExp("[<>:\"/\\\\|\\?\\*]", "gi");

  this.appNameAsFilename = this.appNameAsFilename.replace(filenameRE, "");
  if (this.appNameAsFilename == "") {
    this.appNameAsFilename = "webapp";
  }

  this.webapprt = this.appNameAsFilename + ".exe";
  this.configJson = "webapp.json";
  this.webappINI = "webapp.ini";
  this.iconPath = OS.Path.join("chrome", "icons", "default", "default.ico");
  this.uninstallDir = "uninstall";
  this.uninstallerFile = OS.Path.join(this.uninstallDir,
                                      "webapp-uninstaller.exe");
  this.shortcutLogsINI = OS.Path.join(this.uninstallDir, "shortcuts_log.ini");

  this.uninstallSubkeyStr = this.uniqueName;
}

WinNativeApp.prototype = {
  __proto__: NativeApp.prototype,
  size: null,

  /**
   * Install the app in the system
   *
   */
  install: function(aZipPath) {
    return Task.spawn(function() {
      this._getInstallDir();

      try {
        yield this._createDirectoryStructure();
        yield this._copyPrebuiltFiles();
        this._createConfigFiles();

        if (aZipPath) {
          yield OS.File.move(aZipPath, OS.Path.join(this.tmpInstallDir,
                                                    "application.zip"));
        }

        yield this.getIcon();

        // Remove previously installed app
        this._removeInstallation(true);
      } catch (ex) {
        removeFiles([this.tmpInstallDir]);
        throw(ex);
      }

      try {
        // On Windows, the webapprt executable can't be overwritten while it's
        // running.
        // As it takes care of updating itself, there's no need to update
        // it here.
        let filesToIgnore = [ this.webapprt ];
        yield moveDirectory(this.tmpInstallDir, this.installDir, filesToIgnore);

        this._createShortcutFiles();
        this._writeSystemKeys();
      } catch (ex) {
        this._removeInstallation(false);
        throw(ex);
      }
    }.bind(this));
  },

  _getInstallDir: function() {
    // The ${InstallDir} is: sanitized app name + "-" + manifest url hash
    this.installDir = WebappOSUtils.getInstallPath(this.app);
    if (this.installDir) {
      if (this.uniqueName != OS.Path.basename(this.installDir)) {
        // Bug 919799: If the app is still in the registry, migrate its data to
        // the new format.
        throw("Updates for apps installed with the old naming scheme unsupported");
      }

      let shortcutLogsINIfile = getFile(this.installDir, this.shortcutLogsINI);
      // If it's a reinstallation (or an update) get the shortcut names
      // from the shortcut_log.ini file
      let parser = Cc["@mozilla.org/xpcom/ini-processor-factory;1"]
                     .getService(Ci.nsIINIParserFactory)
                     .createINIParser(shortcutLogsINIfile);
      this.shortcutName = parser.getString("STARTMENU", "Shortcut0");
    } else {
      this.installDir = OS.Path.join(APP_DATA_DIR, this.uniqueName);

      // Check in both directories to see if a shortcut with the same name
      // already exists.
      this.shortcutName = getAvailableFileName([ PROGS_DIR, DESKTOP_DIR ],
                                               this.appNameAsFilename,
                                               ".lnk");
    }
  },

  /**
   * Remove the current installation
   */
  _removeInstallation: function(keepProfile) {
    let uninstallKey;
    try {
      uninstallKey = Cc["@mozilla.org/windows-registry-key;1"]
                     .createInstance(Ci.nsIWindowsRegKey);
      uninstallKey.open(uninstallKey.ROOT_KEY_CURRENT_USER,
                        "SOFTWARE\\Microsoft\\Windows\\" +
                        "CurrentVersion\\Uninstall",
                        uninstallKey.ACCESS_WRITE);
      if(uninstallKey.hasChild(this.uninstallSubkeyStr)) {
        uninstallKey.removeChild(this.uninstallSubkeyStr);
      }
    } catch (e) {
    } finally {
      if(uninstallKey)
        uninstallKey.close();
    }

    let filesToRemove = [ OS.Path.join(DESKTOP_DIR, this.shortcutName),
                          OS.Path.join(PROGS_DIR, this.shortcutName) ];

    if (keepProfile) {
      [ this.iconPath, this.webapprt, this.configJson,
        this.webappINI, this.uninstallDir ].forEach((filePath) => {
        filesToRemove.push(OS.Path.join(this.installDir, filePath));
      });
    } else {
      filesToRemove.push(this.installDir);
      filesToRemove.push(this.tmpInstallDir);
    }

    removeFiles(filesToRemove);
  },

  /**
   * Creates the main directory structure.
   */
  _createDirectoryStructure: function() {
    let dir = getFile(TMP_DIR, this.uniqueName);
    dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
    this.tmpInstallDir = dir.path;

    yield OS.File.makeDir(OS.Path.join(this.tmpInstallDir, this.uninstallDir),
                          { ignoreExisting: true });

    // Recursively create the icon path's directory structure.
    let path = this.tmpInstallDir;
    let components = OS.Path.split(OS.Path.dirname(this.iconPath)).components;
    for (let component of components) {
      path = OS.Path.join(path, component);
      yield OS.File.makeDir(path, { ignoreExisting: true });
    }
  },

  /**
   * Copy the pre-built files into their destination folders.
   */
  _copyPrebuiltFiles: function() {
    yield OS.File.copy(OS.Path.join(this.runtimeFolder, "webapprt-stub.exe"),
                       OS.Path.join(this.tmpInstallDir, this.webapprt));

    yield OS.File.copy(OS.Path.join(this.runtimeFolder, "webapp-uninstaller.exe"),
                       OS.Path.join(this.tmpInstallDir, this.uninstallerFile));
  },

  /**
   * Creates the configuration files into their destination folders.
   */
  _createConfigFiles: function() {
    // ${InstallDir}/webapp.json
    writeToFile(OS.Path.join(this.tmpInstallDir, this.configJson),
                JSON.stringify(this.webappJson));

    let factory = Cc["@mozilla.org/xpcom/ini-processor-factory;1"]
                    .getService(Ci.nsIINIParserFactory);

    // ${InstallDir}/webapp.ini
    let webappINIfile = getFile(this.tmpInstallDir, this.webappINI);

    let writer = factory.createINIParser(webappINIfile)
                        .QueryInterface(Ci.nsIINIParserWriter);
    writer.setString("Webapp", "Name", this.appName);
    writer.setString("Webapp", "Profile", OS.Path.basename(this.installDir));
    writer.setString("Webapp", "Executable", this.appNameAsFilename);
    writer.setString("WebappRT", "InstallDir", this.runtimeFolder);
    writer.writeFile(null, Ci.nsIINIParserWriter.WRITE_UTF16);

    let shortcutLogsINIfile = getFile(this.tmpInstallDir, this.shortcutLogsINI);

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

    writeToFile(OS.Path.join(this.tmpInstallDir, this.uninstallDir,
                             "uninstall.log"),
                uninstallContent);
  },

  /**
   * Writes the keys to the system registry that are necessary for the app operation
   * and uninstall process.
   */
  _writeSystemKeys: function() {
    let parentKey;
    let uninstallKey;
    let subKey;

    try {
      parentKey = Cc["@mozilla.org/windows-registry-key;1"]
                  .createInstance(Ci.nsIWindowsRegKey);
      parentKey.open(parentKey.ROOT_KEY_CURRENT_USER,
                     "SOFTWARE\\Microsoft\\Windows\\CurrentVersion",
                     parentKey.ACCESS_WRITE);
      uninstallKey = parentKey.createChild("Uninstall", parentKey.ACCESS_WRITE)
      subKey = uninstallKey.createChild(this.uninstallSubkeyStr, uninstallKey.ACCESS_WRITE);

      subKey.writeStringValue("DisplayName", this.appName);

      let uninstallerPath = OS.Path.join(this.installDir,
                                         this.uninstallerFile);

      subKey.writeStringValue("UninstallString", '"' + uninstallerPath + '"');
      subKey.writeStringValue("InstallLocation", '"' + this.installDir + '"');
      subKey.writeStringValue("AppFilename", this.appNameAsFilename);
      subKey.writeStringValue("DisplayIcon", OS.Path.join(this.installDir,
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
      if (this.manifest.version) {
        subKey.writeStringValue("DisplayVersion", this.manifest.version);
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
      throw(ex);
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
  _createShortcutFiles: function() {
    let shortcut = getFile(this.installDir, this.shortcutName).
                      QueryInterface(Ci.nsILocalFileWin);

    /* function nsILocalFileWin.setShortcut(targetFile, workingDir, args,
                                            description, iconFile, iconIndex) */

    shortcut.setShortcut(getFile(this.installDir, this.webapprt),
                         getFile(this.installDir),
                         null,
                         this.shortDescription,
                         getFile(this.installDir, this.iconPath),
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
   * @param aMimeType     ahe icon mimetype
   * @param aImageStream  the stream for the image data
   */
  processIcon: function(aMimeType, aImageStream) {
    let deferred = Promise.defer();

    let imgTools = Cc["@mozilla.org/image/tools;1"]
                     .createInstance(Ci.imgITools);

    let imgContainer = imgTools.decodeImage(aImageStream, aMimeType);
    let iconStream = imgTools.encodeImage(imgContainer,
                                          "image/vnd.microsoft.icon",
                                          "format=bmp;bpp=32");

    let tmpIconFile = getFile(this.tmpInstallDir, this.iconPath);

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

#elifdef XP_MACOSX

const USER_LIB_DIR = OS.Constants.Path.macUserLibDir;
const LOCAL_APP_DIR = OS.Constants.Path.macLocalApplicationsDir;

function MacNativeApp(aData) {
  NativeApp.call(this, aData);

  let filenameRE = new RegExp("[<>:\"/\\\\|\\?\\*]", "gi");
  this.appNameAsFilename = this.appNameAsFilename.replace(filenameRE, "");
  if (this.appNameAsFilename == "") {
    this.appNameAsFilename = "Webapp";
  }

  // The ${ProfileDir} is: sanitized app name + "-" + manifest url hash
  this.appProfileDir = OS.Path.join(USER_LIB_DIR, "Application Support",
                                    this.uniqueName);

  this.contentsDir = "Contents";
  this.macOSDir = OS.Path.join(this.contentsDir, "MacOS");
  this.resourcesDir = OS.Path.join(this.contentsDir, "Resources");
  this.iconFile = OS.Path.join(this.resourcesDir, "appicon.icns");
}

MacNativeApp.prototype = {
  __proto__: NativeApp.prototype,

  install: function(aZipPath) {
    return Task.spawn(function() {
      this._getInstallDir();

      try {
        yield this._createDirectoryStructure();
        this._copyPrebuiltFiles();
        this._createConfigFiles();

        if (aZipPath) {
          yield OS.File.move(aZipPath, OS.Path.join(this.tmpInstallDir,
                                                    "application.zip"));
        }

        yield this.getIcon();

        // Remove previously installed app
        this._removeInstallation(true);
      } catch (ex) {
        removeFiles([this.tmpInstallDir]);
        throw(ex);
      }

      try {
        // Move the temp installation directory to the /Applications directory
        yield moveDirectory(this.tmpInstallDir, this.installDir, []);
      } catch (ex) {
        this._removeInstallation(false);
        throw(ex);
      }
    }.bind(this));
  },

  _getInstallDir: function() {
    let [ oldUniqueName, installPath ] = WebappOSUtils.getLaunchTarget(this.app);
    if (installPath) {
      this.installDir = installPath;

      if (this.uniqueName != oldUniqueName) {
        // Bug 919799: If the app is still in the registry, migrate its data to
        // the new format.
        throw("Updates for apps installed with the old naming scheme unsupported");
      }
    } else {
      let destinationName = getAvailableFileName([ LOCAL_APP_DIR ],
                                                 this.appNameAsFilename,
                                                ".app");
      if (!destinationName) {
        throw("No available filename");
      }

      this.installDir = OS.Path.join(LOCAL_APP_DIR, destinationName);
    }
  },

  _removeInstallation: function(keepProfile) {
    let filesToRemove = [this.installDir];

    if (!keepProfile) {
      filesToRemove.push(this.appProfileDir);
    }

    removeFiles(filesToRemove);
  },

  _createDirectoryStructure: function() {
    let dir = getFile(TMP_DIR, this.appNameAsFilename + ".app");
    dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
    this.tmpInstallDir = dir.path;

    yield OS.File.makeDir(this.appProfileDir,
                          { unixMode: PERMS_DIRECTORY, ignoreExisting: true });

    yield OS.File.makeDir(OS.Path.join(this.tmpInstallDir, this.contentsDir),
                          { unixMode: PERMS_DIRECTORY, ignoreExisting: true });

    yield OS.File.makeDir(OS.Path.join(this.tmpInstallDir, this.macOSDir),
                          { unixMode: PERMS_DIRECTORY, ignoreExisting: true });

    yield OS.File.makeDir(OS.Path.join(this.tmpInstallDir, this.resourcesDir),
                          { unixMode: PERMS_DIRECTORY, ignoreExisting: true });
  },

  _copyPrebuiltFiles: function() {
    let destDir = getFile(this.tmpInstallDir, this.macOSDir);
    let stub = getFile(this.runtimeFolder, "webapprt-stub");
    stub.copyTo(destDir, "webapprt");
  },

  _createConfigFiles: function() {
    // ${ProfileDir}/webapp.json
    writeToFile(OS.Path.join(this.appProfileDir, "webapp.json"),
                JSON.stringify(this.webappJson));

    // ${InstallDir}/Contents/MacOS/webapp.ini
    let applicationINI = getFile(this.tmpInstallDir, this.macOSDir, "webapp.ini");

    let writer = Cc["@mozilla.org/xpcom/ini-processor-factory;1"]
                   .getService(Ci.nsIINIParserFactory)
                   .createINIParser(applicationINI)
                   .QueryInterface(Ci.nsIINIParserWriter);
    writer.setString("Webapp", "Name", this.appName);
    writer.setString("Webapp", "Profile", OS.Path.basename(this.appProfileDir));
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
    <string>' + escapeXML(this.appName) + '</string>\n\
    <key>CFBundleExecutable</key>\n\
    <string>webapprt</string>\n\
    <key>CFBundleIconFile</key>\n\
    <string>appicon</string>\n\
    <key>CFBundleIdentifier</key>\n\
    <string>' + escapeXML(this.uniqueName) + '</string>\n\
    <key>CFBundleInfoDictionaryVersion</key>\n\
    <string>6.0</string>\n\
    <key>CFBundleName</key>\n\
    <string>' + escapeXML(this.appName) + '</string>\n\
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

    writeToFile(OS.Path.join(this.tmpInstallDir, this.contentsDir, "Info.plist"),
                infoPListContent);
  },

  /**
   * Process the icon from the imageStream as retrieved from
   * the URL by getIconForApp(). This will bundle the icon to the
   * app package at Contents/Resources/appicon.icns.
   *
   * @param aMimeType     the icon mimetype
   * @param aImageStream  the stream for the image data
   */
  processIcon: function(aMimeType, aIcon) {
    let deferred = Promise.defer();

    function conversionDone(aSubject, aTopic) {
      if (aTopic == "process-finished") {
        deferred.resolve();
      } else {
        deferred.reject("Failure converting icon.");
      }
    }

    let process = Cc["@mozilla.org/process/util;1"].
                  createInstance(Ci.nsIProcess);
    let sipsFile = getFile("/usr/bin/sips");

    process.init(sipsFile);
    process.runAsync(["-s",
                "format", "icns",
                aIcon.path,
                "--out", OS.Path.join(this.tmpInstallDir, this.iconFile),
                "-z", "128", "128"],
                9, conversionDone);

    return deferred.promise;
  }

}

#elifdef XP_UNIX

function LinuxNativeApp(aData) {
  NativeApp.call(this, aData);

  this.iconFile = "icon.png";
  this.webapprt = "webapprt-stub";
  this.configJson = "webapp.json";
  this.webappINI = "webapp.ini";

  let xdg_data_home = Cc["@mozilla.org/process/environment;1"]
                        .getService(Ci.nsIEnvironment)
                        .get("XDG_DATA_HOME");
  if (!xdg_data_home) {
    xdg_data_home = OS.Path.join(HOME_DIR, ".local", "share");
  }

  this.desktopINI = OS.Path.join(xdg_data_home, "applications",
                                 "owa-" + this.uniqueName + ".desktop");
}

LinuxNativeApp.prototype = {
  __proto__: NativeApp.prototype,

  install: function(aZipPath) {
    return Task.spawn(function() {
      this._getInstallDir();

      try {
        this._createDirectoryStructure();
        this._copyPrebuiltFiles();
        this._createConfigFiles();

        if (aZipPath) {
          yield OS.File.move(aZipPath, OS.Path.join(this.tmpInstallDir,
                                                    "application.zip"));
        }

        yield this.getIcon();

        // Remove previously installed app
        this._removeInstallation(true);
      } catch (ex) {
        removeFiles([this.tmpInstallDir]);
        throw(ex);
      }

      try {
        yield moveDirectory(this.tmpInstallDir, this.installDir, []);

        this._createSystemFiles();
      } catch (ex) {
        this._removeInstallation(false);
        throw(ex);
      }
    }.bind(this));
  },

  _getInstallDir: function() {
    // The ${InstallDir} and desktop entry filename are: sanitized app name +
    // "-" + manifest url hash
    this.installDir = WebappOSUtils.getInstallPath(this.app);
    if (this.installDir) {
      let baseName = OS.Path.basename(this.installDir)
      let oldUniqueName = baseName.substring(1, baseName.length);
      if (this.uniqueName != oldUniqueName) {
        // Bug 919799: If the app is still in the registry, migrate its data to
        // the new format.
        throw("Updates for apps installed with the old naming scheme unsupported");
      }
    } else {
      this.installDir = OS.Path.join(HOME_DIR, "." + this.uniqueName);
    }
  },

  _removeInstallation: function(keepProfile) {
    let filesToRemove = [this.desktopINI];

    if (keepProfile) {
      [ this.iconFile, this.webapprt,
        this.configJson, this.webappINI ].forEach((filePath) => {
        filesToRemove.push(OS.Path.join(this.installDir, filePath));
      });
    } else {
      filesToRemove.push(this.installDir);
      filesToRemove.push(this.tmpInstallDir);
    }

    removeFiles(filesToRemove);
  },

  _createDirectoryStructure: function() {
    let dir = getFile(TMP_DIR, this.uniqueName);
    dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
    this.tmpInstallDir = dir.path;
  },

  _copyPrebuiltFiles: function() {
    let destDir = getFile(this.tmpInstallDir);
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

  _createConfigFiles: function() {
    // ${InstallDir}/webapp.json
    writeToFile(OS.Path.join(this.tmpInstallDir, this.configJson),
                JSON.stringify(this.webappJson));

    let webappsBundle = Services.strings.createBundle("chrome://global/locale/webapps.properties");

    // ${InstallDir}/webapp.ini
    let webappINIfile = getFile(this.tmpInstallDir, this.webappINI);

    let writer = Cc["@mozilla.org/xpcom/ini-processor-factory;1"]
                   .getService(Ci.nsIINIParserFactory)
                   .createINIParser(webappINIfile)
                   .QueryInterface(Ci.nsIINIParserWriter);
    writer.setString("Webapp", "Name", this.appName);
    writer.setString("Webapp", "Profile", this.uniqueName);
    writer.setString("Webapp", "UninstallMsg", webappsBundle.formatStringFromName("uninstall.notification", [this.appName], 1));
    writer.setString("WebappRT", "InstallDir", this.runtimeFolder);
    writer.writeFile();
  },

  _createSystemFiles: function() {
    let webappsBundle = Services.strings.createBundle("chrome://global/locale/webapps.properties");

    let webapprtPath = OS.Path.join(this.installDir, this.webapprt);

    // $XDG_DATA_HOME/applications/owa-<webappuniquename>.desktop
    let desktopINIfile = getFile(this.desktopINI);

    let writer = Cc["@mozilla.org/xpcom/ini-processor-factory;1"]
                   .getService(Ci.nsIINIParserFactory)
                   .createINIParser(desktopINIfile)
                   .QueryInterface(Ci.nsIINIParserWriter);
    writer.setString("Desktop Entry", "Name", this.appName);
    writer.setString("Desktop Entry", "Comment", this.shortDescription);
    writer.setString("Desktop Entry", "Exec", '"' + webapprtPath + '"');
    writer.setString("Desktop Entry", "Icon", OS.Path.join(this.installDir,
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

    desktopINIfile.permissions = PERMS_FILE | OS.Constants.libc.S_IXUSR;
  },

  /**
   * Process the icon from the imageStream as retrieved from
   * the URL by getIconForApp().
   *
   * @param aMimeType     ahe icon mimetype
   * @param aImageStream  the stream for the image data
   */
  processIcon: function(aMimeType, aImageStream) {
    let deferred = Promise.defer();

    let imgTools = Cc["@mozilla.org/image/tools;1"]
                     .createInstance(Ci.imgITools);

    let imgContainer = imgTools.decodeImage(aImageStream, aMimeType);
    let iconStream = imgTools.encodeImage(imgContainer, "image/png");

    let iconFile = getFile(this.tmpInstallDir, this.iconFile);
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

#endif

/* Helper Functions */

/**
 * Async write a data string into a file
 *
 * @param aPath     the path to the file to write to
 * @param aData     a string with the data to be written
 */
function writeToFile(aPath, aData) {
  return Task.spawn(function() {
    let data = new TextEncoder().encode(aData);
    let file = yield OS.File.open(aPath, { truncate: true }, { unixMode: PERMS_FILE });
    yield file.write(data);
    yield file.close();
  });
}

/**
 * Removes unprintable characters from a string.
 */
function sanitize(aStr) {
  let unprintableRE = new RegExp("[\\x00-\\x1F\\x7F]" ,"gi");
  return aStr.replace(unprintableRE, "");
}

/**
 * Strips all non-word characters from the beginning and end of a string
 */
function stripStringForFilename(aPossiblyBadFilenameString) {
  //strip everything from the front up to the first [0-9a-zA-Z]

  let stripFrontRE = new RegExp("^\\W*","gi");
  let stripBackRE = new RegExp("\\s*$","gi");

  let stripped = aPossiblyBadFilenameString.replace(stripFrontRE, "");
  stripped = stripped.replace(stripBackRE, "");
  return stripped;
}

/**
 * Finds a unique name available in a folder (i.e., non-existent file)
 *
 * @param aPathSet a set of paths that represents the set of
 * directories where we want to write
 * @param aName   string with the filename (minus the extension) desired
 * @param aExtension string with the file extension, including the dot

 * @return file name or null if folder is unwritable or unique name
 *         was not available
 */
function getAvailableFileName(aPathSet, aName, aExtension) {
  let fileSet = [];
  let name = aName + aExtension;
  let isUnique = true;

  // Check if the plain name is a unique name in all the directories.
  for (let path of aPathSet) {
    let folder = getFile(path);

    folder.followLinks = false;
    if (!folder.isDirectory() || !folder.isWritable()) {
      return null;
    }

    let file = folder.clone();
    file.append(name);
    // Avoid exists() call if we already know this file name is not unique in
    // one of the directories.
    if (isUnique && file.exists()) {
      isUnique = false;
    }

    fileSet.push(file);
  }

  if (isUnique) {
    return name;
  }


  function checkUnique(aName) {
    for (let file of fileSet) {
      file.leafName = aName;

      if (file.exists()) {
        return false;
      }
    }

    return true;
  }

  // If we're here, the plain name wasn't enough. Let's try modifying the name
  // by adding "(" + num + ")".
  for (let i = 2; i < 100; i++) {
    name = aName + " (" + i + ")" + aExtension;

    if (checkUnique(name)) {
      return name;
    }
  }

  return null;
}

/**
 * Attempts to remove files or directories.
 *
 * @param aPaths An array with paths to files to remove
 */
function removeFiles(aPaths) {
  for (let path of aPaths) {
    let file = getFile(path);

    try {
      if (file.exists()) {
        file.followLinks = false;
        file.remove(true);
      }
    } catch(ex) {}
  }
}

/**
 * Move (overwriting) the contents of one directory into another.
 *
 * @param srcPath A path to the source directory
 * @param destPath A path to the destination directory
 * @param filesToIgnore An array of files. If one of those files can't be
 *                      overwritten the function will not fail.
 */
function moveDirectory(srcPath, destPath, filesToIgnore) {
  let srcDir = getFile(srcPath);
  let destDir = getFile(destPath);

  let entries = srcDir.directoryEntries;
  let array = [];
  while (entries.hasMoreElements()) {
    let entry = entries.getNext().QueryInterface(Ci.nsIFile);

    if (entry.isDirectory()) {
      yield moveDirectory(entry.path, OS.Path.join(destPath, entry.leafName));
    } else {
      try {
        entry.moveTo(destDir, entry.leafName);
      } catch (ex if ex.result == Cr.NS_ERROR_FILE_ACCESS_DENIED) {
        if (filesToIgnore.indexOf(entry.leafName) != -1) {
          yield OS.File.remove(entry.path);
        } else {
          throw(ex);
        }
      }
    }
  }

  // The source directory is now empty, remove it.
  yield OS.File.removeEmptyDir(srcPath);
}

function escapeXML(aStr) {
  return aStr.toString()
             .replace(/&/g, "&amp;")
             .replace(/"/g, "&quot;")
             .replace(/'/g, "&apos;")
             .replace(/</g, "&lt;")
             .replace(/>/g, "&gt;");
}

// Helper to create a nsIFile from a set of path components
function getFile() {
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(OS.Path.join.apply(OS.Path, arguments));
  return file;
}

/* More helpers for handling the app icon */
#include WebappsIconHelpers.js
