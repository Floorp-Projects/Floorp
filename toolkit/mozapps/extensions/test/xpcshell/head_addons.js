/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* eslint no-unused-vars: ["error", {vars: "local", args: "none"}] */

var AM_Cc = Components.classes;
var AM_Ci = Components.interfaces;
var AM_Cu = Components.utils;

AM_Cu.importGlobalProperties(["TextEncoder"]);

const CERTDB_CONTRACTID = "@mozilla.org/security/x509certdb;1";
const CERTDB_CID = Components.ID("{fb0bbc5c-452e-4783-b32c-80124693d871}");

const PREF_EM_CHECK_UPDATE_SECURITY   = "extensions.checkUpdateSecurity";
const PREF_EM_STRICT_COMPATIBILITY    = "extensions.strictCompatibility";
const PREF_EM_MIN_COMPAT_APP_VERSION      = "extensions.minCompatibleAppVersion";
const PREF_EM_MIN_COMPAT_PLATFORM_VERSION = "extensions.minCompatiblePlatformVersion";
const PREF_GETADDONS_BYIDS               = "extensions.getAddons.get.url";
const PREF_GETADDONS_BYIDS_PERFORMANCE   = "extensions.getAddons.getWithPerformance.url";
const PREF_XPI_SIGNATURES_REQUIRED    = "xpinstall.signatures.required";
const PREF_SYSTEM_ADDON_SET           = "extensions.systemAddonSet";
const PREF_SYSTEM_ADDON_UPDATE_URL    = "extensions.systemAddon.update.url";
const PREF_APP_UPDATE_ENABLED         = "app.update.enabled";
const PREF_ALLOW_NON_MPC              = "extensions.allow-non-mpc-extensions";
const PREF_DISABLE_SECURITY = ("security.turn_off_all_security_so_that_" +
                               "viruses_can_take_over_this_computer");

// Forcibly end the test if it runs longer than 15 minutes
const TIMEOUT_MS = 900000;

// Maximum error in file modification times. Some file systems don't store
// modification times exactly. As long as we are closer than this then it
// still passes.
const MAX_TIME_DIFFERENCE = 3000;

// Time to reset file modified time relative to Date.now() so we can test that
// times are modified (10 hours old).
const MAKE_FILE_OLD_DIFFERENCE = 10 * 3600 * 1000;

Components.utils.import("resource://gre/modules/addons/AddonRepository.jsm");
Components.utils.import("resource://gre/modules/AppConstants.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");
const { OS } = Components.utils.import("resource://gre/modules/osfile.jsm", {});
Components.utils.import("resource://gre/modules/AsyncShutdown.jsm");

Components.utils.import("resource://testing-common/AddonTestUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Extension",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionTestUtils",
                                  "resource://testing-common/ExtensionXPCShellUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionTestCommon",
                                  "resource://testing-common/ExtensionTestCommon.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "HttpServer",
                                  "resource://testing-common/httpd.js");
XPCOMUtils.defineLazyModuleGetter(this, "MockAsyncShutdown",
                                  "resource://testing-common/AddonTestUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MockRegistrar",
                                  "resource://testing-common/MockRegistrar.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MockRegistry",
                                  "resource://testing-common/MockRegistry.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "aomStartup",
                                   "@mozilla.org/addons/addon-manager-startup;1",
                                   "amIAddonManagerStartup");

const {
  awaitPromise,
  createAppInfo,
  createInstallRDF,
  createTempWebExtensionFile,
  createUpdateRDF,
  getFileForAddon,
  manuallyUninstall,
  overrideBuiltIns,
  promiseAddonEvent,
  promiseCompleteAllInstalls,
  promiseCompleteInstall,
  promiseConsoleOutput,
  promiseFindAddonUpdates,
  promiseInstallAllFiles,
  promiseInstallFile,
  promiseRestartManager,
  promiseSetExtensionModifiedTime,
  promiseShutdownManager,
  promiseStartupManager,
  promiseWebExtensionStartup,
  promiseWriteProxyFileToDir,
  registerDirectory,
  setExtensionModifiedTime,
  writeFilesToZip
} = AddonTestUtils;

function manuallyInstall(...args) {
  return AddonTestUtils.awaitPromise(
    AddonTestUtils.manuallyInstall(...args));
}

// WebExtension wrapper for ease of testing
ExtensionTestUtils.init(this);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

Object.defineProperty(this, "gAppInfo", {
  get() {
    return AddonTestUtils.appInfo;
  },
});

Object.defineProperty(this, "gAddonStartup", {
  get() {
    return AddonTestUtils.addonStartup.clone();
  },
});

Object.defineProperty(this, "gInternalManager", {
  get() {
    return AddonTestUtils.addonIntegrationService.QueryInterface(AM_Ci.nsITimerCallback);
  },
});

Object.defineProperty(this, "gProfD", {
  get() {
    return AddonTestUtils.profileDir.clone();
  },
});

Object.defineProperty(this, "gTmpD", {
  get() {
    return AddonTestUtils.tempDir.clone();
  },
});

Object.defineProperty(this, "gUseRealCertChecks", {
  get() {
    return AddonTestUtils.useRealCertChecks;
  },
  set(val) {
   return AddonTestUtils.useRealCertChecks = val;
  },
});

Object.defineProperty(this, "TEST_UNPACKED", {
  get() {
    return AddonTestUtils.testUnpacked;
  },
  set(val) {
   return AddonTestUtils.testUnpacked = val;
  },
});

// We need some internal bits of AddonManager
var AMscope = Components.utils.import("resource://gre/modules/AddonManager.jsm", {});
var { AddonManager, AddonManagerInternal, AddonManagerPrivate } = AMscope;

const promiseAddonByID = AddonManager.getAddonByID;
const promiseAddonsByIDs = AddonManager.getAddonsByIDs;
const promiseAddonsWithOperationsByTypes = AddonManager.getAddonsWithOperationsByTypes;

var gPort = null;
var gUrlToFileMap = {};

// Map resource://xpcshell-data/ to the data directory
var resHandler = Services.io.getProtocolHandler("resource")
                         .QueryInterface(AM_Ci.nsISubstitutingProtocolHandler);
// Allow non-existent files because of bug 1207735
var dataURI = NetUtil.newURI(do_get_file("data", true));
resHandler.setSubstitution("xpcshell-data", dataURI);

function isManifestRegistered(file) {
  let manifests = Components.manager.getManifestLocations();
  for (let i = 0; i < manifests.length; i++) {
    let manifest = manifests.queryElementAt(i, AM_Ci.nsIURI);

    // manifest is the url to the manifest file either in an XPI or a directory.
    // We want the location of the XPI or directory itself.
    if (manifest instanceof AM_Ci.nsIJARURI) {
      manifest = manifest.JARFile.QueryInterface(AM_Ci.nsIFileURL).file;
    } else if (manifest instanceof AM_Ci.nsIFileURL) {
      manifest = manifest.file.parent;
    } else {
      continue;
    }

    if (manifest.equals(file))
      return true;
  }
  return false;
}

// Listens to messages from bootstrap.js telling us what add-ons were started
// and stopped etc. and performs some sanity checks that only installed add-ons
// are started etc.
this.BootstrapMonitor = {
  inited: false,

  // Contain the current state of add-ons in the system
  installed: new Map(),
  started: new Map(),

  // Contain the last state of shutdown and uninstall calls for an add-on
  stopped: new Map(),
  uninstalled: new Map(),

  startupPromises: [],
  installPromises: [],

  restartfulIds: new Set(),

  init() {
    this.inited = true;
    Services.obs.addObserver(this, "bootstrapmonitor-event");
  },

  shutdownCheck() {
    if (!this.inited)
      return;

    Assert.equal(this.started.size, 0);
  },

  clear(id) {
    this.installed.delete(id);
    this.started.delete(id);
    this.stopped.delete(id);
    this.uninstalled.delete(id);
  },

  promiseAddonStartup(id) {
    return new Promise(resolve => {
      this.startupPromises.push(resolve);
    });
  },

  promiseAddonInstall(id) {
    return new Promise(resolve => {
      this.installPromises.push(resolve);
    });
  },

  checkMatches(cached, current) {
    Assert.notEqual(cached, undefined);
    Assert.equal(current.data.version, cached.data.version);
    Assert.equal(current.data.installPath, cached.data.installPath);
    Assert.equal(current.data.resourceURI, cached.data.resourceURI);
  },

  checkAddonStarted(id, version = undefined) {
    let started = this.started.get(id);
    Assert.notEqual(started, undefined);
    if (version != undefined)
      Assert.equal(started.data.version, version);

    // Chrome should be registered by now
    let installPath = new FileUtils.File(started.data.installPath);
    let isRegistered = isManifestRegistered(installPath);
    Assert.ok(isRegistered);
  },

  checkAddonNotStarted(id) {
    Assert.ok(!this.started.has(id));
  },

  checkAddonInstalled(id, version = undefined) {
    const installed = this.installed.get(id);
    notEqual(installed, undefined);
    if (version !== undefined) {
      equal(installed.data.version, version);
    }
    return installed;
  },

  checkAddonNotInstalled(id) {
    Assert.ok(!this.installed.has(id));
  },

  observe(subject, topic, data) {
    let info = JSON.parse(data);
    let id = info.data.id;
    let installPath = new FileUtils.File(info.data.installPath);

    if (subject && subject.wrappedJSObject) {
      // NOTE: in some of the new tests, we need to received the real objects instead of
      // their JSON representations, but most of the current tests expect intallPath
      // and resourceURI to have been converted to strings.
      info.data = Object.assign({}, subject.wrappedJSObject.data, {
        installPath: info.data.installPath,
        resourceURI: info.data.resourceURI,
      });
    }

    // If this is the install event the add-ons shouldn't already be installed
    if (info.event == "install") {
      this.checkAddonNotInstalled(id);

      this.installed.set(id, info);

      for (let resolve of this.installPromises)
        resolve();
      this.installPromises = [];
    } else {
      this.checkMatches(this.installed.get(id), info);
    }

    // If this is the shutdown event than the add-on should already be started
    if (info.event == "shutdown") {
      this.checkMatches(this.started.get(id), info);

      this.started.delete(id);
      this.stopped.set(id, info);

      // Chrome should still be registered at this point
      let isRegistered = isManifestRegistered(installPath);
      Assert.ok(isRegistered);

      // XPIProvider doesn't bother unregistering chrome on app shutdown but
      // since we simulate restarts we must do so manually to keep the registry
      // consistent.
      if (info.reason == 2 /* APP_SHUTDOWN */)
        Components.manager.removeBootstrappedManifestLocation(installPath);
    } else {
      this.checkAddonNotStarted(id);
    }

    if (info.event == "uninstall") {
      // We currently support registering, but not unregistering,
      // restartful add-on manifests during xpcshell AOM "restarts".
      if (!this.restartfulIds.has(id)) {
        // Chrome should be unregistered at this point
        let isRegistered = isManifestRegistered(installPath);
        Assert.ok(!isRegistered);
      }

      this.installed.delete(id);
      this.uninstalled.set(id, info);
    } else if (info.event == "startup") {
      this.started.set(id, info);

      // Chrome should be registered at this point
      let isRegistered = isManifestRegistered(installPath);
      Assert.ok(isRegistered);

      for (let resolve of this.startupPromises)
        resolve();
      this.startupPromises = [];
    }
  }
};

AddonTestUtils.on("addon-manager-shutdown", () => BootstrapMonitor.shutdownCheck());

function isNightlyChannel() {
  var channel = Services.prefs.getCharPref("app.update.channel", "default");

  return channel != "aurora" && channel != "beta" && channel != "release" && channel != "esr";
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
  if (!AppConstants.MOZ_CRASHREPORTER) {
    return;
  }

  if (!("Add-ons" in gAppInfo.annotations)) {
    Assert.equal(false, true);
    return;
  }

  let addons = gAppInfo.annotations["Add-ons"].split(",");
  Assert.ok(addons.includes(`${encodeURIComponent(aId)}:${encodeURIComponent(aVersion)}`));
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
  if (!AppConstants.MOZ_CRASHREPORTER) {
    return;
  }

  if (!("Add-ons" in gAppInfo.annotations)) {
    Assert.ok(true);
    return;
  }

  let addons = gAppInfo.annotations["Add-ons"].split(",");
  Assert.ok(!addons.includes(`${encodeURIComponent(aId)}:${encodeURIComponent(aVersion)}`));
}

/**
 * Returns a testcase xpi
 *
 * @param  aName
 *         The name of the testcase (without extension)
 * @return an nsIFile pointing to the testcase xpi
 */
function do_get_addon(aName) {
  return do_get_file("addons/" + aName + ".xpi");
}

function do_get_addon_hash(aName, aAlgorithm) {
  let file = do_get_addon(aName);
  return do_get_file_hash(file);
}

function do_get_file_hash(aFile, aAlgorithm) {
  if (!aAlgorithm)
    aAlgorithm = "sha1";

  let crypto = AM_Cc["@mozilla.org/security/hash;1"].
               createInstance(AM_Ci.nsICryptoHash);
  crypto.initWithString(aAlgorithm);
  let fis = AM_Cc["@mozilla.org/network/file-input-stream;1"].
            createInstance(AM_Ci.nsIFileInputStream);
  fis.init(aFile, -1, -1, false);
  crypto.updateFromStream(fis, aFile.fileSize);

  // return the two-digit hexadecimal code for a byte
  let toHexString = charCode => ("0" + charCode.toString(16)).slice(-2);

  let binary = crypto.finish(false);
  let hash = Array.from(binary, c => toHexString(c.charCodeAt(0)));
  return aAlgorithm + ":" + hash.join("");
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
  return Services.io.newFileURI(path).spec;
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
  Assert.notEqual(aActualAddons, null);
  Assert.equal(aActualAddons.length, aExpectedAddons.length);
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
  Assert.notEqual(aActualAddon, null);

  aProperties.forEach(function(aProperty) {
    let actualValue = aActualAddon[aProperty];
    let expectedValue = aExpectedAddon[aProperty];

    // Check that all undefined expected properties are null on actual add-on
    if (!(aProperty in aExpectedAddon)) {
      if (actualValue !== undefined && actualValue !== null) {
        do_throw("Unexpected defined/non-null property for add-on " +
                 aExpectedAddon.id + " (addon[" + aProperty + "] = " +
                 actualValue.toSource() + ")");
      }

      return;
    } else if (expectedValue && !actualValue) {
      do_throw("Missing property for add-on " + aExpectedAddon.id +
        ": expected addon[" + aProperty + "] = " + expectedValue);
      return;
    }

    switch (aProperty) {
      case "creator":
        do_check_author(actualValue, expectedValue);
        break;

      case "developers":
      case "translators":
      case "contributors":
        Assert.equal(actualValue.length, expectedValue.length);
        for (let i = 0; i < actualValue.length; i++)
          do_check_author(actualValue[i], expectedValue[i]);
        break;

      case "screenshots":
        Assert.equal(actualValue.length, expectedValue.length);
        for (let i = 0; i < actualValue.length; i++)
          do_check_screenshot(actualValue[i], expectedValue[i]);
        break;

      case "sourceURI":
        Assert.equal(actualValue.spec, expectedValue);
        break;

      case "updateDate":
        Assert.equal(actualValue.getTime(), expectedValue.getTime());
        break;

      case "compatibilityOverrides":
        Assert.equal(actualValue.length, expectedValue.length);
        for (let i = 0; i < actualValue.length; i++)
          do_check_compatibilityoverride(actualValue[i], expectedValue[i]);
        break;

      case "icons":
        do_check_icons(actualValue, expectedValue);
        break;

      default:
        if (remove_port(actualValue) !== remove_port(expectedValue))
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
  Assert.equal(aActual.toString(), aExpected.name);
  Assert.equal(aActual.name, aExpected.name);
  Assert.equal(aActual.url, aExpected.url);
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
  Assert.equal(aActual.toString(), aExpected.url);
  Assert.equal(aActual.url, aExpected.url);
  Assert.equal(aActual.width, aExpected.width);
  Assert.equal(aActual.height, aExpected.height);
  Assert.equal(aActual.thumbnailURL, aExpected.thumbnailURL);
  Assert.equal(aActual.thumbnailWidth, aExpected.thumbnailWidth);
  Assert.equal(aActual.thumbnailHeight, aExpected.thumbnailHeight);
  Assert.equal(aActual.caption, aExpected.caption);
}

/**
 * Check that the actual compatibility override is the same as the expected
 * compatibility override.
 *
 * @param  aAction
 *         The actual compatibility override to check.
 * @param  aExpected
 *         The expected compatibility override to check against.
 */
function do_check_compatibilityoverride(aActual, aExpected) {
  Assert.equal(aActual.type, aExpected.type);
  Assert.equal(aActual.minVersion, aExpected.minVersion);
  Assert.equal(aActual.maxVersion, aExpected.maxVersion);
  Assert.equal(aActual.appID, aExpected.appID);
  Assert.equal(aActual.appMinVersion, aExpected.appMinVersion);
  Assert.equal(aActual.appMaxVersion, aExpected.appMaxVersion);
}

function do_check_icons(aActual, aExpected) {
  for (var size in aExpected) {
    Assert.equal(remove_port(aActual[size]), remove_port(aExpected[size]));
  }
}

function startupManager(aAppChanged) {
  promiseStartupManager(aAppChanged);
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
  awaitPromise(promiseRestartManager(aNewVersion));
}

function shutdownManager() {
  awaitPromise(promiseShutdownManager());
}

function isItemMarkedMPIncompatible(aId) {
  return AddonTestUtils.addonsList.isMultiprocessIncompatible(aId);
}

function isThemeInAddonsList(aDir, aId) {
  return AddonTestUtils.addonsList.hasTheme(aDir, aId);
}

function isExtensionInAddonsList(aDir, aId) {
  return AddonTestUtils.addonsList.hasExtension(aDir, aId);
}

function check_startup_changes(aType, aIds) {
  var ids = aIds.slice(0);
  ids.sort();
  var changes = AddonManager.getStartupChanges(aType);
  changes = changes.filter(aEl => /@tests.mozilla.org$/.test(aEl));
  changes.sort();

  Assert.equal(JSON.stringify(ids), JSON.stringify(changes));
}

/**
 * Writes an install.rdf manifest into a directory using the properties passed
 * in a JS object. The objects should contain a property for each property to
 * appear in the RDF. The object may contain an array of objects with id,
 * minVersion and maxVersion in the targetApplications property to give target
 * application compatibility.
 *
 * @param   aData
 *          The object holding data about the add-on
 * @param   aDir
 *          The directory to add the install.rdf to
 * @param   aId
 *          An optional string to override the default installation aId
 * @param   aExtraFile
 *          An optional dummy file to create in the directory
 * @return  An nsIFile for the directory in which the add-on is installed.
 */
function writeInstallRDFToDir(aData, aDir, aId = aData.id, aExtraFile = null) {
  let files = {
    "install.rdf": AddonTestUtils.createInstallRDF(aData),
  };
  if (aExtraFile)
    files[aExtraFile] = "";

  let dir = aDir.clone();
  dir.append(aId);

  awaitPromise(AddonTestUtils.promiseWriteFilesToDir(dir.path, files));
  return dir;
}

/**
 * Writes an install.rdf manifest into a packed extension using the properties passed
 * in a JS object. The objects should contain a property for each property to
 * appear in the RDF. The object may contain an array of objects with id,
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
function writeInstallRDFToXPI(aData, aDir, aId = aData.id, aExtraFile = null) {
  let files = {
    "install.rdf": AddonTestUtils.createInstallRDF(aData),
  };
  if (aExtraFile)
    files[aExtraFile] = "";

  if (!aDir.exists())
    aDir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  var file = aDir.clone();
  file.append(`${aId}.xpi`);

  AddonTestUtils.writeFilesToZip(file.path, files);

  return file;
}

/**
 * Writes an install.rdf manifest into an extension using the properties passed
 * in a JS object. The objects should contain a property for each property to
 * appear in the RDF. The object may contain an array of objects with id,
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
  if (TEST_UNPACKED) {
    return writeInstallRDFToDir(aData, aDir, aId, aExtraFile);
  }
  return writeInstallRDFToXPI(aData, aDir, aId, aExtraFile);
}

/**
 * Writes a manifest.json manifest into an extension using the properties passed
 * in a JS object.
 *
 * @param   aManifest
 *          The data to write
 * @param   aDir
 *          The install directory to add the extension to
 * @param   aId
 *          An optional string to override the default installation aId
 * @return  A file pointing to where the extension was installed
 */
function promiseWriteWebManifestForExtension(aData, aDir, aId = aData.applications.gecko.id) {
  let files = {
    "manifest.json": JSON.stringify(aData),
  };
  return AddonTestUtils.promiseWriteFilesToExtension(aDir.path, aId, files);
}

/**
 * Creates an XPI file for some manifest data in the temporary directory and
 * returns the nsIFile for it. The file will be deleted when the test completes.
 *
 * @param   aData
 *          The object holding data about the add-on
 * @return  A file pointing to the created XPI file
 */
function createTempXPIFile(aData, aExtraFile) {
  let files = {
    "install.rdf": aData,
  };
  if (typeof aExtraFile == "object")
    Object.assign(files, aExtraFile);
  else if (aExtraFile)
    files[aExtraFile] = "";

  return AddonTestUtils.createTempXPIFile(files);
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
    return gExpectedInstalls.NO_ID.shift();
  let id = aAddon.id;
  if (!(id in gExpectedInstalls) || !(gExpectedInstalls[id] instanceof Array))
    do_throw("Wasn't expecting events for " + id);
  if (gExpectedInstalls[id].length == 0)
    do_throw("Too many events for " + id);
  return gExpectedInstalls[id].shift();
}

const AddonListener = {
  onPropertyChanged(aAddon, aProperties) {
    info(`Got onPropertyChanged event for ${aAddon.id}`);
    let [event, properties] = getExpectedEvent(aAddon.id);
    Assert.equal("onPropertyChanged", event);
    Assert.equal(aProperties.length, properties.length);
    properties.forEach(function(aProperty) {
      // Only test that the expected properties are listed, having additional
      // properties listed is not necessary a problem
      if (aProperties.indexOf(aProperty) == -1)
        do_throw("Did not see property change for " + aProperty);
    });
    return check_test_completed(arguments);
  },

  onEnabling(aAddon, aRequiresRestart) {
    info(`Got onEnabling event for ${aAddon.id}`);
    let [event, expectedRestart] = getExpectedEvent(aAddon.id);
    Assert.equal("onEnabling", event);
    Assert.equal(aRequiresRestart, expectedRestart);
    if (expectedRestart)
      Assert.ok(hasFlag(aAddon.pendingOperations, AddonManager.PENDING_ENABLE));
    Assert.ok(!hasFlag(aAddon.permissions, AddonManager.PERM_CAN_ENABLE));
    return check_test_completed(arguments);
  },

  onEnabled(aAddon) {
    info(`Got onEnabled event for ${aAddon.id}`);
    let [event] = getExpectedEvent(aAddon.id);
    Assert.equal("onEnabled", event);
    Assert.ok(!hasFlag(aAddon.permissions, AddonManager.PERM_CAN_ENABLE));
    return check_test_completed(arguments);
  },

  onDisabling(aAddon, aRequiresRestart) {
    info(`Got onDisabling event for ${aAddon.id}`);
    let [event, expectedRestart] = getExpectedEvent(aAddon.id);
    Assert.equal("onDisabling", event);
    Assert.equal(aRequiresRestart, expectedRestart);
    if (expectedRestart)
      Assert.ok(hasFlag(aAddon.pendingOperations, AddonManager.PENDING_DISABLE));
    Assert.ok(!hasFlag(aAddon.permissions, AddonManager.PERM_CAN_DISABLE));
    return check_test_completed(arguments);
  },

  onDisabled(aAddon) {
    info(`Got onDisabled event for ${aAddon.id}`);
    let [event] = getExpectedEvent(aAddon.id);
    Assert.equal("onDisabled", event);
    Assert.ok(!hasFlag(aAddon.permissions, AddonManager.PERM_CAN_DISABLE));
    return check_test_completed(arguments);
  },

  onInstalling(aAddon, aRequiresRestart) {
    info(`Got onInstalling event for ${aAddon.id}`);
    let [event, expectedRestart] = getExpectedEvent(aAddon.id);
    Assert.equal("onInstalling", event);
    Assert.equal(aRequiresRestart, expectedRestart);
    if (expectedRestart)
      Assert.ok(hasFlag(aAddon.pendingOperations, AddonManager.PENDING_INSTALL));
    return check_test_completed(arguments);
  },

  onInstalled(aAddon) {
    info(`Got onInstalled event for ${aAddon.id}`);
    let [event] = getExpectedEvent(aAddon.id);
    Assert.equal("onInstalled", event);
    return check_test_completed(arguments);
  },

  onUninstalling(aAddon, aRequiresRestart) {
    info(`Got onUninstalling event for ${aAddon.id}`);
    let [event, expectedRestart] = getExpectedEvent(aAddon.id);
    Assert.equal("onUninstalling", event);
    Assert.equal(aRequiresRestart, expectedRestart);
    if (expectedRestart)
      Assert.ok(hasFlag(aAddon.pendingOperations, AddonManager.PENDING_UNINSTALL));
    return check_test_completed(arguments);
  },

  onUninstalled(aAddon) {
    info(`Got onUninstalled event for ${aAddon.id}`);
    let [event] = getExpectedEvent(aAddon.id);
    Assert.equal("onUninstalled", event);
    return check_test_completed(arguments);
  },

  onOperationCancelled(aAddon) {
    info(`Got onOperationCancelled event for ${aAddon.id}`);
    let [event] = getExpectedEvent(aAddon.id);
    Assert.equal("onOperationCancelled", event);
    return check_test_completed(arguments);
  }
};

const InstallListener = {
  onNewInstall(install) {
    if (install.state != AddonManager.STATE_DOWNLOADED &&
        install.state != AddonManager.STATE_DOWNLOAD_FAILED &&
        install.state != AddonManager.STATE_AVAILABLE)
      do_throw("Bad install state " + install.state);
    if (install.state != AddonManager.STATE_DOWNLOAD_FAILED)
      Assert.equal(install.error, 0);
    else
      Assert.notEqual(install.error, 0);
    Assert.equal("onNewInstall", getExpectedInstall());
    return check_test_completed(arguments);
  },

  onDownloadStarted(install) {
    Assert.equal(install.state, AddonManager.STATE_DOWNLOADING);
    Assert.equal(install.error, 0);
    Assert.equal("onDownloadStarted", getExpectedInstall());
    return check_test_completed(arguments);
  },

  onDownloadEnded(install) {
    Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);
    Assert.equal(install.error, 0);
    Assert.equal("onDownloadEnded", getExpectedInstall());
    return check_test_completed(arguments);
  },

  onDownloadFailed(install) {
    Assert.equal(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
    Assert.equal("onDownloadFailed", getExpectedInstall());
    return check_test_completed(arguments);
  },

  onDownloadCancelled(install) {
    Assert.equal(install.state, AddonManager.STATE_CANCELLED);
    Assert.equal(install.error, 0);
    Assert.equal("onDownloadCancelled", getExpectedInstall());
    return check_test_completed(arguments);
  },

  onInstallStarted(install) {
    Assert.equal(install.state, AddonManager.STATE_INSTALLING);
    Assert.equal(install.error, 0);
    Assert.equal("onInstallStarted", getExpectedInstall(install.addon));
    return check_test_completed(arguments);
  },

  onInstallEnded(install, newAddon) {
    Assert.equal(install.state, AddonManager.STATE_INSTALLED);
    Assert.equal(install.error, 0);
    Assert.equal("onInstallEnded", getExpectedInstall(install.addon));
    return check_test_completed(arguments);
  },

  onInstallFailed(install) {
    Assert.equal(install.state, AddonManager.STATE_INSTALL_FAILED);
    Assert.equal("onInstallFailed", getExpectedInstall(install.addon));
    return check_test_completed(arguments);
  },

  onInstallCancelled(install) {
    // If the install was cancelled by a listener returning false from
    // onInstallStarted, then the state will revert to STATE_DOWNLOADED.
    let possibleStates = [AddonManager.STATE_CANCELLED,
                          AddonManager.STATE_DOWNLOADED];
    Assert.ok(possibleStates.indexOf(install.state) != -1);
    Assert.equal(install.error, 0);
    Assert.equal("onInstallCancelled", getExpectedInstall(install.addon));
    return check_test_completed(arguments);
  },

  onExternalInstall(aAddon, existingAddon, aRequiresRestart) {
    Assert.equal("onExternalInstall", getExpectedInstall(aAddon));
    Assert.ok(!aRequiresRestart);
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
    return undefined;

  if (gExpectedInstalls instanceof Array &&
      gExpectedInstalls.length > 0)
    return undefined;

  for (let id in gExpectedInstalls) {
    let installList = gExpectedInstalls[id];
    if (installList.length > 0)
      return undefined;
  }

  for (let id in gExpectedEvents) {
    if (gExpectedEvents[id].length > 0)
      return undefined;
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
    Assert.equal(gExpectedInstalls.length, 0);
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
  promiseCompleteAllInstalls(aInstalls).then(aCallback);
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
  promiseInstallAllFiles(aFiles, aIgnoreIncompatible).then(aCallback);
}

const EXTENSIONS_DB = "extensions.json";
var gExtensionsJSON = gProfD.clone();
gExtensionsJSON.append(EXTENSIONS_DB);

function promiseInstallWebExtension(aData) {
  let addonFile = createTempWebExtensionFile(aData);

  return promiseInstallAllFiles([addonFile]).then(installs => {
    Services.obs.notifyObservers(addonFile, "flush-cache-entry");
    // Since themes are disabled by default, it won't start up.
    if (aData.manifest.theme)
      return installs[0].addon;
    return promiseWebExtensionStartup();
  });
}

// By default use strict compatibility
Services.prefs.setBoolPref("extensions.strictCompatibility", true);

// By default, set min compatible versions to 0
Services.prefs.setCharPref(PREF_EM_MIN_COMPAT_APP_VERSION, "0");
Services.prefs.setCharPref(PREF_EM_MIN_COMPAT_PLATFORM_VERSION, "0");

// Ensure signature checks are enabled by default
Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, true);

// Allow non-multiprocessCompatible extensions for now
Services.prefs.setBoolPref(PREF_ALLOW_NON_MPC, true);

Services.prefs.setBoolPref("extensions.legacy.enabled", true);


// Copies blocklistFile (an nsIFile) to gProfD/blocklist.xml.
function copyBlocklistToProfile(blocklistFile) {
  var dest = gProfD.clone();
  dest.append("blocklist.xml");
  if (dest.exists())
    dest.remove(false);
  blocklistFile.copyTo(gProfD, "blocklist.xml");
  dest.lastModifiedTime = Date.now();
}

// Throw a failure and attempt to abandon the test if it looks like it is going
// to timeout
function timeout() {
  timer = null;
  do_throw("Test ran longer than " + TIMEOUT_MS + "ms");

  // Attempt to bail out of the test
  do_test_finished();
}

var timer = AM_Cc["@mozilla.org/timer;1"].createInstance(AM_Ci.nsITimer);
timer.init(timeout, TIMEOUT_MS, AM_Ci.nsITimer.TYPE_ONE_SHOT);

// Make sure that a given path does not exist
function pathShouldntExist(file) {
  if (file.exists()) {
    do_throw(`Test cleanup: path ${file.path} exists when it should not`);
  }
}

registerCleanupFunction(function addon_cleanup() {
  if (timer)
    timer.cancel();
});

/**
 * Creates a new HttpServer for testing, and begins listening on the
 * specified port. Automatically shuts down the server when the test
 * unit ends.
 *
 * @param port
 *        The port to listen on. If omitted, listen on a random
 *        port. The latter is the preferred behavior.
 *
 * @return HttpServer
 */
function createHttpServer(port = -1) {
  let server = new HttpServer();
  server.start(port);

  registerCleanupFunction(() => {
    return new Promise(resolve => {
      server.stop(resolve);
    });
  });

  return server;
}

/**
 * Handler function that responds with the interpolated
 * static file associated to the URL specified by request.path.
 * This replaces the %PORT% entries in the file with the actual
 * value of the running server's port (stored in gPort).
 */
function interpolateAndServeFile(request, response) {
  try {
    let file = gUrlToFileMap[request.path];
    var data = "";
    var fstream = Components.classes["@mozilla.org/network/file-input-stream;1"].
    createInstance(Components.interfaces.nsIFileInputStream);
    var cstream = Components.classes["@mozilla.org/intl/converter-input-stream;1"].
    createInstance(Components.interfaces.nsIConverterInputStream);
    fstream.init(file, -1, 0, 0);
    cstream.init(fstream, "UTF-8", 0, 0);

    let str = {};
    let read = 0;
    do {
      // read as much as we can and put it in str.value
      read = cstream.readString(0xffffffff, str);
      data += str.value;
    } while (read != 0);
    data = data.replace(/%PORT%/g, gPort);

    response.write(data);
  } catch (e) {
    do_throw(`Exception while serving interpolated file: ${e}\n${e.stack}`);
  } finally {
    cstream.close(); // this closes fstream as well
  }
}

/**
 * Sets up a path handler for the given URL and saves the
 * corresponding file in the global url -> file map.
 *
 * @param  url
 *         the actual URL
 * @param  file
 *         nsIFile representing a static file
 */
function mapUrlToFile(url, file, server) {
  server.registerPathHandler(url, interpolateAndServeFile);
  gUrlToFileMap[url] = file;
}

function mapFile(path, server) {
  mapUrlToFile(path, do_get_file(path), server);
}

/**
 * Take out the port number in an URL
 *
 * @param url
 *        String that represents an URL with a port number in it
 */
function remove_port(url) {
  if (typeof url === "string")
    return url.replace(/:\d+/, "");
  return url;
}
// Wrap a function (typically a callback) to catch and report exceptions
function do_exception_wrap(func) {
  return function() {
    try {
      func.apply(null, arguments);
    } catch (e) {
      do_report_unexpected_exception(e);
    }
  };
}

/**
 * Change the schema version of the JSON extensions database
 */
function changeXPIDBVersion(aNewVersion, aMutator = undefined) {
  let jData = loadJSON(gExtensionsJSON);
  jData.schemaVersion = aNewVersion;
  if (aMutator)
    aMutator(jData);
  saveJSON(jData, gExtensionsJSON);
}

/**
 * Load a file into a string
 */
function loadFile(aFile) {
  let data = "";
  let fstream = Components.classes["@mozilla.org/network/file-input-stream;1"].
          createInstance(Components.interfaces.nsIFileInputStream);
  let cstream = Components.classes["@mozilla.org/intl/converter-input-stream;1"].
          createInstance(Components.interfaces.nsIConverterInputStream);
  fstream.init(aFile, -1, 0, 0);
  cstream.init(fstream, "UTF-8", 0, 0);
  let str = {};
  let read = 0;
  do {
    read = cstream.readString(0xffffffff, str); // read as much as we can and put it in str.value
    data += str.value;
  } while (read != 0);
  cstream.close();
  return data;
}

/**
 * Raw load of a JSON file
 */
function loadJSON(aFile) {
  let data = loadFile(aFile);
  info("Loaded JSON file " + aFile.path);
  return (JSON.parse(data));
}

/**
 * Raw save of a JSON blob to file
 */
function saveJSON(aData, aFile) {
  info("Starting to save JSON file " + aFile.path);
  let stream = FileUtils.openSafeFileOutputStream(aFile);
  let converter = AM_Cc["@mozilla.org/intl/converter-output-stream;1"].
    createInstance(AM_Ci.nsIConverterOutputStream);
  converter.init(stream, "UTF-8");
  // XXX pretty print the JSON while debugging
  converter.writeString(JSON.stringify(aData, null, 2));
  converter.flush();
  // nsConverterOutputStream doesn't finish() safe output streams on close()
  FileUtils.closeSafeFileOutputStream(stream);
  converter.close();
  info("Done saving JSON file " + aFile.path);
}

/**
 * Create a callback function that calls do_execute_soon on an actual callback and arguments
 */
function callback_soon(aFunction) {
  return function(...args) {
    executeSoon(function() {
      aFunction.apply(null, args);
    }, aFunction.name ? "delayed callback " + aFunction.name : "delayed callback");
  };
}

function writeProxyFileToDir(aDir, aAddon, aId) {
  awaitPromise(promiseWriteProxyFileToDir(aDir, aAddon, aId));

  let file = aDir.clone();
  file.append(aId);
  return file;
}

async function serveSystemUpdate(xml, perform_update, testserver) {
  testserver.registerPathHandler("/data/update.xml", (request, response) => {
    response.write(xml);
  });

  try {
    await perform_update();
  } finally {
    testserver.registerPathHandler("/data/update.xml", null);
  }
}

// Runs an update check making it use the passed in xml string. Uses the direct
// call to the update function so we get rejections on failure.
async function installSystemAddons(xml, testserver) {
  info("Triggering system add-on update check.");

  await serveSystemUpdate(xml, async function() {
    let { XPIProvider } = Components.utils.import("resource://gre/modules/addons/XPIProvider.jsm", {});
    await XPIProvider.updateSystemAddons();
  }, testserver);
}

// Runs a full add-on update check which will in some cases do a system add-on
// update check. Always succeeds.
async function updateAllSystemAddons(xml, testserver) {
  info("Triggering full add-on update check.");

  await serveSystemUpdate(xml, function() {
    return new Promise(resolve => {
      Services.obs.addObserver(function observer() {
        Services.obs.removeObserver(observer, "addons-background-update-complete");

        resolve();
      }, "addons-background-update-complete");

      // Trigger the background update timer handler
      gInternalManager.notify(null);
    });
  }, testserver);
}

// Builds an update.xml file for an update check based on the data passed.
function buildSystemAddonUpdates(addons, root) {
  let xml = `<?xml version="1.0" encoding="UTF-8"?>\n\n<updates>\n`;
  if (addons) {
    xml += `  <addons>\n`;
    for (let addon of addons) {
      xml += `    <addon id="${addon.id}" URL="${root + addon.path}" version="${addon.version}"`;
      if (addon.size)
        xml += ` size="${addon.size}"`;
      if (addon.hashFunction)
        xml += ` hashFunction="${addon.hashFunction}"`;
      if (addon.hashValue)
        xml += ` hashValue="${addon.hashValue}"`;
      xml += `/>\n`;
    }
    xml += `  </addons>\n`;
  }
  xml += `</updates>\n`;

  return xml;
}

function initSystemAddonDirs() {
  let hiddenSystemAddonDir = FileUtils.getDir("ProfD", ["sysfeatures", "hidden"], true);
  do_get_file("data/system_addons/system1_1.xpi").copyTo(hiddenSystemAddonDir, "system1@tests.mozilla.org.xpi");
  do_get_file("data/system_addons/system2_1.xpi").copyTo(hiddenSystemAddonDir, "system2@tests.mozilla.org.xpi");

  let prefilledSystemAddonDir = FileUtils.getDir("ProfD", ["sysfeatures", "prefilled"], true);
  do_get_file("data/system_addons/system2_2.xpi").copyTo(prefilledSystemAddonDir, "system2@tests.mozilla.org.xpi");
  do_get_file("data/system_addons/system3_2.xpi").copyTo(prefilledSystemAddonDir, "system3@tests.mozilla.org.xpi");
}

/**
 * Returns current system add-on update directory (stored in pref).
 */
function getCurrentSystemAddonUpdatesDir() {
  const updatesDir = FileUtils.getDir("ProfD", ["features"], false);
  let dir = updatesDir.clone();
  let set = JSON.parse(Services.prefs.getCharPref(PREF_SYSTEM_ADDON_SET));
  dir.append(set.directory);
  return dir;
}

/**
 * Removes all files from system add-on update directory.
 */
function clearSystemAddonUpdatesDir() {
  const updatesDir = FileUtils.getDir("ProfD", ["features"], false);
  // Delete any existing directories
  if (updatesDir.exists())
    updatesDir.remove(true);

  Services.prefs.clearUserPref(PREF_SYSTEM_ADDON_SET);
}

/**
 * Installs a known set of add-ons into the system add-on update directory.
 */
function buildPrefilledUpdatesDir() {
  clearSystemAddonUpdatesDir();

  // Build the test set
  let dir = FileUtils.getDir("ProfD", ["features", "prefilled"], true);

  do_get_file("data/system_addons/system2_2.xpi").copyTo(dir, "system2@tests.mozilla.org.xpi");
  do_get_file("data/system_addons/system3_2.xpi").copyTo(dir, "system3@tests.mozilla.org.xpi");

  // Mark these in the past so the startup file scan notices when files have changed properly
  FileUtils.getFile("ProfD", ["features", "prefilled", "system2@tests.mozilla.org.xpi"]).lastModifiedTime -= 10000;
  FileUtils.getFile("ProfD", ["features", "prefilled", "system3@tests.mozilla.org.xpi"]).lastModifiedTime -= 10000;

  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, JSON.stringify({
    schema: 1,
    directory: dir.leafName,
    addons: {
      "system2@tests.mozilla.org": {
        version: "2.0"
      },
      "system3@tests.mozilla.org": {
        version: "2.0"
      },
    }
  }));
}

/**
 * Check currently installed ssystem add-ons against a set of conditions.
 *
 * @param {Array<Object>} conditions - an array of objects of the form { isUpgrade: false, version: null}
 * @param {nsIFile} distroDir - the system add-on distribution directory (the "features" dir in the app directory)
 */
async function checkInstalledSystemAddons(conditions, distroDir) {
  for (let i = 0; i < conditions.length; i++) {
    let condition = conditions[i];
    let id = "system" + (i + 1) + "@tests.mozilla.org";
    let addon = await promiseAddonByID(id);

    if (!("isUpgrade" in condition) || !("version" in condition)) {
      throw Error("condition must contain isUpgrade and version");
    }
    let isUpgrade = conditions[i].isUpgrade;
    let version = conditions[i].version;

    let expectedDir = isUpgrade ? getCurrentSystemAddonUpdatesDir() : distroDir;

    if (version) {
      info(`Checking state of add-on ${id}, expecting version ${version}`);

      // Add-on should be installed
      Assert.notEqual(addon, null);
      Assert.equal(addon.version, version);
      Assert.ok(addon.isActive);
      Assert.ok(!addon.foreignInstall);
      Assert.ok(addon.hidden);
      Assert.ok(addon.isSystem);

      // Verify the add-ons file is in the right place
      let file = expectedDir.clone();
      file.append(id + ".xpi");
      Assert.ok(file.exists());
      Assert.ok(file.isFile());

      let uri = addon.getResourceURI(null);
      Assert.ok(uri instanceof AM_Ci.nsIFileURL);
      Assert.equal(uri.file.path, file.path);

      if (isUpgrade) {
        Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SYSTEM);
      }

      // Verify the add-on actually started
      BootstrapMonitor.checkAddonStarted(id, version);
    } else {
      info(`Checking state of add-on ${id}, expecting it to be missing`);

      if (isUpgrade) {
        // Add-on should not be installed
        Assert.equal(addon, null);
      }

      BootstrapMonitor.checkAddonNotStarted(id);

      if (addon)
        BootstrapMonitor.checkAddonInstalled(id);
      else
        BootstrapMonitor.checkAddonNotInstalled(id);
    }
  }
}

/**
 * Returns all system add-on updates directories.
 */
async function getSystemAddonDirectories() {
  const updatesDir = FileUtils.getDir("ProfD", ["features"], false);
  let subdirs = [];

  if (await OS.File.exists(updatesDir.path)) {
    let iterator = new OS.File.DirectoryIterator(updatesDir.path);
    await iterator.forEach(entry => {
      if (entry.isDir) {
        subdirs.push(entry);
      }
    });
    iterator.close();
  }

  return subdirs;
}

/**
 * Sets up initial system add-on update conditions.
 *
 * @param {Object<function, Array<Object>} setup - an object containing a setup function and an array of objects
 *  of the form {isUpgrade: false, version: null}
 *
 * @param {nsIFile} distroDir - the system add-on distribution directory (the "features" dir in the app directory)
 */
async function setupSystemAddonConditions(setup, distroDir) {
  info("Clearing existing database.");
  Services.prefs.clearUserPref(PREF_SYSTEM_ADDON_SET);
  distroDir.leafName = "empty";

  let updateList = [];
  awaitPromise(overrideBuiltIns({ "system": updateList }));
  startupManager(false);
  await promiseShutdownManager();

  info("Setting up conditions.");
  await setup.setup();

  if (distroDir) {
    if (distroDir.path.endsWith("hidden")) {
      updateList = ["system1@tests.mozilla.org", "system2@tests.mozilla.org"];
    } else if (distroDir.path.endsWith("prefilled")) {
      updateList = ["system2@tests.mozilla.org", "system3@tests.mozilla.org"];
    }
  }
  awaitPromise(overrideBuiltIns({ "system": updateList }));
  startupManager(false);

  // Make sure the initial state is correct
  info("Checking initial state.");
  await checkInstalledSystemAddons(setup.initialState, distroDir);
}

/**
 * Verify state of system add-ons after installation.
 *
 * @param {Array<Object>} initialState - an array of objects of the form {isUpgrade: false, version: null}
 * @param {Array<Object>} finalState - an array of objects of the form {isUpgrade: false, version: null}
 * @param {Boolean} alreadyUpgraded - whether a restartless upgrade has already been performed.
 * @param {nsIFile} distroDir - the system add-on distribution directory (the "features" dir in the app directory)
 */
async function verifySystemAddonState(initialState, finalState = undefined, alreadyUpgraded = false, distroDir) {
  let expectedDirs = 0;

  // If the initial state was using the profile set then that directory will
  // still exist.

  if (initialState.some(a => a.isUpgrade)) {
    expectedDirs++;
  }

  if (finalState == undefined) {
    finalState = initialState;
  } else if (finalState.some(a => a.isUpgrade)) {
    // If the new state is using the profile then that directory will exist.
    expectedDirs++;
  }

  // Since upgrades are restartless now, the previous update dir hasn't been removed.
  if (alreadyUpgraded) {
    expectedDirs++;
  }

  info("Checking final state.");

  let dirs = await getSystemAddonDirectories();
  Assert.equal(dirs.length, expectedDirs);

  await checkInstalledSystemAddons(...finalState, distroDir);

  // Check that the new state is active after a restart
  await promiseShutdownManager();

  let updateList = [];

  if (distroDir) {
    if (distroDir.path.endsWith("hidden")) {
      updateList = ["system1@tests.mozilla.org", "system2@tests.mozilla.org"];
    } else if (distroDir.path.endsWith("prefilled")) {
      updateList = ["system2@tests.mozilla.org", "system3@tests.mozilla.org"];
    }
  }
  awaitPromise(overrideBuiltIns({ "system": updateList }));
  startupManager();
  await checkInstalledSystemAddons(finalState, distroDir);
}

/**
 * Run system add-on tests and compare the results against a set of expected conditions.
 *
 * @param {String} setupName - name of the current setup conditions.
 * @param {Object<function, Array<Object>} setup -  Defines the set of initial conditions to run each test against. Each should
 *                                                  define the following properties:
 *    setup:        A task to setup the profile into the initial state.
 *    initialState: The initial expected system add-on state after setup has run.
 * @param {Array<Object>} test -  The test to run. Each test must define an updateList or test. The following
 *                                properties are used:
 *    updateList: The set of add-ons the server should respond with.
 *    test:       A function to run to perform the update check (replaces
 *                updateList)
 *    fails:      An optional property, if true the update check is expected to
 *                fail.
 *    finalState: An optional property, the expected final state of system add-ons,
 *                if missing the test condition's initialState is used.
 * @param {nsIFile} distroDir - the system add-on distribution directory (the "features" dir in the app directory)
 * @param {String} root - HTTP URL to the test server
 * @param {HttpServer} testserver - existing HTTP test server to use
 */

async function execSystemAddonTest(setupName, setup, test, distroDir, root, testserver) {
  await setupSystemAddonConditions(setup, distroDir);

  try {
    if ("test" in test) {
      await test.test();
    } else {
      await installSystemAddons(await buildSystemAddonUpdates(test.updateList, root), testserver);
    }

    if (test.fails) {
      do_throw("Expected this test to fail");
    }
  } catch (e) {
    if (!test.fails) {
      do_throw(e);
    }
  }

  // some tests have a different expected combination of default
  // and updated add-ons.
  if (test.finalState && setupName in test.finalState) {
    await verifySystemAddonState(setup.initialState, test.finalState[setupName], false, distroDir);
  } else {
    await verifySystemAddonState(setup.initialState, undefined, false, distroDir);
  }

  await promiseShutdownManager();
}
