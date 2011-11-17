/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const AM_Cc = Components.classes;
const AM_Ci = Components.interfaces;

const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");

const PREF_EM_CHECK_UPDATE_SECURITY   = "extensions.checkUpdateSecurity";
const PREF_EM_STRICT_COMPATIBILITY    = "extensions.strictCompatibility";
const PREF_EM_MIN_COMPAT_APP_VERSION      = "extensions.minCompatibleAppVersion";
const PREF_EM_MIN_COMPAT_PLATFORM_VERSION = "extensions.minCompatiblePlatformVersion";

Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/AddonRepository.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

var gInternalManager = null;
var gAppInfo = null;
var gAddonsList;

var TEST_UNPACKED = false;

function createAppInfo(id, name, version, platformVersion) {
  gAppInfo = {
    // nsIXULAppInfo
    vendor: "Mozilla",
    name: name,
    ID: id,
    version: version,
    appBuildID: "2007010101",
    platformVersion: platformVersion ? platformVersion : "1.0",
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

function do_get_addon_hash(aName, aAlgorithm) {
  if (!aAlgorithm)
    aAlgorithm = "sha1";

  let file = do_get_addon(aName);

  let crypto = AM_Cc["@mozilla.org/security/hash;1"].
               createInstance(AM_Ci.nsICryptoHash);
  crypto.initWithString(aAlgorithm);
  let fis = AM_Cc["@mozilla.org/network/file-input-stream;1"].
            createInstance(AM_Ci.nsIFileInputStream);
  fis.init(file, -1, -1, false);
  crypto.updateFromStream(fis, file.fileSize);

  // return the two-digit hexadecimal code for a byte
  function toHexString(charCode)
    ("0" + charCode.toString(16)).slice(-2);

  let binary = crypto.finish(false);
  return aAlgorithm + ":" + [toHexString(binary.charCodeAt(i)) for (i in binary)].join("")
}

/**
 * Returns an extension uri spec
 *
 * @param  aProfileDir
 *         The extension install directory
 * @return a uri spec pointing to the root of the extension
 */
function do_get_addon_root_uri(aProfileDir, aId) {
  let path = aProfileDir.clone();
  path.append(aId);
  if (!path.exists()) {
    path.leafName += ".xpi";
    return "jar:" + Services.io.newFileURI(path).spec + "!/";
  }
  else {
    return Services.io.newFileURI(path).spec;
  }
}

function do_get_expected_addon_name(aId) {
  if (TEST_UNPACKED)
    return aId;
  return aId + ".xpi";
}

/**
 * Check that an array of actual add-ons is the same as an array of
 * expected add-ons.
 *
 * @param  aActualAddons
 *         The array of actual add-ons to check.
 * @param  aExpectedAddons
 *         The array of expected add-ons to check against.
 * @param  aProperties
 *         An array of properties to check.
 */
function do_check_addons(aActualAddons, aExpectedAddons, aProperties) {
  do_check_neq(aActualAddons, null);
  do_check_eq(aActualAddons.length, aExpectedAddons.length);
  for (let i = 0; i < aActualAddons.length; i++)
    do_check_addon(aActualAddons[i], aExpectedAddons[i], aProperties);
}

/**
 * Check that the actual add-on is the same as the expected add-on.
 *
 * @param  aActualAddon
 *         The actual add-on to check.
 * @param  aExpectedAddon
 *         The expected add-on to check against.
 * @param  aProperties
 *         An array of properties to check.
 */
function do_check_addon(aActualAddon, aExpectedAddon, aProperties) {
  do_check_neq(aActualAddon, null);

  aProperties.forEach(function(aProperty) {
    let actualValue = aActualAddon[aProperty];
    let expectedValue = aExpectedAddon[aProperty];

    // Check that all undefined expected properties are null on actual add-on
    if (!(aProperty in aExpectedAddon)) {
      if (actualValue !== undefined && actualValue !== null)
        do_throw("Unexpected defined/non-null property for add-on " +
                 aExpectedAddon.id + " (addon[" + aProperty + "] = " + actualValue);

      return;
    }

    switch (aProperty) {
      case "creator":
        do_check_author(actualValue, expectedValue);
        break;

      case "developers":
      case "translators":
      case "contributors":
        do_check_eq(actualValue.length, expectedValue.length);
        for (let i = 0; i < actualValue.length; i++)
          do_check_author(actualValue[i], expectedValue[i]);
        break;

      case "screenshots":
        do_check_eq(actualValue.length, expectedValue.length);
        for (let i = 0; i < actualValue.length; i++)
          do_check_screenshot(actualValue[i], expectedValue[i]);
        break;

      case "sourceURI":
        do_check_eq(actualValue.spec, expectedValue);
        break;

      case "updateDate":
        do_check_eq(actualValue.getTime(), expectedValue.getTime());
        break;

      default:
        if (actualValue !== expectedValue)
          do_throw("Failed for " + aProperty + " for add-on " + aExpectedAddon.id +
                   " (" + actualValue + " === " + expectedValue + ")");
    }
  });
}

/**
 * Check that the actual author is the same as the expected author.
 *
 * @param  aActual
 *         The actual author to check.
 * @param  aExpected
 *         The expected author to check against.
 */
function do_check_author(aActual, aExpected) {
  do_check_eq(aActual.toString(), aExpected.name);
  do_check_eq(aActual.name, aExpected.name);
  do_check_eq(aActual.url, aExpected.url);
}

/**
 * Check that the actual screenshot is the same as the expected screenshot.
 *
 * @param  aActual
 *         The actual screenshot to check.
 * @param  aExpected
 *         The expected screenshot to check against.
 */
function do_check_screenshot(aActual, aExpected) {
  do_check_eq(aActual.toString(), aExpected.url);
  do_check_eq(aActual.url, aExpected.url);
  do_check_eq(aActual.width, aExpected.width);
  do_check_eq(aActual.height, aExpected.height);
  do_check_eq(aActual.thumbnailURL, aExpected.thumbnailURL);
  do_check_eq(aActual.thumbnailWidth, aExpected.thumbnailWidth);
  do_check_eq(aActual.thumbnailHeight, aExpected.thumbnailHeight);
  do_check_eq(aActual.caption, aExpected.caption);
}

/**
 * Starts up the add-on manager as if it was started by the application.
 *
 * @param  aAppChanged
 *         An optional boolean parameter to simulate the case where the
 *         application has changed version since the last run. If not passed it
 *         defaults to true
 */
function startupManager(aAppChanged) {
  if (gInternalManager)
    do_throw("Test attempt to startup manager that was already started.");

  if (aAppChanged || aAppChanged === undefined) {
    var file = gProfD.clone();
    file.append("extensions.ini");
    if (file.exists())
      file.remove(true);
  }

  gInternalManager = AM_Cc["@mozilla.org/addons/integration;1"].
                     getService(AM_Ci.nsIObserver).
                     QueryInterface(AM_Ci.nsITimerCallback);

  gInternalManager.observe(null, "addons-startup", null);

  // Load the add-ons list as it was after extension registration
  loadAddonsList();
}

/**
 * Restarts the add-on manager as if the host application was restarted.
 *
 * @param  aNewVersion
 *         An optional new version to use for the application. Passing this
 *         will change nsIXULAppInfo.version and make the startup appear as if
 *         the application version has changed.
 */
function restartManager(aNewVersion) {
  shutdownManager();
  if (aNewVersion) {
    gAppInfo.version = aNewVersion;
    startupManager(true);
  }
  else {
    startupManager(false);
  }
}

function shutdownManager() {
  if (!gInternalManager)
    return;

  let obs = AM_Cc["@mozilla.org/observer-service;1"].
            getService(AM_Ci.nsIObserverService);

  let xpiShutdown = false;
  obs.addObserver({
    observe: function(aSubject, aTopic, aData) {
      xpiShutdown = true;
      obs.removeObserver(this, "xpi-provider-shutdown");
    }
  }, "xpi-provider-shutdown", false);

  let repositoryShutdown = false;
  obs.addObserver({
    observe: function(aSubject, aTopic, aData) {
      repositoryShutdown = true;
      obs.removeObserver(this, "addon-repository-shutdown");
    }
  }, "addon-repository-shutdown", false);

  obs.notifyObservers(null, "quit-application-granted", null);
  gInternalManager.observe(null, "xpcom-shutdown", null);
  gInternalManager = null;

  AddonRepository.shutdown();

  // Load the add-ons list as it was after application shutdown
  loadAddonsList();

  // Clear any crash report annotations
  gAppInfo.annotations = {};

  let thr = AM_Cc["@mozilla.org/thread-manager;1"].
            getService(AM_Ci.nsIThreadManager).
            mainThread;

  // Wait until we observe the shutdown notifications
  while (!repositoryShutdown || !xpiShutdown) {
    if (thr.hasPendingEvents())
      thr.processNextEvent(false);
  }

  // Force the XPIProvider provider to reload since it defines some constants on
  // load that need to change during tests
  let scope = Components.utils.import("resource://gre/modules/XPIProvider.jsm");
  AddonManagerPrivate.unregisterProvider(scope.XPIProvider);
  Components.utils.unload("resource://gre/modules/XPIProvider.jsm");
}

function loadAddonsList() {
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

  var factory = AM_Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
                getService(AM_Ci.nsIINIParserFactory);
  var parser = factory.createINIParser(file);
  gAddonsList.extensions = readDirectories("ExtensionDirs");
  gAddonsList.themes = readDirectories("ThemeDirs");
}

function isItemInAddonsList(aType, aDir, aId) {
  var path = aDir.clone();
  path.append(aId);
  var xpiPath = aDir.clone();
  xpiPath.append(aId + ".xpi");
  for (var i = 0; i < gAddonsList[aType].length; i++) {
    let file = gAddonsList[aType][i];
    if (file.isDirectory() && file.equals(path))
      return true;
    if (file.isFile() && file.equals(xpiPath))
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

function check_startup_changes(aType, aIds) {
  var ids = aIds.slice(0);
  ids.sort();
  var changes = AddonManager.getStartupChanges(aType);
  changes = changes.filter(function(aEl) /@tests.mozilla.org$/.test(aEl));
  changes.sort();

  do_check_eq(JSON.stringify(ids), JSON.stringify(changes));
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

function createInstallRDF(aData) {
  var rdf = '<?xml version="1.0"?>\n';
  rdf += '<RDF xmlns="http://www.w3.org/1999/02/22-rdf-syntax-ns#"\n' +
         '     xmlns:em="http://www.mozilla.org/2004/em-rdf#">\n';
  rdf += '<Description about="urn:mozilla:install-manifest">\n';

  ["id", "version", "type", "internalName", "updateURL", "updateKey",
   "optionsURL", "optionsType", "aboutURL", "iconURL", "icon64URL",
   "skinnable", "bootstrap", "strictCompatibility"].forEach(function(aProp) {
    if (aProp in aData)
      rdf += "<em:" + aProp + ">" + escapeXML(aData[aProp]) + "</em:" + aProp + ">\n";
  });

  rdf += writeLocaleStrings(aData);

  if ("targetPlatforms" in aData) {
    aData.targetPlatforms.forEach(function(aPlatform) {
      rdf += "<em:targetPlatform>" + escapeXML(aPlatform) + "</em:targetPlatform>\n";
    });
  }

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
 * @param   aExtraFile
 *          An optional dummy file to create in the directory
 */
function writeInstallRDFToDir(aData, aDir, aExtraFile) {
  var rdf = createInstallRDF(aData);
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

  if (!aExtraFile)
    return;

  file = aDir.clone();
  file.append(aExtraFile);
  file.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, 0644);
}

/**
 * Writes an install.rdf manifest into an extension using the properties passed
 * in a JS object. The objects should contain a property for each property to
 * appear in the RDFThe object may contain an array of objects with id,
 * minVersion and maxVersion in the targetApplications property to give target
 * application compatibility.
 *
 * @param   aData
 *          The object holding data about the add-on
 * @param   aDir
 *          The install directory to add the extension to
 * @param   aId
 *          An optional string to override the default installation aId
 * @param   aExtraFile
 *          An optional dummy file to create in the extension
 * @return  A file pointing to where the extension was installed
 */
function writeInstallRDFForExtension(aData, aDir, aId, aExtraFile) {
  var id = aId ? aId : aData.id

  var dir = aDir.clone();

  if (TEST_UNPACKED) {
    dir.append(id);
    writeInstallRDFToDir(aData, dir, aExtraFile);
    return dir;
  }

  if (!dir.exists())
    dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
  dir.append(id + ".xpi");
  var rdf = createInstallRDF(aData);
  var stream = AM_Cc["@mozilla.org/io/string-input-stream;1"].
               createInstance(AM_Ci.nsIStringInputStream);
  stream.setData(rdf, -1);
  var zipW = AM_Cc["@mozilla.org/zipwriter;1"].
             createInstance(AM_Ci.nsIZipWriter);
  zipW.open(dir, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE | FileUtils.MODE_TRUNCATE);
  zipW.addEntryStream("install.rdf", 0, AM_Ci.nsIZipWriter.COMPRESSION_NONE,
                      stream, false);
  if (aExtraFile)
    zipW.addEntryStream(aExtraFile, 0, AM_Ci.nsIZipWriter.COMPRESSION_NONE,
                        stream, false);
  zipW.close();
  return dir;
}

/**
 * Sets the last modified time of the extension, usually to trigger an update
 * of its metadata. If the extension is unpacked, this function assumes that
 * the extension contains only the install.rdf file.
 *
 * @param aExt   a file pointing to either the packed extension or its unpacked directory.
 * @param aTime  the time to which we set the lastModifiedTime of the extension
 */
function setExtensionModifiedTime(aExt, aTime) {
  aExt.lastModifiedTime = aTime;
  if (aExt.isDirectory()) {
    let entries = aExt.directoryEntries
                      .QueryInterface(AM_Ci.nsIDirectoryEnumerator);
    while (entries.hasMoreElements())
      setExtensionModifiedTime(entries.nextFile, aTime);
    entries.close();
  }
}

/**
 * Gets the nsIFile for where an add-on is installed. It may point to a file or
 * a directory depending on whether add-ons are being installed unpacked or not.
 *
 * @param  aDir
 *         The nsIFile for the install location
 * @param  aId
 *         The ID of the add-on
 * @return an nsIFile
 */
function getFileForAddon(aDir, aId) {
  var dir = aDir.clone();
  dir.append(do_get_expected_addon_name(aId));
  return dir;
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

function getExpectedInstall(aAddon) {
  if (gExpectedInstalls instanceof Array)
    return gExpectedInstalls.shift();
  if (!aAddon || !aAddon.id)
    return gExpectedInstalls["NO_ID"].shift();
  let id = aAddon.id;
  if (!(id in gExpectedInstalls) || !(gExpectedInstalls[id] instanceof Array))
    do_throw("Wasn't expecting events for " + id);
  if (gExpectedInstalls[id].length == 0)
    do_throw("Too many events for " + id);
  return gExpectedInstalls[id].shift();
}

const AddonListener = {
  onPropertyChanged: function(aAddon, aProperties) {
    let [event, properties] = getExpectedEvent(aAddon.id);
    do_check_eq("onPropertyChanged", event);
    do_check_eq(aProperties.length, properties.length);
    properties.forEach(function(aProperty) {
      // Only test that the expected properties are listed, having additional
      // properties listed is not necessary a problem
      if (aProperties.indexOf(aProperty) == -1)
        do_throw("Did not see property change for " + aProperty);
    });
    return check_test_completed(arguments);
  },

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
    do_check_eq("onNewInstall", getExpectedInstall());
    return check_test_completed(arguments);
  },

  onDownloadStarted: function(install) {
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADING);
    do_check_eq(install.error, 0);
    do_check_eq("onDownloadStarted", getExpectedInstall());
    return check_test_completed(arguments);
  },

  onDownloadEnded: function(install) {
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
    do_check_eq(install.error, 0);
    do_check_eq("onDownloadEnded", getExpectedInstall());
    return check_test_completed(arguments);
  },

  onDownloadFailed: function(install) {
    do_check_eq(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
    do_check_eq("onDownloadFailed", getExpectedInstall());
    return check_test_completed(arguments);
  },

  onDownloadCancelled: function(install) {
    do_check_eq(install.state, AddonManager.STATE_CANCELLED);
    do_check_eq(install.error, 0);
    do_check_eq("onDownloadCancelled", getExpectedInstall());
    return check_test_completed(arguments);
  },

  onInstallStarted: function(install) {
    do_check_eq(install.state, AddonManager.STATE_INSTALLING);
    do_check_eq(install.error, 0);
    do_check_eq("onInstallStarted", getExpectedInstall(install.addon));
    return check_test_completed(arguments);
  },

  onInstallEnded: function(install, newAddon) {
    do_check_eq(install.state, AddonManager.STATE_INSTALLED);
    do_check_eq(install.error, 0);
    do_check_eq("onInstallEnded", getExpectedInstall(install.addon));
    return check_test_completed(arguments);
  },

  onInstallFailed: function(install) {
    do_check_eq(install.state, AddonManager.STATE_INSTALL_FAILED);
    do_check_eq("onInstallFailed", getExpectedInstall(install.addon));
    return check_test_completed(arguments);
  },

  onInstallCancelled: function(install) {
    do_check_eq(install.state, AddonManager.STATE_CANCELLED);
    do_check_eq(install.error, 0);
    do_check_eq("onInstallCancelled", getExpectedInstall(install.addon));
    return check_test_completed(arguments);
  },

  onExternalInstall: function(aAddon, existingAddon, aRequiresRestart) {
    do_check_eq("onExternalInstall", getExpectedInstall(aAddon));
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

  if (gExpectedInstalls instanceof Array &&
      gExpectedInstalls.length > 0)
    return;
  else for each (let installList in gExpectedInstalls) {
    if (installList.length > 0)
      return;
  }

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

// By default don't disable add-ons from any scope
Services.prefs.setIntPref("extensions.autoDisableScopes", 0);

// By default, don't cache add-ons in AddonRepository.jsm
Services.prefs.setBoolPref("extensions.getAddons.cache.enabled", false);

// Disable the compatibility updates window by default
Services.prefs.setBoolPref("extensions.showMismatchUI", false);

// Point update checks to the local machine for fast failures
Services.prefs.setCharPref("extensions.update.url", "http://127.0.0.1/updateURL");
Services.prefs.setCharPref("extensions.blocklist.url", "http://127.0.0.1/blocklistURL");

// By default ignore bundled add-ons
Services.prefs.setBoolPref("extensions.installDistroAddons", false);

// By default use strict compatibility
Services.prefs.setBoolPref("extensions.strictCompatibility", true);

// By default, set min compatible versions to 0
Services.prefs.setCharPref(PREF_EM_MIN_COMPAT_APP_VERSION, "0");
Services.prefs.setCharPref(PREF_EM_MIN_COMPAT_PLATFORM_VERSION, "0");

// Register a temporary directory for the tests.
const gTmpD = gProfD.clone();
gTmpD.append("temp");
gTmpD.create(AM_Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
registerDirectory("TmpD", gTmpD);

// Write out an empty blocklist.xml file to the profile to ensure nothing
// is blocklisted by default
var blockFile = gProfD.clone();
blockFile.append("blocklist.xml");
var stream = AM_Cc["@mozilla.org/network/file-output-stream;1"].
             createInstance(AM_Ci.nsIFileOutputStream);
stream.init(blockFile, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE | FileUtils.MODE_TRUNCATE,
            FileUtils.PERMS_FILE, 0);

var data = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
           "<blocklist xmlns=\"http://www.mozilla.org/2006/addons-blocklist\">\n" +
           "</blocklist>\n";
stream.write(data, data.length);
stream.close();

do_register_cleanup(function() {
  // Check that the temporary directory is empty
  var dirEntries = gTmpD.directoryEntries
                        .QueryInterface(AM_Ci.nsIDirectoryEnumerator);
  var entry;
  while (entry = dirEntries.nextFile) {
    do_throw("Found unexpected file in temporary directory: " + entry.leafName);
  }
  dirEntries.close();

  var testDir = gProfD.clone();
  testDir.append("extensions");
  testDir.append("trash");
  do_check_false(testDir.exists());

  testDir.leafName = "staged";
  do_check_false(testDir.exists());

  testDir.leafName = "staged-xpis";
  do_check_false(testDir.exists());

  shutdownManager();

  // Clear commonly set prefs.
  try {
    Services.prefs.clearUserPref(PREF_EM_CHECK_UPDATE_SECURITY);
  } catch (e) {}
  try {
    Services.prefs.clearUserPref(PREF_EM_STRICT_COMPATIBILITY);
  } catch (e) {}
});
