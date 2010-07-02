/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const AM_Cc = Components.classes;
const AM_Ci = Components.interfaces;

const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");

Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

var gInternalManager = null;
var gAppInfo = null;
var gAddonsList;

function createAppInfo(id, name, version, platformVersion) {
  gAppInfo = {
    // nsIXULAppInfo
    vendor: "Mozilla",
    name: name,
    ID: id,
    version: version,
    appBuildID: "2007010101",
    platformVersion: platformVersion,
    platformBuildID: "2007010101",

    // nsIXULRuntime
    inSafeMode: false,
    logConsoleErrors: true,
    OS: "XPCShell",
    XPCOMABI: "noarch-spidermonkey",
    invalidateCachesOnRestart: function invalidateCachesOnRestart() {
      // Do nothing
    },

    // nsICrashReporter
    annotations: {},

    annotateCrashReport: function(key, data) {
      this.annotations[key] = data;
    },

    QueryInterface: XPCOMUtils.generateQI([AM_Ci.nsIXULAppInfo,
                                           AM_Ci.nsIXULRuntime,
                                           AM_Ci.nsICrashReporter,
                                           AM_Ci.nsISupports])
  };

  var XULAppInfoFactory = {
    createInstance: function (outer, iid) {
      if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      return gAppInfo.QueryInterface(iid);
    }
  };
  var registrar = Components.manager.QueryInterface(AM_Ci.nsIComponentRegistrar);
  registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                            XULAPPINFO_CONTRACTID, XULAppInfoFactory);
}

/**
 * Tests that an add-on does appear in the crash report annotations, if
 * crash reporting is enabled. The test will fail if the add-on is not in the
 * annotation.
 * @param  aId
 *         The ID of the add-on
 * @param  aVersion
 *         The version of the add-on
 */
function do_check_in_crash_annotation(aId, aVersion) {
  if (!("nsICrashReporter" in AM_Ci))
    return;

  if (!("Add-ons" in gAppInfo.annotations)) {
    do_check_false(true);
    return;
  }

  let addons = gAppInfo.annotations["Add-ons"].split(",");
  do_check_false(addons.indexOf(aId + ":" + aVersion) < 0);
}

/**
 * Tests that an add-on does not appear in the crash report annotations, if
 * crash reporting is enabled. The test will fail if the add-on is in the
 * annotation.
 * @param  aId
 *         The ID of the add-on
 * @param  aVersion
 *         The version of the add-on
 */
function do_check_not_in_crash_annotation(aId, aVersion) {
  if (!("nsICrashReporter" in AM_Ci))
    return;

  if (!("Add-ons" in gAppInfo.annotations)) {
    do_check_true(true);
    return;
  }

  let addons = gAppInfo.annotations["Add-ons"].split(",");
  do_check_true(addons.indexOf(aId + ":" + aVersion) < 0);
}

/**
 * Returns a testcase xpi
 *
 * @param  aName
 *         The name of the testcase (without extension)
 * @return an nsILocalFile pointing to the testcase xpi
 */
function do_get_addon(aName) {
  return do_get_file("addons/" + aName + ".xpi");
}

/**
 * Starts up the add-on manager as if it was started by the application. This
 * will simulate any restarts requested by the manager.
 *
 * @param  aExpectedRestarts
 *         An optional parameter to specify the expected number of restarts.
 *         If passed and the number of restarts requested differs then the
 *         test will fail
 * @param  aAppChanged
 *         An optional boolean parameter to simulate the case where the
 *         application has changed version since the last run. If not passed it
 *         defaults to true
 */
function startupManager(aExpectedRestarts, aAppChanged) {
  if (gInternalManager)
    do_throw("Test attempt to startup manager that was already started.");

  if (aAppChanged === undefined)
    aAppChanged = true;

  // Load the add-ons list as it was during application startup
  loadAddonsList(aAppChanged);

  gInternalManager = AM_Cc["@mozilla.org/addons/integration;1"].
                     getService(AM_Ci.nsIObserver).
                     QueryInterface(AM_Ci.nsITimerCallback);

  gInternalManager.observe(null, "addons-startup", null);

  let appStartup = AM_Cc["@mozilla.org/toolkit/app-startup;1"].
                   getService(AM_Ci.nsIAppStartup2);
  var restart = aAppChanged || appStartup.needsRestart;
  appStartup.needsRestart = false;

  if (restart) {
    if (aExpectedRestarts !== undefined)
      restartManager(aExpectedRestarts - 1);
    else
      restartManager();
  }
  else if (aExpectedRestarts !== undefined) {
    if (aExpectedRestarts > 0)
      do_throw("Expected to need to restart " + aExpectedRestarts + " more times");
    else if (aExpectedRestarts < 0)
      do_throw("Restarted " + (-aExpectedRestarts) + " more times than expected");
  }
}

/**
 * Restarts the add-on manager as if the host application was restarted. This
 * will simulate any restarts requested by the manager.
 *
 * @param  aExpectedRestarts
 *         An optional parameter to specify the expected number of restarts.
 *         If passed and the number of restarts requested differs then the
 *         test will fail
 * @param  aNewVersion
 *         An optional new version to use for the application. Passing this
 *         will change nsIXULAppInfo.version and make the startup appear as if
 *         the application version has changed.
 */
function restartManager(aExpectedRestarts, aNewVersion) {
  shutdownManager();
  if (aNewVersion) {
    gAppInfo.version = aNewVersion;
    startupManager(aExpectedRestarts, true);
  }
  else {
    startupManager(aExpectedRestarts, false);
  }
}

function shutdownManager() {
  if (!gInternalManager)
    return;

  let obs = AM_Cc["@mozilla.org/observer-service;1"].
            getService(AM_Ci.nsIObserverService);
  obs.notifyObservers(null, "quit-application-granted", null);
  gInternalManager.observe(null, "xpcom-shutdown", null);
  gInternalManager = null;

  // Load the add-ons list as it was after application shutdown
  loadAddonsList(false);

  // Clear any crash report annotations
  gAppInfo.annotations = {};

  let dbfile = gProfD.clone();
  dbfile.append("extensions.sqlite");

  // If there is no database then it cannot be locked.
  if (!dbfile.exists())
    return;

  let thr = AM_Cc["@mozilla.org/thread-manager;1"].
            getService(AM_Ci.nsIThreadManager).
            mainThread;

  // Wait until we can open a connection to the database
  let db = null;
  while (!db) {
    // Poll for database
    try {
      db = Services.storage.openUnsharedDatabase(dbfile);
    }
    catch (e) {
      if (thr.hasPendingEvents())
        thr.processNextEvent(false);
    }
  }

  // Wait until we can write to the database
  while (db) {
    // Poll for write access
    try {
      db.executeSimpleSQL("PRAGMA user_version = 1");
      db.executeSimpleSQL("PRAGMA user_version = 0");
      db.close();
      db = null;
    }
    catch (e) {
      if (thr.hasPendingEvents())
        thr.processNextEvent(false);
    }
  }
}

function loadAddonsList(aAppChanged) {
  function readDirectories(aSection) {
    var dirs = [];
    var keys = parser.getKeys(aSection);
    while (keys.hasMore()) {
      let descriptor = parser.getString(aSection, keys.getNext());
      try {
        let file = AM_Cc["@mozilla.org/file/local;1"].
                   createInstance(AM_Ci.nsILocalFile);
        file.persistentDescriptor = descriptor;
        dirs.push(file);
      }
      catch (e) {
        // Throws if the directory doesn't exist, we can ignore this since the
        // platform will too.
      }
    }
    return dirs;
  }

  gAddonsList = {
    extensions: [],
    themes: []
  };

  var file = gProfD.clone();
  file.append("extensions.ini");
  if (!file.exists())
    return;
  if (aAppChanged) {
    file.remove(true);
    return;
  }

  var factory = AM_Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
                getService(AM_Ci.nsIINIParserFactory);
  var parser = factory.createINIParser(file);
  gAddonsList.extensions = readDirectories("ExtensionDirs");
  gAddonsList.themes = readDirectories("ThemeDirs");
}

function isItemInAddonsList(aType, aDir, aId) {
  var path = aDir.clone();
  path.append(aId);
  for (var i = 0; i < gAddonsList[aType].length; i++) {
    if (gAddonsList[aType][i].equals(path))
      return true;
  }
  return false;
}

function isThemeInAddonsList(aDir, aId) {
  return isItemInAddonsList("themes", aDir, aId);
}

function isExtensionInAddonsList(aDir, aId) {
  return isItemInAddonsList("extensions", aDir, aId);
}

/**
 * Escapes any occurances of &, ", < or > with XML entities.
 *
 * @param   str
 *          The string to escape
 * @return  The escaped string
 */
function escapeXML(aStr) {
  return aStr.toString()
             .replace(/&/g, "&amp;")
             .replace(/"/g, "&quot;")
             .replace(/</g, "&lt;")
             .replace(/>/g, "&gt;");
}

function writeLocaleStrings(aData) {
  let rdf = "";
  ["name", "description", "creator", "homepageURL"].forEach(function(aProp) {
    if (aProp in aData)
      rdf += "<em:" + aProp + ">" + escapeXML(aData[aProp]) + "</em:" + aProp + ">\n";
  });

  ["developer", "translator", "contributor"].forEach(function(aProp) {
    if (aProp in aData) {
      aData[aProp].forEach(function(aValue) {
        rdf += "<em:" + aProp + ">" + escapeXML(aValue) + "</em:" + aProp + ">\n";
      });
    }
  });
  return rdf;
}

/**
 * Writes an install.rdf manifest into a directory using the properties passed
 * in a JS object. The objects should contain a property for each property to
 * appear in the RDFThe object may contain an array of objects with id,
 * minVersion and maxVersion in the targetApplications property to give target
 * application compatibility.
 *
 * @param   aData
 *          The object holding data about the add-on
 * @param   aDir
 *          The directory to add the install.rdf to
 */
function writeInstallRDFToDir(aData, aDir) {
  var rdf = '<?xml version="1.0"?>\n';
  rdf += '<RDF xmlns="http://www.w3.org/1999/02/22-rdf-syntax-ns#"\n' +
         '     xmlns:em="http://www.mozilla.org/2004/em-rdf#">\n';
  rdf += '<Description about="urn:mozilla:install-manifest">\n';

  ["id", "version", "type", "internalName", "updateURL", "updateKey",
   "optionsURL", "aboutURL", "iconURL"].forEach(function(aProp) {
    if (aProp in aData)
      rdf += "<em:" + aProp + ">" + escapeXML(aData[aProp]) + "</em:" + aProp + ">\n";
  });

  rdf += writeLocaleStrings(aData);

  if ("targetApplications" in aData) {
    aData.targetApplications.forEach(function(aApp) {
      rdf += "<em:targetApplication><Description>\n";
      ["id", "minVersion", "maxVersion"].forEach(function(aProp) {
        if (aProp in aApp)
          rdf += "<em:" + aProp + ">" + escapeXML(aApp[aProp]) + "</em:" + aProp + ">\n";
      });
      rdf += "</Description></em:targetApplication>\n";
    });
  }

  if ("localized" in aData) {
    aData.localized.forEach(function(aLocalized) {
      rdf += "<em:localized><Description>\n";
      if ("locale" in aLocalized) {
        aLocalized.locale.forEach(function(aLocaleName) {
          rdf += "<em:locale>" + escapeXML(aLocaleName) + "</em:locale>\n";
        });
      }
      rdf += writeLocaleStrings(aLocalized);
      rdf += "</Description></em:localized>\n";
    });
  }

  rdf += "</Description>\n</RDF>\n";

  if (!aDir.exists())
    aDir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
  var file = aDir.clone();
  file.append("install.rdf");
  if (file.exists())
    file.remove(true);
  var fos = AM_Cc["@mozilla.org/network/file-output-stream;1"].
            createInstance(AM_Ci.nsIFileOutputStream);
  fos.init(file,
           FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE | FileUtils.MODE_TRUNCATE,
           FileUtils.PERMS_FILE, 0);
  fos.write(rdf, rdf.length);
  fos.close();
}

function registerDirectory(aKey, aDir) {
  var dirProvider = {
    getFile: function(aProp, aPersistent) {
      aPersistent.value = true;
      if (aProp == aKey)
        return aDir.clone();
      return null;
    },

    QueryInterface: XPCOMUtils.generateQI([AM_Ci.nsIDirectoryServiceProvider,
                                           AM_Ci.nsISupports])
  };
  Services.dirsvc.registerProvider(dirProvider);
}

var gExpectedEvents = {};
var gExpectedInstalls = [];
var gNext = null;

function getExpectedEvent(aId) {
  if (!(aId in gExpectedEvents))
    do_throw("Wasn't expecting events for " + aId);
  if (gExpectedEvents[aId].length == 0)
    do_throw("Too many events for " + aId);
  let event = gExpectedEvents[aId].shift();
  if (event instanceof Array)
    return event;
  return [event, true];
}

const AddonListener = {
  onEnabling: function(aAddon, aRequiresRestart) {
    let [event, expectedRestart] = getExpectedEvent(aAddon.id);
    do_check_eq("onEnabling", event);
    do_check_eq(aRequiresRestart, expectedRestart);
    if (expectedRestart)
      do_check_true(hasFlag(aAddon.pendingOperations, AddonManager.PENDING_ENABLE));
    do_check_false(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_ENABLE));
    return check_test_completed(arguments);
  },

  onEnabled: function(aAddon) {
    let [event, expectedRestart] = getExpectedEvent(aAddon.id);
    do_check_eq("onEnabled", event);
    do_check_false(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_ENABLE));
    return check_test_completed(arguments);
  },

  onDisabling: function(aAddon, aRequiresRestart) {
    let [event, expectedRestart] = getExpectedEvent(aAddon.id);
    do_check_eq("onDisabling", event);
    do_check_eq(aRequiresRestart, expectedRestart);
    if (expectedRestart)
      do_check_true(hasFlag(aAddon.pendingOperations, AddonManager.PENDING_DISABLE));
    do_check_false(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_DISABLE));
    return check_test_completed(arguments);
  },

  onDisabled: function(aAddon) {
    let [event, expectedRestart] = getExpectedEvent(aAddon.id);
    do_check_eq("onDisabled", event);
    do_check_false(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_DISABLE));
    return check_test_completed(arguments);
  },

  onInstalling: function(aAddon, aRequiresRestart) {
    let [event, expectedRestart] = getExpectedEvent(aAddon.id);
    do_check_eq("onInstalling", event);
    do_check_eq(aRequiresRestart, expectedRestart);
    if (expectedRestart)
      do_check_true(hasFlag(aAddon.pendingOperations, AddonManager.PENDING_INSTALL));
    return check_test_completed(arguments);
  },

  onInstalled: function(aAddon) {
    let [event, expectedRestart] = getExpectedEvent(aAddon.id);
    do_check_eq("onInstalled", event);
    return check_test_completed(arguments);
  },

  onUninstalling: function(aAddon, aRequiresRestart) {
    let [event, expectedRestart] = getExpectedEvent(aAddon.id);
    do_check_eq("onUninstalling", event);
    do_check_eq(aRequiresRestart, expectedRestart);
    if (expectedRestart)
      do_check_true(hasFlag(aAddon.pendingOperations, AddonManager.PENDING_UNINSTALL));
    return check_test_completed(arguments);
  },

  onUninstalled: function(aAddon) {
    let [event, expectedRestart] = getExpectedEvent(aAddon.id);
    do_check_eq("onUninstalled", event);
    return check_test_completed(arguments);
  },

  onOperationCancelled: function(aAddon) {
    let [event, expectedRestart] = getExpectedEvent(aAddon.id);
    do_check_eq("onOperationCancelled", event);
    return check_test_completed(arguments);
  }
};

const InstallListener = {
  onNewInstall: function(install) {
    if (install.state != AddonManager.STATE_DOWNLOADED &&
        install.state != AddonManager.STATE_AVAILABLE)
      do_throw("Bad install state " + install.state);
    do_check_eq(install.error, 0);
    do_check_eq("onNewInstall", gExpectedInstalls.shift());
    return check_test_completed(arguments);
  },

  onDownloadStarted: function(install) {
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADING);
    do_check_eq(install.error, 0);
    do_check_eq("onDownloadStarted", gExpectedInstalls.shift());
    return check_test_completed(arguments);
  },

  onDownloadEnded: function(install) {
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
    do_check_eq(install.error, 0);
    do_check_eq("onDownloadEnded", gExpectedInstalls.shift());
    return check_test_completed(arguments);
  },

  onDownloadFailed: function(install) {
    do_check_eq(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
    do_check_eq("onDownloadFailed", gExpectedInstalls.shift());
    return check_test_completed(arguments);
  },

  onDownloadCancelled: function(install) {
    do_check_eq(install.state, AddonManager.STATE_CANCELLED);
    do_check_eq(install.error, 0);
    do_check_eq("onDownloadCancelled", gExpectedInstalls.shift());
    return check_test_completed(arguments);
  },

  onInstallStarted: function(install) {
    do_check_eq(install.state, AddonManager.STATE_INSTALLING);
    do_check_eq(install.error, 0);
    do_check_eq("onInstallStarted", gExpectedInstalls.shift());
    return check_test_completed(arguments);
  },

  onInstallEnded: function(install, newAddon) {
    do_check_eq(install.state, AddonManager.STATE_INSTALLED);
    do_check_eq(install.error, 0);
    do_check_eq("onInstallEnded", gExpectedInstalls.shift());
    return check_test_completed(arguments);
  },

  onInstallFailed: function(install) {
    do_check_eq(install.state, AddonManager.STATE_INSTALL_FAILED);
    do_check_eq("onInstallFailed", gExpectedInstalls.shift());
    return check_test_completed(arguments);
  },

  onInstallCancelled: function(install) {
    do_check_eq(install.state, AddonManager.STATE_CANCELLED);
    do_check_eq(install.error, 0);
    do_check_eq("onInstallCancelled", gExpectedInstalls.shift());
    return check_test_completed(arguments);
  },

  onExternalInstall: function(aAddon, existingAddon, aRequiresRestart) {
    do_check_eq("onExternalInstall", gExpectedInstalls.shift());
    do_check_false(aRequiresRestart);
    return check_test_completed(arguments);
  }
};

function hasFlag(aBits, aFlag) {
  return (aBits & aFlag) != 0;
}

// Just a wrapper around setting the expected events
function prepare_test(aExpectedEvents, aExpectedInstalls, aNext) {
  AddonManager.addAddonListener(AddonListener);
  AddonManager.addInstallListener(InstallListener);

  gExpectedInstalls = aExpectedInstalls;
  gExpectedEvents = aExpectedEvents;
  gNext = aNext;
}

// Checks if all expected events have been seen and if so calls the callback
function check_test_completed(aArgs) {
  if (!gNext)
    return;

  if (gExpectedInstalls.length > 0)
    return;

  for (let id in gExpectedEvents) {
    if (gExpectedEvents[id].length > 0)
      return;
  }

  return gNext.apply(null, aArgs);
}

// Verifies that all the expected events for all add-ons were seen
function ensure_test_completed() {
  for (let i in gExpectedEvents) {
    if (gExpectedEvents[i].length > 0)
      do_throw("Didn't see all the expected events for " + i);
  }
  gExpectedEvents = {};
  if (gExpectedInstalls)
    do_check_eq(gExpectedInstalls.length, 0);
}

/**
 * A helper method to install an array of AddonInstall to completion and then
 * call a provided callback.
 *
 * @param   aInstalls
 *          The array of AddonInstalls to install
 * @param   aCallback
 *          The callback to call when all installs have finished
 */
function completeAllInstalls(aInstalls, aCallback) {
  let count = aInstalls.length;

  if (count == 0) {
    aCallback();
    return;
  }

  function installCompleted(aInstall) {
    aInstall.removeListener(listener);

    if (--count == 0)
      aCallback();
  }

  let listener = {
    onDownloadFailed: installCompleted,
    onDownloadCancelled: installCompleted,
    onInstallFailed: installCompleted,
    onInstallCancelled: installCompleted,
    onInstallEnded: installCompleted
  };

  aInstalls.forEach(function(aInstall) {
    aInstall.addListener(listener);
    aInstall.install();
  });
}

/**
 * A helper method to install an array of files and call a callback after the
 * installs are completed.
 *
 * @param   aFiles
 *          The array of files to install
 * @param   aCallback
 *          The callback to call when all installs have finished
 * @param   aIgnoreIncompatible
 *          Optional parameter to ignore add-ons that are incompatible in
 *          aome way with the application
 */
function installAllFiles(aFiles, aCallback, aIgnoreIncompatible) {
  let count = aFiles.length;
  let installs = [];

  aFiles.forEach(function(aFile) {
    AddonManager.getInstallForFile(aFile, function(aInstall) {
      if (!aInstall)
        do_throw("No AddonInstall created for " + aFile.path);
      do_check_eq(aInstall.state, AddonManager.STATE_DOWNLOADED);

      if (!aIgnoreIncompatible || !aInstall.addon.appDisabled)
        installs.push(aInstall);

      if (--count == 0)
        completeAllInstalls(installs, aCallback);
    });
  });
}

if ("nsIWindowsRegKey" in AM_Ci) {
  var MockRegistry = {
    LOCAL_MACHINE: {},
    CURRENT_USER: {},

    setValue: function(aRoot, aPath, aName, aValue) {
      switch (aRoot) {
      case AM_Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE:
        var rootKey = MockRegistry.LOCAL_MACHINE;
        break
      case AM_Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER:
        rootKey = MockRegistry.CURRENT_USER;
        break
      }

      if (!(aPath in rootKey)) {
        rootKey[aPath] = [];
      }
      else {
        for (let i = 0; i < rootKey[aPath].length; i++) {
          if (rootKey[aPath][i].name == aName) {
            if (aValue === null)
              rootKey[aPath].splice(i, 1);
            else
              rootKey[aPath][i].value = aValue;
            return;
          }
        }
      }

      if (aValue === null)
        return;

      rootKey[aPath].push({
        name: aName,
        value: aValue
      });
    }
  };

  /**
   * This is a mock nsIWindowsRegistry implementation. It only implements the
   * methods that the extension manager requires.
   */
  function MockWindowsRegKey() {
  }

  MockWindowsRegKey.prototype = {
    values: null,

    // --- Overridden nsISupports interface functions ---
    QueryInterface: XPCOMUtils.generateQI([AM_Ci.nsIWindowsRegKey]),

    // --- Overridden nsIWindowsRegKey interface functions ---
    open: function(aRootKey, aRelPath, aMode) {
      switch (aRootKey) {
      case AM_Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE:
        var rootKey = MockRegistry.LOCAL_MACHINE;
        break
      case AM_Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER:
        rootKey = MockRegistry.CURRENT_USER;
        break
      }

      if (!(aRelPath in rootKey))
        rootKey[aRelPath] = [];
      this.values = rootKey[aRelPath];
    },

    close: function() {
      this.values = null;
    },

    get valueCount() {
      if (!this.values)
        throw Components.results.NS_ERROR_FAILURE;
      return this.values.length;
    },

    getValueName: function(aIndex) {
      if (!this.values || aIndex >= this.values.length)
        throw Components.results.NS_ERROR_FAILURE;
      return this.values[aIndex].name;
    },

    readStringValue: function(aName) {
      for (let i = 0; i < this.values.length; i++) {
        if (this.values[i].name == aName)
          return this.values[i].value;
      }
    }
  };

  var WinRegFactory = {
    createInstance: function(aOuter, aIid) {
      if (aOuter != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

      var key = new MockWindowsRegKey();
      return key.QueryInterface(aIid);
    }
  };

  var registrar = Components.manager.QueryInterface(AM_Ci.nsIComponentRegistrar);
  registrar.registerFactory(Components.ID("{0478de5b-0f38-4edb-851d-4c99f1ed8eba}"),
                            "Mock Windows Registry Implementation",
                            "@mozilla.org/windows-registry-key;1", WinRegFactory);
}

// Get the profile directory for tests to use.
const gProfD = do_get_profile().QueryInterface(AM_Ci.nsILocalFile);

// Enable more extensive EM logging
Services.prefs.setBoolPref("extensions.logging.enabled", true);

// By default only load extensions from the profile install location
Services.prefs.setIntPref("extensions.enabledScopes", AddonManager.SCOPE_PROFILE);

do_register_cleanup(shutdownManager);
