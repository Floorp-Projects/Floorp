/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* eslint no-unused-vars: ["error", {vars: "local", args: "none"}] */

if (!_TEST_NAME.includes("toolkit/mozapps/extensions/test/xpcshell/")) {
  Assert.ok(
    false,
    "head_addons.js may not be loaded by tests outside of " +
      "the add-on manager component."
  );
}

const PREF_EM_CHECK_UPDATE_SECURITY = "extensions.checkUpdateSecurity";
const PREF_EM_STRICT_COMPATIBILITY = "extensions.strictCompatibility";
const PREF_GETADDONS_BYIDS = "extensions.getAddons.get.url";
const PREF_XPI_SIGNATURES_REQUIRED = "xpinstall.signatures.required";

// Maximum error in file modification times. Some file systems don't store
// modification times exactly. As long as we are closer than this then it
// still passes.
const MAX_TIME_DIFFERENCE = 3000;

// Time to reset file modified time relative to Date.now() so we can test that
// times are modified (10 hours old).
const MAKE_FILE_OLD_DIFFERENCE = 10 * 3600 * 1000;

const { AddonManager, AddonManagerPrivate } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs"
);
var { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
var { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
);
var { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);
var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
var { AddonRepository } = ChromeUtils.importESModule(
  "resource://gre/modules/addons/AddonRepository.sys.mjs"
);

var { AddonTestUtils, MockAsyncShutdown } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  Blocklist: "resource://gre/modules/Blocklist.sys.mjs",
  Extension: "resource://gre/modules/Extension.sys.mjs",
  ExtensionTestCommon: "resource://testing-common/ExtensionTestCommon.sys.mjs",
  ExtensionTestUtils:
    "resource://testing-common/ExtensionXPCShellUtils.sys.mjs",
  HttpServer: "resource://testing-common/httpd.sys.mjs",
  MockRegistrar: "resource://testing-common/MockRegistrar.sys.mjs",
  MockRegistry: "resource://testing-common/MockRegistry.sys.mjs",
  PromiseTestUtils: "resource://testing-common/PromiseTestUtils.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "aomStartup",
  "@mozilla.org/addons/addon-manager-startup;1",
  "amIAddonManagerStartup"
);

const {
  createAppInfo,
  createHttpServer,
  createTempWebExtensionFile,
  getFileForAddon,
  manuallyInstall,
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
  writeFilesToZip,
} = AddonTestUtils;

// WebExtension wrapper for ease of testing
ExtensionTestUtils.init(this);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

ChromeUtils.defineLazyGetter(
  this,
  "BOOTSTRAP_REASONS",
  () => AddonManagerPrivate.BOOTSTRAP_REASONS
);

function getReasonName(reason) {
  for (let key of Object.keys(BOOTSTRAP_REASONS)) {
    if (BOOTSTRAP_REASONS[key] == reason) {
      return key;
    }
  }
  throw new Error("This shouldn't happen.");
}

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
    return AddonTestUtils.addonIntegrationService.QueryInterface(
      Ci.nsITimerCallback
    );
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
    AddonTestUtils.useRealCertChecks = val;
  },
});

Object.defineProperty(this, "TEST_UNPACKED", {
  get() {
    return AddonTestUtils.testUnpacked;
  },
  set(val) {
    AddonTestUtils.testUnpacked = val;
  },
});

const promiseAddonByID = AddonManager.getAddonByID;
const promiseAddonsByIDs = AddonManager.getAddonsByIDs;
const promiseAddonsByTypes = AddonManager.getAddonsByTypes;

var gPort = null;

var BootstrapMonitor = {
  started: new Map(),
  stopped: new Map(),
  installed: new Map(),
  uninstalled: new Map(),

  init() {
    this.onEvent = this.onEvent.bind(this);

    AddonTestUtils.on("addon-manager-shutdown", this.onEvent);
    AddonTestUtils.on("bootstrap-method", this.onEvent);
  },

  shutdownCheck() {
    equal(
      this.started.size,
      0,
      "Should have no add-ons that were started but not shutdown"
    );
  },

  onEvent(msg, data) {
    switch (msg) {
      case "addon-manager-shutdown":
        this.shutdownCheck();
        break;
      case "bootstrap-method":
        this.onBootstrapMethod(data.method, data.params, data.reason);
        break;
    }
  },

  onBootstrapMethod(method, params, reason) {
    let { id } = params;

    info(
      `Bootstrap method ${method} ${reason} for ${params.id} version ${params.version}`
    );

    if (method !== "install") {
      this.checkInstalled(id);
    }

    switch (method) {
      case "install":
        this.checkNotInstalled(id);
        this.installed.set(id, { reason, params });
        this.uninstalled.delete(id);
        break;
      case "startup":
        this.checkNotStarted(id);
        this.started.set(id, { reason, params });
        this.stopped.delete(id);
        break;
      case "shutdown":
        this.checkMatches("shutdown", "startup", params, this.started.get(id));
        this.checkStarted(id);
        this.stopped.set(id, { reason, params });
        this.started.delete(id);
        break;
      case "uninstall":
        this.checkMatches(
          "uninstall",
          "install",
          params,
          this.installed.get(id)
        );
        this.uninstalled.set(id, { reason, params });
        this.installed.delete(id);
        break;
      case "update":
        this.checkMatches("update", "install", params, this.installed.get(id));
        this.installed.set(id, { reason, params, method });
        break;
    }
  },

  clear(id) {
    this.installed.delete(id);
    this.started.delete(id);
    this.stopped.delete(id);
    this.uninstalled.delete(id);
  },

  checkMatches(method, lastMethod, params, { params: lastParams } = {}) {
    ok(
      lastParams,
      `Expecting matching ${lastMethod} call for add-on ${params.id} ${method} call`
    );

    if (method == "update") {
      equal(
        params.oldVersion,
        lastParams.version,
        "params.oldVersion should match last call"
      );
    } else {
      equal(
        params.version,
        lastParams.version,
        "params.version should match last call"
      );
    }

    if (method !== "update" && method !== "uninstall") {
      equal(
        params.resourceURI.spec,
        lastParams.resourceURI.spec,
        `params.resourceURI should match last call`
      );

      ok(
        params.resourceURI.equals(lastParams.resourceURI),
        `params.resourceURI should match: "${params.resourceURI.spec}" == "${lastParams.resourceURI.spec}"`
      );
    }
  },

  checkStarted(id, version = undefined) {
    let started = this.started.get(id);
    ok(started, `Should have seen startup method call for ${id}`);

    if (version !== undefined) {
      equal(started.params.version, version, "Expected version number");
    }
    return started;
  },

  checkNotStarted(id) {
    ok(
      !this.started.has(id),
      `Should not have seen startup method call for ${id}`
    );
  },

  checkInstalled(id, version = undefined) {
    const installed = this.installed.get(id);
    ok(installed, `Should have seen install call for ${id}`);

    if (version !== undefined) {
      equal(installed.params.version, version, "Expected version number");
    }

    return installed;
  },

  checkUpdated(id, version = undefined) {
    const installed = this.installed.get(id);
    equal(installed.method, "update", `Should have seen update call for ${id}`);

    if (version !== undefined) {
      equal(installed.params.version, version, "Expected version number");
    }

    return installed;
  },

  checkNotInstalled(id) {
    ok(
      !this.installed.has(id),
      `Should not have seen install method call for ${id}`
    );
  },
};

function isNightlyChannel() {
  var channel = Services.prefs.getCharPref("app.update.channel", "default");

  return (
    channel != "aurora" &&
    channel != "beta" &&
    channel != "release" &&
    channel != "esr"
  );
}

async function restartWithLocales(locales) {
  Services.locale.requestedLocales = locales;
  await promiseRestartManager();
}

function delay(msec) {
  return new Promise(resolve => {
    setTimeout(resolve, msec);
  });
}

/**
 * Returns a map of Addon objects for installed add-ons with the given
 * IDs. The returned map contains a key for the ID of each add-on that
 * is found. IDs for add-ons which do not exist are not present in the
 * map.
 *
 * @param {sequence<string>} ids
 *        The list of add-on IDs to get.
 * @returns {Promise<string, Addon>}
 *        Map of add-ons that were found.
 */
async function getAddons(ids) {
  let addons = new Map();
  for (let addon of await AddonManager.getAddonsByIDs(ids)) {
    if (addon) {
      addons.set(addon.id, addon);
    }
  }
  return addons;
}

/**
 * Checks that the given add-on has the given expected properties.
 *
 * @param {string} id
 *        The id of the add-on.
 * @param {Addon?} addon
 *        The add-on object, or null if the add-on does not exist.
 * @param {object?} expected
 *        An object containing the expected values for properties of the
 *        add-on, or null if the add-on is expected not to exist.
 */
function checkAddon(id, addon, expected) {
  info(`Checking state of addon ${id}`);

  if (expected === null) {
    ok(!addon, `Addon ${id} should not exist`);
  } else {
    ok(addon, `Addon ${id} should exist`);
    for (let [key, value] of Object.entries(expected)) {
      if (value instanceof Ci.nsIURI) {
        equal(
          addon[key] && addon[key].spec,
          value.spec,
          `Expected value of addon.${key}`
        );
      } else {
        deepEqual(addon[key], value, `Expected value of addon.${key}`);
      }
    }
  }
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
    Assert.ok(false, "Cannot find Add-ons entry in crash annotations");
    return;
  }

  let addons = gAppInfo.annotations["Add-ons"].split(",");
  Assert.ok(
    addons.includes(
      `${encodeURIComponent(aId)}:${encodeURIComponent(aVersion)}`
    )
  );
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
  Assert.ok(
    !addons.includes(
      `${encodeURIComponent(aId)}:${encodeURIComponent(aVersion)}`
    )
  );
}

function do_get_file_hash(aFile, aAlgorithm) {
  if (!aAlgorithm) {
    aAlgorithm = "sha1";
  }

  let crypto = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );
  crypto.initWithString(aAlgorithm);
  let fis = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
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
  if (TEST_UNPACKED) {
    return aId;
  }
  return aId + ".xpi";
}

/**
 * Returns the file containing the add-on. For packed add-ons, this is
 * an XPI file. For unpacked add-ons, it is the add-on's root directory.
 *
 * @param {Addon} addon
 * @returns {nsIFile}
 */
function getAddonFile(addon) {
  let uri = addon.getResourceURI("");
  if (uri instanceof Ci.nsIJARURI) {
    uri = uri.JARFile;
  }
  return uri.QueryInterface(Ci.nsIFileURL).file;
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
  for (let i = 0; i < aActualAddons.length; i++) {
    do_check_addon(aActualAddons[i], aExpectedAddons[i], aProperties);
  }
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

  aProperties.forEach(function (aProperty) {
    let actualValue = aActualAddon[aProperty];
    let expectedValue = aExpectedAddon[aProperty];

    // Check that all undefined expected properties are null on actual add-on
    if (!(aProperty in aExpectedAddon)) {
      if (actualValue !== undefined && actualValue !== null) {
        do_throw(
          "Unexpected defined/non-null property for add-on " +
            aExpectedAddon.id +
            " (addon[" +
            aProperty +
            "] = " +
            actualValue.toSource() +
            ")"
        );
      }

      return;
    } else if (expectedValue && !actualValue) {
      do_throw(
        "Missing property for add-on " +
          aExpectedAddon.id +
          ": expected addon[" +
          aProperty +
          "] = " +
          expectedValue
      );
      return;
    }

    switch (aProperty) {
      case "creator":
        do_check_author(actualValue, expectedValue);
        break;

      case "developers":
        Assert.equal(actualValue.length, expectedValue.length);
        for (let i = 0; i < actualValue.length; i++) {
          do_check_author(actualValue[i], expectedValue[i]);
        }
        break;

      case "screenshots":
        Assert.equal(actualValue.length, expectedValue.length);
        for (let i = 0; i < actualValue.length; i++) {
          do_check_screenshot(actualValue[i], expectedValue[i]);
        }
        break;

      case "sourceURI":
        Assert.equal(actualValue.spec, expectedValue);
        break;

      case "updateDate":
        Assert.equal(actualValue.getTime(), expectedValue.getTime());
        break;

      case "compatibilityOverrides":
        Assert.equal(actualValue.length, expectedValue.length);
        for (let i = 0; i < actualValue.length; i++) {
          do_check_compatibilityoverride(actualValue[i], expectedValue[i]);
        }
        break;

      case "icons":
        do_check_icons(actualValue, expectedValue);
        break;

      default:
        if (actualValue !== expectedValue) {
          do_throw(
            "Failed for " +
              aProperty +
              " for add-on " +
              aExpectedAddon.id +
              " (" +
              actualValue +
              " === " +
              expectedValue +
              ")"
          );
        }
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
    Assert.equal(aActual[size], aExpected[size]);
  }
}

function isThemeInAddonsList(aDir, aId) {
  return AddonTestUtils.addonsList.hasTheme(aDir, aId);
}

function isExtensionInBootstrappedList(aDir, aId) {
  return AddonTestUtils.addonsList.hasExtension(aDir, aId);
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
function promiseWriteWebManifestForExtension(aData, aDir, aId) {
  let files = {
    "manifest.json": JSON.stringify(aData),
  };
  if (!aId) {
    aId =
      aData?.browser_specific_settings?.gecko?.id ||
      aData?.applications?.gecko?.id;
  }
  return AddonTestUtils.promiseWriteFilesToExtension(aDir.path, aId, files);
}

function hasFlag(aBits, aFlag) {
  return (aBits & aFlag) != 0;
}

class EventChecker {
  constructor(options) {
    this.expectedEvents = options.addonEvents || {};
    this.expectedInstalls = options.installEvents || null;
    this.ignorePlugins = options.ignorePlugins || false;

    this.finished = new Promise(resolve => {
      this.resolveFinished = resolve;
    });

    AddonManager.addAddonListener(this);
    if (this.expectedInstalls) {
      AddonManager.addInstallListener(this);
    }
  }

  cleanup() {
    AddonManager.removeAddonListener(this);
    if (this.expectedInstalls) {
      AddonManager.removeInstallListener(this);
    }
  }

  checkValue(prop, value, flagName) {
    if (Array.isArray(flagName)) {
      let names = flagName.map(name => `AddonManager.${name}`);

      Assert.ok(
        flagName.map(name => AddonManager[name]).includes(value),
        `${prop} value \`${value}\` should be one of [${names.join(", ")}`
      );
    } else {
      Assert.equal(
        value,
        AddonManager[flagName],
        `${prop} should have value AddonManager.${flagName}`
      );
    }
  }

  checkFlag(prop, value, flagName) {
    Assert.equal(
      value & AddonManager[flagName],
      AddonManager[flagName],
      `${prop} should have flag AddonManager.${flagName}`
    );
  }

  checkNoFlag(prop, value, flagName) {
    Assert.ok(
      !(value & AddonManager[flagName]),
      `${prop} should not have flag AddonManager.${flagName}`
    );
  }

  checkComplete() {
    if (this.expectedInstalls && this.expectedInstalls.length) {
      return;
    }

    if (Object.values(this.expectedEvents).some(events => events.length)) {
      return;
    }

    info("Test complete");
    this.cleanup();
    this.resolveFinished();
  }

  ensureComplete() {
    this.cleanup();

    for (let [id, events] of Object.entries(this.expectedEvents)) {
      Assert.equal(
        events.length,
        0,
        `Should have no remaining events for ${id}`
      );
    }
    if (this.expectedInstalls) {
      Assert.deepEqual(
        this.expectedInstalls,
        [],
        "Should have no remaining install events"
      );
    }
  }

  // Add-on listener events
  getExpectedEvent(aId) {
    if (!(aId in this.expectedEvents)) {
      return null;
    }

    let events = this.expectedEvents[aId];
    Assert.ok(!!events.length, `Should be expecting events for ${aId}`);

    return events.shift();
  }

  checkAddonEvent(event, addon, details = {}) {
    info(`Got event "${event}" for add-on ${addon.id}`);

    if ("requiresRestart" in details) {
      Assert.equal(
        details.requiresRestart,
        false,
        "requiresRestart should always be false"
      );
    }

    let expected = this.getExpectedEvent(addon.id);
    if (!expected) {
      return undefined;
    }

    Assert.equal(
      expected.event,
      event,
      `Expecting event "${expected.event}" got "${event}"`
    );

    for (let prop of ["properties"]) {
      if (prop in expected) {
        Assert.deepEqual(
          expected[prop],
          details[prop],
          `Expected value for ${prop}`
        );
      }
    }

    this.checkComplete();

    if ("returnValue" in expected) {
      return expected.returnValue;
    }
    return undefined;
  }

  onPropertyChanged(addon, properties) {
    return this.checkAddonEvent("onPropertyChanged", addon, { properties });
  }

  onEnabling(addon, requiresRestart) {
    let result = this.checkAddonEvent("onEnabling", addon, { requiresRestart });

    this.checkNoFlag("addon.permissions", addon.permissions, "PERM_CAN_ENABLE");

    return result;
  }

  onEnabled(addon) {
    let result = this.checkAddonEvent("onEnabled", addon);

    this.checkNoFlag("addon.permissions", addon.permissions, "PERM_CAN_ENABLE");

    return result;
  }

  onDisabling(addon, requiresRestart) {
    let result = this.checkAddonEvent("onDisabling", addon, {
      requiresRestart,
    });

    this.checkNoFlag(
      "addon.permissions",
      addon.permissions,
      "PERM_CAN_DISABLE"
    );
    return result;
  }

  onDisabled(addon) {
    let result = this.checkAddonEvent("onDisabled", addon);

    this.checkNoFlag(
      "addon.permissions",
      addon.permissions,
      "PERM_CAN_DISABLE"
    );

    return result;
  }

  onInstalling(addon, requiresRestart) {
    return this.checkAddonEvent("onInstalling", addon, { requiresRestart });
  }

  onInstalled(addon) {
    return this.checkAddonEvent("onInstalled", addon);
  }

  onUninstalling(addon, requiresRestart) {
    return this.checkAddonEvent("onUninstalling", addon);
  }

  onUninstalled(addon) {
    return this.checkAddonEvent("onUninstalled", addon);
  }

  onOperationCancelled(addon) {
    return this.checkAddonEvent("onOperationCancelled", addon);
  }

  // Install listener events.
  checkInstall(event, install, details = {}) {
    // Lazy initialization of the plugin host means we can get spurious
    // install events for plugins. If we're not looking for plugin
    // installs, ignore them completely. If we *are* looking for plugin
    // installs, the onus is on the individual test to ensure it waits
    // for the plugin host to have done its initial work.
    if (this.ignorePlugins && install.type == "plugin") {
      info(`Ignoring install event for plugin ${install.id}`);
      return undefined;
    }
    info(`Got install event "${event}"`);

    let expected = this.expectedInstalls.shift();
    Assert.ok(expected, "Should be expecting install event");

    Assert.equal(
      expected.event,
      event,
      "Should be expecting onExternalInstall event"
    );

    if ("state" in details) {
      this.checkValue("install.state", install.state, details.state);
    }

    this.checkComplete();

    if ("callback" in expected) {
      expected.callback(install);
    }

    if ("returnValue" in expected) {
      return expected.returnValue;
    }
    return undefined;
  }

  onNewInstall(install) {
    let result = this.checkInstall("onNewInstall", install, {
      state: ["STATE_DOWNLOADED", "STATE_DOWNLOAD_FAILED", "STATE_AVAILABLE"],
    });

    if (install.state != AddonManager.STATE_DOWNLOAD_FAILED) {
      Assert.equal(install.error, 0, "Should have no error");
    } else {
      Assert.notEqual(install.error, 0, "Should have error");
    }

    return result;
  }

  onDownloadStarted(install) {
    return this.checkInstall("onDownloadStarted", install, {
      state: "STATE_DOWNLOADING",
      error: 0,
    });
  }

  onDownloadEnded(install) {
    return this.checkInstall("onDownloadEnded", install, {
      state: "STATE_DOWNLOADED",
      error: 0,
    });
  }

  onDownloadFailed(install) {
    return this.checkInstall("onDownloadFailed", install, {
      state: "STATE_FAILED",
    });
  }

  onDownloadCancelled(install) {
    return this.checkInstall("onDownloadCancelled", install, {
      state: "STATE_CANCELLED",
      error: 0,
    });
  }

  onInstallStarted(install) {
    return this.checkInstall("onInstallStarted", install, {
      state: "STATE_INSTALLING",
      error: 0,
    });
  }

  onInstallEnded(install, newAddon) {
    return this.checkInstall("onInstallEnded", install, {
      state: "STATE_INSTALLED",
      error: 0,
    });
  }

  onInstallFailed(install) {
    return this.checkInstall("onInstallFailed", install, {
      state: "STATE_FAILED",
    });
  }

  onInstallCancelled(install) {
    // If the install was cancelled by a listener returning false from
    // onInstallStarted, then the state will revert to STATE_DOWNLOADED.
    return this.checkInstall("onInstallCancelled", install, {
      state: ["STATE_CANCELED", "STATE_DOWNLOADED"],
      error: 0,
    });
  }

  onExternalInstall(addon, existingAddon, requiresRestart) {
    if (this.ignorePlugins && addon.type == "plugin") {
      info(`Ignoring install event for plugin ${addon.id}`);
      return undefined;
    }
    let expected = this.expectedInstalls.shift();
    Assert.ok(expected, "Should be expecting install event");

    Assert.equal(
      expected.event,
      "onExternalInstall",
      "Should be expecting onExternalInstall event"
    );
    Assert.ok(!requiresRestart, "Should never require restart");

    this.checkComplete();
    if ("returnValue" in expected) {
      return expected.returnValue;
    }
    return undefined;
  }
}

/**
 * Run the giving callback function, and expect the given set of add-on
 * and install listener events to be emitted, and returns a promise
 * which resolves when they have all been observed.
 *
 * If `callback` returns a promise, all events are expected to be
 * observed by the time the promise resolves. If not, simply waits for
 * all events to be observed before resolving the returned promise.
 *
 * @param {object} details
 * @param {function} callback
 * @returns {Promise}
 */
/* exported expectEvents */
async function expectEvents(details, callback) {
  let checker = new EventChecker(details);

  try {
    let result = callback();

    if (
      result &&
      typeof result === "object" &&
      typeof result.then === "function"
    ) {
      result = await result;
      checker.ensureComplete();
    } else {
      await checker.finished;
    }

    return result;
  } catch (e) {
    do_throw(e);
    return undefined;
  }
}

const EXTENSIONS_DB = "extensions.json";
var gExtensionsJSON = gProfD.clone();
gExtensionsJSON.append(EXTENSIONS_DB);

async function promiseInstallWebExtension(aData) {
  let addonFile = createTempWebExtensionFile(aData);

  let { addon } = await promiseInstallFile(addonFile);
  return addon;
}

// By default use strict compatibility
Services.prefs.setBoolPref("extensions.strictCompatibility", true);

// Ensure signature checks are enabled by default
Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, true);

Services.prefs.setBoolPref("extensions.experiments.enabled", true);

// Copies blocklistFile (an nsIFile) to gProfD/blocklist.xml.
function copyBlocklistToProfile(blocklistFile) {
  var dest = gProfD.clone();
  dest.append("blocklist.xml");
  if (dest.exists()) {
    dest.remove(false);
  }
  blocklistFile.copyTo(gProfD, "blocklist.xml");
  dest.lastModifiedTime = Date.now();
}

async function mockGfxBlocklistItemsFromDisk(path) {
  let response = await fetch(Services.io.newFileURI(do_get_file(path)).spec);
  let json = await response.json();
  return mockGfxBlocklistItems(json);
}

async function mockGfxBlocklistItems(items) {
  const { generateUUID } = Services.uuid;
  const { BlocklistPrivate } = ChromeUtils.importESModule(
    "resource://gre/modules/Blocklist.sys.mjs"
  );
  const client = RemoteSettings("gfx", {
    bucketName: "blocklists",
  });
  const records = items.map(item => {
    if (item.id && item.last_modified) {
      return item;
    }
    return {
      id: generateUUID().toString().replace(/[{}]/g, ""),
      last_modified: Date.now(),
      ...item,
    };
  });
  const collectionTimestamp = Math.max(...records.map(r => r.last_modified));
  await client.db.importChanges({}, collectionTimestamp, records, {
    clear: true,
  });
  let rv = await BlocklistPrivate.GfxBlocklistRS.checkForEntries();
  return rv;
}

/**
 * Change the schema version of the JSON extensions database
 */
async function changeXPIDBVersion(aNewVersion) {
  let json = await IOUtils.readJSON(gExtensionsJSON.path);
  json.schemaVersion = aNewVersion;
  await IOUtils.writeJSON(gExtensionsJSON.path, json);
}

async function setInitialState(addon, initialState) {
  if (initialState.userDisabled) {
    await addon.disable();
  } else if (initialState.userDisabled === false) {
    await addon.enable();
  }
}

async function setupBuiltinExtension(extensionData, location = "ext-test") {
  let xpi = await AddonTestUtils.createTempWebExtensionFile(extensionData);

  // The built-in location requires a resource: URL that maps to a
  // jar: or file: URL.  This would typically be something bundled
  // into omni.ja but for testing we just use a temp file.
  let base = Services.io.newURI(`jar:file:${xpi.path}!/`);
  let resProto = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);
  resProto.setSubstitution(location, base);
}

async function installBuiltinExtension(extensionData, waitForStartup = true) {
  await setupBuiltinExtension(extensionData);

  let id =
    extensionData.manifest?.browser_specific_settings?.gecko?.id ||
    extensionData.manifest?.applications?.gecko?.id;
  let wrapper = ExtensionTestUtils.expectExtension(id);
  await AddonManager.installBuiltinAddon("resource://ext-test/");
  if (waitForStartup) {
    await wrapper.awaitStartup();
  }
  return wrapper;
}
