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
   *
   * @returns true on success, false if an error was thrown
   */
  install: function(aData, aManifest) {
    try {
      if (Services.prefs.getBoolPref("browser.mozApps.installer.dry_run")) {
        return true;
      }
    } catch (ex) {}

    this.shell.init(aData, aManifest);

    try {
      this.shell.install();
    } catch (ex) {
      Cu.reportError("Error installing app: " + ex);
      return false;
    }

    let data = {
      "installDir": this.shell.installDir.path,
      "app": {
        "manifest": aManifest,
        "origin": aData.app.origin
      }
    };
    Services.obs.notifyObservers(null, "webapp-installed", JSON.stringify(data));

    return true;
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
  this.uniqueName = WebappOSUtils.getUniqueName(aData.app);

  let jsonManifest = aData.isPackage ? aData.app.updateManifest : aData.app.manifest;
  let manifest = new ManifestHelper(jsonManifest, aData.app.origin);

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

  /**
   * This function reads and parses the data from the app
   * manifest and stores it in the NativeApp object.
   *
   * @param aData the data object provided to the install function
   * @param aManifest the manifest data provided by the web app
   *
   */
  init: function(aData, aManifest) {
    let manifest = new ManifestHelper(aManifest, aData.app.origin);

    let origin = Services.io.newURI(aData.app.origin, null, null);

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

    if (manifest.developer && manifest.developer.name) {
      let devName = sanitize(manifest.developer.name.substr(0, 128));
      if (devName) {
        this.developerName = devName;
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

    this.categories = aData.app.categories.slice(0);

    // The app registry is the Firefox profile from which the app
    // was installed.
    let registryFolder = Services.dirsvc.get("ProfD", Ci.nsIFile);

    this.webappJson = {
      "registryDir": registryFolder.path,
      "app": {
        "manifest": aManifest,
        "origin": aData.app.origin
      }
    };

    this.runtimeFolder = Services.dirsvc.get("GreD", Ci.nsIFile);
  }
};

#ifdef XP_WIN
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
  this._init();
}

WinNativeApp.prototype = {
  __proto__: NativeApp.prototype,

  /**
   * Install the app in the system
   *
   */
  install: function() {
    try {
      this._copyPrebuiltFiles();
      this._createShortcutFiles();
      this._createConfigFiles();
      this._writeSystemKeys();
    } catch (ex) {
      this._removeInstallation(false);
      throw(ex);
    }

    getIconForApp(this, function() {});
  },

  /**
   * Initializes properties that will be used during the installation process,
   * such as paths and filenames.
   */
  _init: function() {
    let filenameRE = new RegExp("[<>:\"/\\\\|\\?\\*]", "gi");

    this.appNameAsFilename = this.appNameAsFilename.replace(filenameRE, "");
    if (this.appNameAsFilename == "") {
      this.appNameAsFilename = "webapp";
    }

    // The ${InstallDir} is: sanitized app name + "-" + manifest url hash
    this.installDir = Services.dirsvc.get("AppData", Ci.nsIFile);
    this.installDir.append(this.uniqueName);

    this.webapprt = this.installDir.clone();
    this.webapprt.append(this.appNameAsFilename + ".exe");

    this.configJson = this.installDir.clone();
    this.configJson.append("webapp.json");

    this.webappINI = this.installDir.clone();
    this.webappINI.append("webapp.ini");

    this.uninstallDir = this.installDir.clone();
    this.uninstallDir.append("uninstall");

    this.uninstallerFile = this.uninstallDir.clone();
    this.uninstallerFile.append("webapp-uninstaller.exe");

    this.iconFile = this.installDir.clone();
    this.iconFile.append("chrome");
    this.iconFile.append("icons");
    this.iconFile.append("default");
    this.iconFile.append("default.ico");

    this.uninstallSubkeyStr = this.uniqueName;

    // ${UninstallDir}/shortcuts_log.ini
    this.shortcutLogsINI = this.uninstallDir.clone();
    this.shortcutLogsINI.append("shortcuts_log.ini");

    if (this.shortcutLogsINI.exists()) {
      // If it's a reinstallation (or an update) get the shortcut names
      // from the shortcut_log.ini file
      let factory = Cc["@mozilla.org/xpcom/ini-processor-factory;1"]
                      .getService(Ci.nsIINIParserFactory);
      let parser = factory.createINIParser(this.shortcutLogsINI);

      this.shortcutName = parser.getString("STARTMENU", "Shortcut0");
    } else {
      let desktop = Services.dirsvc.get("Desk", Ci.nsIFile);
      let startMenu = Services.dirsvc.get("Progs", Ci.nsIFile);

      // Check in both directories to see if a shortcut with the same name
      // already exists.
      this.shortcutName = getAvailableFileName([ startMenu, desktop ],
                                               this.appNameAsFilename,
                                               ".lnk");
    }

    // Remove previously installed app (for update purposes)
    this._removeInstallation(true);

    this._createDirectoryStructure();
  },

  /**
   * Remove the current installation
   */
  _removeInstallation : function(keepProfile) {
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

    let desktopShortcut = Services.dirsvc.get("Desk", Ci.nsIFile);
    desktopShortcut.append(this.shortcutName);

    let startMenuShortcut = Services.dirsvc.get("Progs", Ci.nsIFile);
    startMenuShortcut.append(this.shortcutName);

    let filesToRemove = [desktopShortcut, startMenuShortcut];

    if (keepProfile) {
      filesToRemove.push(this.iconFile);
      filesToRemove.push(this.webapprt);
      filesToRemove.push(this.configJson);
      filesToRemove.push(this.webappINI);
      filesToRemove.push(this.uninstallDir);
    } else {
      filesToRemove.push(this.installDir);
    }

    removeFiles(filesToRemove);
  },

  /**
   * Creates the main directory structure.
   */
  _createDirectoryStructure: function() {
    if (!this.installDir.exists()) {
      this.installDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
    }

    this.uninstallDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
  },

  /**
   * Creates the profile to be used for this app.
   */
  createAppProfile: function() {
    let profSvc = Cc["@mozilla.org/toolkit/profile-service;1"]
                    .getService(Ci.nsIToolkitProfileService);

    try {
      this.appProfile = profSvc.createDefaultProfileForApp(this.installDir.leafName,
                                                           null, null);
    } catch (ex if ex.result == Cr.NS_ERROR_ALREADY_INITIALIZED) {}
  },

  /**
   * Copy the pre-built files into their destination folders.
   */
  _copyPrebuiltFiles: function() {
    let webapprtPre = this.runtimeFolder.clone();
    webapprtPre.append("webapprt-stub.exe");
    webapprtPre.copyTo(this.installDir, this.webapprt.leafName);

    let uninstaller = this.runtimeFolder.clone();
    uninstaller.append("webapp-uninstaller.exe");
    uninstaller.copyTo(this.uninstallDir, this.uninstallerFile.leafName);
  },

  /**
   * Creates the configuration files into their destination folders.
   */
  _createConfigFiles: function() {
    // ${InstallDir}/webapp.json
    writeToFile(this.configJson, JSON.stringify(this.webappJson));

    let factory = Cc["@mozilla.org/xpcom/ini-processor-factory;1"]
                    .getService(Ci.nsIINIParserFactory);

    // ${InstallDir}/webapp.ini
    let writer = factory.createINIParser(this.webappINI).QueryInterface(Ci.nsIINIParserWriter);
    writer.setString("Webapp", "Name", this.appName);
    writer.setString("Webapp", "Profile", this.installDir.leafName);
    writer.setString("Webapp", "Executable", this.appNameAsFilename);
    writer.setString("WebappRT", "InstallDir", this.runtimeFolder.path);
    writer.writeFile(null, Ci.nsIINIParserWriter.WRITE_UTF16);

    writer = factory.createINIParser(this.shortcutLogsINI).QueryInterface(Ci.nsIINIParserWriter);
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
    let uninstallLog = this.uninstallDir.clone();
    uninstallLog.append("uninstall.log");
    writeToFile(uninstallLog, uninstallContent);
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

      subKey.writeStringValue("UninstallString", '"' + this.uninstallerFile.path + '"');
      subKey.writeStringValue("InstallLocation", '"' + this.installDir.path + '"');
      subKey.writeStringValue("AppFilename", this.appNameAsFilename);

      if(this.iconFile) {
        subKey.writeStringValue("DisplayIcon", this.iconFile.path);
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
    let shortcut = this.installDir.clone().QueryInterface(Ci.nsILocalFileWin);
    shortcut.append(this.shortcutName);

    let target = this.installDir.clone();
    target.append(this.webapprt.leafName);

    /* function nsILocalFileWin.setShortcut(targetFile, workingDir, args,
                                            description, iconFile, iconIndex) */

    shortcut.setShortcut(target, this.installDir.clone(), null,
                         this.shortDescription, this.iconFile, 0);

    let desktop = Services.dirsvc.get("Desk", Ci.nsILocalFile);
    let startMenu = Services.dirsvc.get("Progs", Ci.nsILocalFile);

    shortcut.copyTo(desktop, this.shortcutName);
    shortcut.copyTo(startMenu, this.shortcutName);

    shortcut.followLinks = false;
    shortcut.remove(false);
  },

  /**
   * This variable specifies if the icon retrieval process should
   * use a temporary file in the system or a binary stream. This
   * is accessed by a common function in WebappsIconHelpers.js and
   * is different for each platform.
   */
  useTmpForIcon: false,

  /**
   * Process the icon from the imageStream as retrieved from
   * the URL by getIconForApp(). This will save the icon to the
   * topwindow.ico file.
   *
   * @param aMimeType     ahe icon mimetype
   * @param aImageStream  the stream for the image data
   * @param aCallback     a callback function to be called
   *                      after the process finishes
   */
  processIcon: function(aMimeType, aImageStream, aCallback) {
    let iconStream;
    try {
      let imgTools = Cc["@mozilla.org/image/tools;1"]
                       .createInstance(Ci.imgITools);
      let imgContainer = { value: null };

      imgTools.decodeImageData(aImageStream, aMimeType, imgContainer);
      iconStream = imgTools.encodeImage(imgContainer.value,
                                        "image/vnd.microsoft.icon",
                                        "format=bmp;bpp=32");
    } catch (e) {
      throw("processIcon - Failure converting icon (" + e + ")");
    }

    if (!this.iconFile.parent.exists())
      this.iconFile.parent.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
    let outputStream = FileUtils.openSafeFileOutputStream(this.iconFile);
    NetUtil.asyncCopy(iconStream, outputStream);
  }
}

#elifdef XP_MACOSX

function MacNativeApp(aData) {
  NativeApp.call(this, aData);
  this._init();
}

MacNativeApp.prototype = {
  __proto__: NativeApp.prototype,

  _init: function() {
    this.appSupportDir = Services.dirsvc.get("ULibDir", Ci.nsILocalFile);
    this.appSupportDir.append("Application Support");

    let filenameRE = new RegExp("[<>:\"/\\\\|\\?\\*]", "gi");
    this.appNameAsFilename = this.appNameAsFilename.replace(filenameRE, "");
    if (this.appNameAsFilename == "") {
      this.appNameAsFilename = "Webapp";
    }

    // The ${ProfileDir} is: sanitized app name + "-" + manifest url hash
    this.appProfileDir = this.appSupportDir.clone();
    this.appProfileDir.append(this.uniqueName);

    this.installDir = Services.dirsvc.get("TmpD", Ci.nsILocalFile);
    this.installDir.append(this.appNameAsFilename + ".app");
    this.installDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0755);

    this.contentsDir = this.installDir.clone();
    this.contentsDir.append("Contents");

    this.macOSDir = this.contentsDir.clone();
    this.macOSDir.append("MacOS");

    this.resourcesDir = this.contentsDir.clone();
    this.resourcesDir.append("Resources");

    this.iconFile = this.resourcesDir.clone();
    this.iconFile.append("appicon.icns");

    // Remove previously installed app (for update purposes)
    this._removeInstallation(true);

    this._createDirectoryStructure();
  },

  install: function() {
    try {
      this._copyPrebuiltFiles();
      this._createConfigFiles();
    } catch (ex) {
      this._removeInstallation(false);
      throw(ex);
    }

    getIconForApp(this, this._moveToApplicationsFolder);
  },

  _removeInstallation: function(keepProfile) {
    let filesToRemove = [this.installDir];

    if (!keepProfile) {
      filesToRemove.push(this.appProfileDir);
    }

    removeFiles(filesToRemove);
  },

  _createDirectoryStructure: function() {
    if (!this.appProfileDir.exists()) {
      this.appProfileDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
    }

    this.contentsDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
    this.macOSDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
    this.resourcesDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
  },

  createAppProfile: function() {
    let profSvc = Cc["@mozilla.org/toolkit/profile-service;1"]
                    .getService(Ci.nsIToolkitProfileService);

    try {
      this.appProfile = profSvc.createDefaultProfileForApp(this.appProfileDir.leafName,
                                                           null, null);
    } catch (ex if ex.result == Cr.NS_ERROR_ALREADY_INITIALIZED) {}
  },

  _copyPrebuiltFiles: function() {
    let webapprt = this.runtimeFolder.clone();
    webapprt.append("webapprt-stub");
    webapprt.copyTo(this.macOSDir, "webapprt");
  },

  _createConfigFiles: function() {
    // ${ProfileDir}/webapp.json
    let configJson = this.appProfileDir.clone();
    configJson.append("webapp.json");
    writeToFile(configJson, JSON.stringify(this.webappJson));

    // ${InstallDir}/Contents/MacOS/webapp.ini
    let applicationINI = this.macOSDir.clone().QueryInterface(Ci.nsILocalFile);
    applicationINI.append("webapp.ini");

    let factory = Cc["@mozilla.org/xpcom/ini-processor-factory;1"]
                    .getService(Ci.nsIINIParserFactory);

    let writer = factory.createINIParser(applicationINI).QueryInterface(Ci.nsIINIParserWriter);
    writer.setString("Webapp", "Name", this.appName);
    writer.setString("Webapp", "Profile", this.appProfileDir.leafName);
    writer.writeFile();
    applicationINI.permissions = FileUtils.PERMS_FILE;

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

    let infoPListFile = this.contentsDir.clone();
    infoPListFile.append("Info.plist");
    writeToFile(infoPListFile, infoPListContent);
  },

  _moveToApplicationsFolder: function() {
    let appDir = Services.dirsvc.get("LocApp", Ci.nsILocalFile);
    let destinationName = getAvailableFileName([appDir],
                                               this.appNameAsFilename,
                                              ".app");
    if (!destinationName) {
      return false;
    }
    this.installDir.moveTo(appDir, destinationName);
  },

  /**
   * This variable specifies if the icon retrieval process should
   * use a temporary file in the system or a binary stream. This
   * is accessed by a common function in WebappsIconHelpers.js and
   * is different for each platform.
   */
  useTmpForIcon: true,

  /**
   * Process the icon from the imageStream as retrieved from
   * the URL by getIconForApp(). This will bundle the icon to the
   * app package at Contents/Resources/appicon.icns.
   *
   * @param aMimeType     the icon mimetype
   * @param aImageStream  the stream for the image data
   * @param aCallback     a callback function to be called
   *                      after the process finishes
   */
  processIcon: function(aMimeType, aIcon, aCallback) {
    try {
      let process = Cc["@mozilla.org/process/util;1"]
                    .createInstance(Ci.nsIProcess);
      let sipsFile = Cc["@mozilla.org/file/local;1"]
                    .createInstance(Ci.nsILocalFile);
      sipsFile.initWithPath("/usr/bin/sips");

      process.init(sipsFile);
      process.run(true, ["-s",
                  "format", "icns",
                  aIcon.path,
                  "--out", this.iconFile.path,
                  "-z", "128", "128"],
                  9);
    } catch(e) {
      throw(e);
    } finally {
      aCallback.call(this);
    }
  }

}

#elifdef XP_UNIX

function LinuxNativeApp(aData) {
  NativeApp.call(this, aData);
  this._init();
}

LinuxNativeApp.prototype = {
  __proto__: NativeApp.prototype,

  _init: function() {
    // The ${InstallDir} and desktop entry filename are: sanitized app name +
    // "-" + manifest url hash

    this.installDir = Services.dirsvc.get("Home", Ci.nsIFile);
    this.installDir.append("." + this.uniqueName);

    this.iconFile = this.installDir.clone();
    this.iconFile.append("icon.png");

    this.webapprt = this.installDir.clone();
    this.webapprt.append("webapprt-stub");

    this.configJson = this.installDir.clone();
    this.configJson.append("webapp.json");

    this.webappINI = this.installDir.clone();
    this.webappINI.append("webapp.ini");

    let env = Cc["@mozilla.org/process/environment;1"]
                .getService(Ci.nsIEnvironment);
    let xdg_data_home_env = env.get("XDG_DATA_HOME");
    if (xdg_data_home_env != "") {
      this.desktopINI = Cc["@mozilla.org/file/local;1"]
                          .createInstance(Ci.nsILocalFile);
      this.desktopINI.initWithPath(xdg_data_home_env);
    }
    else {
      this.desktopINI = Services.dirsvc.get("Home", Ci.nsIFile);
      this.desktopINI.append(".local");
      this.desktopINI.append("share");
    }

    this.desktopINI.append("applications");
    this.desktopINI.append("owa-" + this.uniqueName + ".desktop");

    // Remove previously installed app (for update purposes)
    this._removeInstallation(true);

    this._createDirectoryStructure();
  },

  install: function() {
    try {
      this._copyPrebuiltFiles();
      this._createConfigFiles();
    } catch (ex) {
      this._removeInstallation(false);
      throw(ex);
    }

    getIconForApp(this, function() {});
  },

  _removeInstallation: function(keepProfile) {
    let filesToRemove = [this.desktopINI];

    if (keepProfile) {
      filesToRemove.push(this.iconFile);
      filesToRemove.push(this.webapprt);
      filesToRemove.push(this.configJson);
      filesToRemove.push(this.webappINI);
    } else {
      filesToRemove.push(this.installDir);
    }

    removeFiles(filesToRemove);
  },

  _createDirectoryStructure: function() {
    if (!this.installDir.exists())
      this.installDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
  },

  _copyPrebuiltFiles: function() {
    let webapprtPre = this.runtimeFolder.clone();
    webapprtPre.append(this.webapprt.leafName);
    webapprtPre.copyTo(this.installDir, this.webapprt.leafName);
  },

  createAppProfile: function() {
    let profSvc = Cc["@mozilla.org/toolkit/profile-service;1"]
                    .getService(Ci.nsIToolkitProfileService);

    try {
      this.appProfile = profSvc.createDefaultProfileForApp(this.uniqueName,
                                                           null, null);
    } catch (ex if ex.result == Cr.NS_ERROR_ALREADY_INITIALIZED) {}
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
    writeToFile(this.configJson, JSON.stringify(this.webappJson));

    let factory = Cc["@mozilla.org/xpcom/ini-processor-factory;1"]
                    .getService(Ci.nsIINIParserFactory);

    let webappsBundle = Services.strings.createBundle("chrome://global/locale/webapps.properties");

    // ${InstallDir}/webapp.ini
    let writer = factory.createINIParser(this.webappINI).QueryInterface(Ci.nsIINIParserWriter);
    writer.setString("Webapp", "Name", this.appName);
    writer.setString("Webapp", "Profile", this.uniqueName);
    writer.setString("Webapp", "UninstallMsg", webappsBundle.formatStringFromName("uninstall.notification", [this.appName], 1));
    writer.setString("WebappRT", "InstallDir", this.runtimeFolder.path);
    writer.writeFile();

    // $XDG_DATA_HOME/applications/owa-<webappuniquename>.desktop
    this.desktopINI.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0755);

    writer = factory.createINIParser(this.desktopINI).QueryInterface(Ci.nsIINIParserWriter);
    writer.setString("Desktop Entry", "Name", this.appName);
    writer.setString("Desktop Entry", "Comment", this.shortDescription);
    writer.setString("Desktop Entry", "Exec", '"'+this.webapprt.path+'"');
    writer.setString("Desktop Entry", "Icon", this.iconFile.path);
    writer.setString("Desktop Entry", "Type", "Application");
    writer.setString("Desktop Entry", "Terminal", "false");

    let categories = this._translateCategories();
    if (categories)
      writer.setString("Desktop Entry", "Categories", categories);

    writer.setString("Desktop Entry", "Actions", "Uninstall;");
    writer.setString("Desktop Action Uninstall", "Name", webappsBundle.GetStringFromName("uninstall.label"));
    writer.setString("Desktop Action Uninstall", "Exec", this.webapprt.path + " -remove");

    writer.writeFile();
  },

  /**
   * This variable specifies if the icon retrieval process should
   * use a temporary file in the system or a binary stream. This
   * is accessed by a common function in WebappsIconHelpers.js and
   * is different for each platform.
   */
  useTmpForIcon: false,

  /**
   * Process the icon from the imageStream as retrieved from
   * the URL by getIconForApp().
   *
   * @param aMimeType     ahe icon mimetype
   * @param aImageStream  the stream for the image data
   * @param aCallback     a callback function to be called
   *                      after the process finishes
   */
  processIcon: function(aMimeType, aImageStream, aCallback) {
    let iconStream;
    try {
      let imgTools = Cc["@mozilla.org/image/tools;1"]
                       .createInstance(Ci.imgITools);
      let imgContainer = { value: null };

      imgTools.decodeImageData(aImageStream, aMimeType, imgContainer);
      iconStream = imgTools.encodeImage(imgContainer.value, "image/png");
    } catch (e) {
      throw("processIcon - Failure converting icon (" + e + ")");
    }

    let outputStream = FileUtils.openSafeFileOutputStream(this.iconFile);
    NetUtil.asyncCopy(iconStream, outputStream);
  }
}

#endif

/* Helper Functions */

/**
 * Async write a data string into a file
 *
 * @param aFile     the nsIFile to write to
 * @param aData     a string with the data to be written
 */
function writeToFile(aFile, aData) {
  return Task.spawn(function() {
    let data = new TextEncoder().encode(aData);
    let file = yield OS.File.open(aFile.path, { truncate: true }, { unixMode: FileUtils.PERMS_FILE });
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
 * @param aFolderSet a set of nsIFile objects that represents the set of
 * directories where we want to write
 * @param aName   string with the filename (minus the extension) desired
 * @param aExtension string with the file extension, including the dot

 * @return file name or null if folder is unwritable or unique name
 *         was not available
 */
function getAvailableFileName(aFolderSet, aName, aExtension) {
  let fileSet = [];
  let name = aName + aExtension;
  let isUnique = true;

  // Check if the plain name is a unique name in all the directories.
  for (let folder of aFolderSet) {
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
 * @param aFiles An array with nsIFile objects to be removed
 */
function removeFiles(aFiles) {
  for (let file of aFiles) {
    try {
      if (file.exists()) {
        file.followLinks = false;
        file.remove(true);
      }
    } catch(ex) {}
  }
}

function escapeXML(aStr) {
  return aStr.toString()
             .replace(/&/g, "&amp;")
             .replace(/"/g, "&quot;")
             .replace(/'/g, "&apos;")
             .replace(/</g, "&lt;")
             .replace(/>/g, "&gt;");
}

/* More helpers for handling the app icon */
#include WebappsIconHelpers.js
