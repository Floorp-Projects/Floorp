/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* eslint "mozilla/no-aArgs": 1 */
/* eslint "no-unused-vars": [2, {"args": "none", "varsIgnorePattern": "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$"}] */
/* eslint "semi": [2, "always"] */
/* eslint "valid-jsdoc": [2, {requireReturn: false}] */

var EXPORTED_SYMBOLS = ["AddonTestUtils", "MockAsyncShutdown"];

const CERTDB_CONTRACTID = "@mozilla.org/security/x509certdb;1";

const { AddonManager, AddonManagerPrivate } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
const { AsyncShutdown } = ChromeUtils.import(
  "resource://gre/modules/AsyncShutdown.jsm"
);
const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);
const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AMTelemetry: "resource://gre/modules/AddonManager.jsm",
  ExtensionTestCommon: "resource://testing-common/ExtensionTestCommon.jsm",
  getAppInfo: "resource://testing-common/AppInfo.jsm",
  Management: "resource://gre/modules/Extension.jsm",
  ExtensionAddonObserver: "resource://gre/modules/Extension.jsm",
  FileTestUtils: "resource://testing-common/FileTestUtils.jsm",
  MockRegistrar: "resource://testing-common/MockRegistrar.jsm",
  updateAppInfo: "resource://testing-common/AppInfo.jsm",
  XPCShellContentUtils: "resource://testing-common/XPCShellContentUtils.jsm",
});

XPCOMUtils.defineLazyServiceGetters(lazy, {
  aomStartup: [
    "@mozilla.org/addons/addon-manager-startup;1",
    "amIAddonManagerStartup",
  ],
});

const PREF_DISABLE_SECURITY =
  "security.turn_off_all_security_so_that_" +
  "viruses_can_take_over_this_computer";

const ArrayBufferInputStream = Components.Constructor(
  "@mozilla.org/io/arraybuffer-input-stream;1",
  "nsIArrayBufferInputStream",
  "setData"
);

const nsFile = Components.Constructor(
  "@mozilla.org/file/local;1",
  "nsIFile",
  "initWithPath"
);

const ZipReader = Components.Constructor(
  "@mozilla.org/libjar/zip-reader;1",
  "nsIZipReader",
  "open"
);

const ZipWriter = Components.Constructor(
  "@mozilla.org/zipwriter;1",
  "nsIZipWriter",
  "open"
);

function isRegExp(val) {
  return val && typeof val === "object" && typeof val.test === "function";
}

class MockBarrier {
  constructor(name) {
    this.name = name;
    this.blockers = [];
  }

  addBlocker(name, blocker, options) {
    this.blockers.push({ name, blocker, options });
  }

  async trigger() {
    await Promise.all(
      this.blockers.map(async ({ blocker, name }) => {
        try {
          if (typeof blocker == "function") {
            await blocker();
          } else {
            await blocker;
          }
        } catch (e) {
          Cu.reportError(e);
          dump(
            `Shutdown blocker '${name}' for ${this.name} threw error: ${e} :: ${e.stack}\n`
          );
        }
      })
    );

    this.blockers = [];
  }
}

// Mock out AddonManager's reference to the AsyncShutdown module so we can shut
// down AddonManager from the test
var MockAsyncShutdown = {
  profileBeforeChange: new MockBarrier("profileBeforeChange"),
  profileChangeTeardown: new MockBarrier("profileChangeTeardown"),
  quitApplicationGranted: new MockBarrier("quitApplicationGranted"),
  // We can use the real Barrier
  Barrier: AsyncShutdown.Barrier,
};

AddonManagerPrivate.overrideAsyncShutdown(MockAsyncShutdown);

class AddonsList {
  constructor(file) {
    this.extensions = [];
    this.themes = [];
    this.xpis = [];

    if (!file.exists()) {
      return;
    }

    let data = lazy.aomStartup.readStartupData();

    for (let loc of Object.values(data)) {
      let dir = loc.path && new nsFile(loc.path);

      for (let addon of Object.values(loc.addons)) {
        let file;
        if (dir) {
          file = dir.clone();
          try {
            file.appendRelativePath(addon.path);
          } catch (e) {
            file = new nsFile(addon.path);
          }
        } else if (addon.path) {
          file = new nsFile(addon.path);
        }

        if (!file) {
          continue;
        }

        this.xpis.push(file);

        if (addon.enabled) {
          addon.type = addon.type || "extension";

          if (addon.type == "theme") {
            this.themes.push(file);
          } else {
            this.extensions.push(file);
          }
        }
      }
    }
  }

  hasItem(type, dir, id) {
    var path = dir.clone();
    path.append(id);

    var xpiPath = dir.clone();
    xpiPath.append(`${id}.xpi`);

    return this[type].some(file => {
      if (!file.exists()) {
        throw new Error(
          `Non-existent path found in addonStartup.json: ${file.path}`
        );
      }

      if (file.isDirectory()) {
        return file.equals(path);
      }
      if (file.isFile()) {
        return file.equals(xpiPath);
      }
      return false;
    });
  }

  hasTheme(dir, id) {
    return this.hasItem("themes", dir, id);
  }

  hasExtension(dir, id) {
    return this.hasItem("extensions", dir, id);
  }
}

var AddonTestUtils = {
  addonIntegrationService: null,
  addonsList: null,
  appInfo: null,
  addonStartup: null,
  collectedTelemetryEvents: [],
  testScope: null,
  testUnpacked: false,
  useRealCertChecks: false,
  usePrivilegedSignatures: true,
  certSignatureDate: null,
  overrideEntry: null,

  maybeInit(testScope) {
    if (this.testScope != testScope) {
      this.init(testScope);
    }
  },

  init(testScope, enableLogging = true) {
    if (this.testScope === testScope) {
      return;
    }
    this.testScope = testScope;

    // Get the profile directory for tests to use.
    this.profileDir = testScope.do_get_profile();

    this.profileExtensions = this.profileDir.clone();
    this.profileExtensions.append("extensions");

    this.addonStartup = this.profileDir.clone();
    this.addonStartup.append("addonStartup.json.lz4");

    // Register a temporary directory for the tests.
    this.tempDir = this.profileDir.clone();
    this.tempDir.append("temp");
    this.tempDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    this.registerDirectory("TmpD", this.tempDir);

    // Create a replacement app directory for the tests.
    const appDirForAddons = this.profileDir.clone();
    appDirForAddons.append("appdir-addons");
    appDirForAddons.create(
      Ci.nsIFile.DIRECTORY_TYPE,
      FileUtils.PERMS_DIRECTORY
    );
    this.registerDirectory("XREAddonAppDir", appDirForAddons);

    // Enable more extensive EM logging.
    if (enableLogging) {
      Services.prefs.setBoolPref("extensions.logging.enabled", true);
    }

    // By default only load extensions from the profile install location
    Services.prefs.setIntPref(
      "extensions.enabledScopes",
      AddonManager.SCOPE_PROFILE
    );

    // By default don't disable add-ons from any scope
    Services.prefs.setIntPref("extensions.autoDisableScopes", 0);

    // And scan for changes at startup
    Services.prefs.setIntPref("extensions.startupScanScopes", 15);

    // By default, don't cache add-ons in AddonRepository.jsm
    Services.prefs.setBoolPref("extensions.getAddons.cache.enabled", false);

    // Point update checks to the local machine for fast failures
    Services.prefs.setCharPref(
      "extensions.update.url",
      "http://127.0.0.1/updateURL"
    );
    Services.prefs.setCharPref(
      "extensions.update.background.url",
      "http://127.0.0.1/updateBackgroundURL"
    );
    Services.prefs.setCharPref(
      "services.settings.server",
      "http://localhost/dummy-kinto/v1"
    );

    // By default ignore bundled add-ons
    Services.prefs.setBoolPref("extensions.installDistroAddons", false);

    // Ensure signature checks are enabled by default
    Services.prefs.setBoolPref("xpinstall.signatures.required", true);

    // Make sure that a given path does not exist
    function pathShouldntExist(file) {
      if (file.exists()) {
        throw new Error(
          `Test cleanup: path ${file.path} exists when it should not`
        );
      }
    }

    testScope.registerCleanupFunction(() => {
      // Force a GC to ensure that anything holding a ref to temp file releases it.
      // XXX This shouldn't be needed here, since cleanupTempXPIs() does a GC if
      // something fails; see bug 1761255
      this.info(`Force a GC`);
      Cu.forceGC();

      this.cleanupTempXPIs();

      let ignoreEntries = new Set();
      {
        // FileTestUtils lazily creates a directory to hold the temporary files
        // it creates. If that directory exists, ignore it.
        let { value } = Object.getOwnPropertyDescriptor(
          lazy.FileTestUtils,
          "_globalTemporaryDirectory"
        );
        if (value) {
          ignoreEntries.add(value.leafName);
        }
      }

      // Check that the temporary directory is empty
      var entries = [];
      for (let { leafName } of this.iterDirectory(this.tempDir)) {
        if (!ignoreEntries.has(leafName)) {
          entries.push(leafName);
        }
      }
      if (entries.length) {
        throw new Error(
          `Found unexpected files in temporary directory: ${entries.join(", ")}`
        );
      }

      try {
        appDirForAddons.remove(true);
      } catch (ex) {
        testScope.info(`Got exception removing addon app dir: ${ex}`);
      }

      // ensure no leftover files in the system addon upgrade location
      let featuresDir = this.profileDir.clone();
      featuresDir.append("features");
      // upgrade directories will be in UUID folders under features/
      for (let dir of this.iterDirectory(featuresDir)) {
        dir.append("stage");
        pathShouldntExist(dir);
      }

      // ensure no leftover files in the user addon location
      let testDir = this.profileDir.clone();
      testDir.append("extensions");
      testDir.append("trash");
      pathShouldntExist(testDir);

      testDir.leafName = "staged";
      pathShouldntExist(testDir);

      return this.promiseShutdownManager();
    });
  },

  initMochitest(testScope) {
    if (this.testScope === testScope) {
      return;
    }
    this.testScope = testScope;

    this.profileDir = FileUtils.getDir("ProfD", []);

    this.profileExtensions = FileUtils.getDir("ProfD", ["extensions"]);

    this.tempDir = FileUtils.getDir("TmpD", []);
    this.tempDir.append("addons-mochitest");
    this.tempDir.createUnique(
      Ci.nsIFile.DIRECTORY_TYPE,
      FileUtils.PERMS_DIRECTORY
    );

    testScope.registerCleanupFunction(() => {
      // Defer testScope cleanup until the last cleanup function has run.
      testScope.registerCleanupFunction(() => {
        this.testScope = null;
      });
      this.cleanupTempXPIs();
      try {
        this.tempDir.remove(true);
      } catch (e) {
        Cu.reportError(e);
      }
    });
  },

  /**
   * Iterates over the entries in a given directory.
   *
   * Fails silently if the given directory does not exist.
   *
   * @param {nsIFile} dir
   *        Directory to iterate.
   */
  *iterDirectory(dir) {
    let dirEnum;
    try {
      dirEnum = dir.directoryEntries;
      let file;
      while ((file = dirEnum.nextFile)) {
        yield file;
      }
    } catch (e) {
      if (dir.exists()) {
        Cu.reportError(e);
      }
    } finally {
      if (dirEnum) {
        dirEnum.close();
      }
    }
  },

  /**
   * Creates a new HttpServer for testing, and begins listening on the
   * specified port. Automatically shuts down the server when the test
   * unit ends.
   *
   * @param {object} [options = {}]
   *        The options object.
   * @param {integer} [options.port = -1]
   *        The port to listen on. If omitted, listen on a random
   *        port. The latter is the preferred behavior.
   * @param {sequence<string>?} [options.hosts = null]
   *        A set of hosts to accept connections to. Support for this is
   *        implemented using a proxy filter.
   *
   * @returns {HttpServer}
   *        The HTTP server instance.
   */
  createHttpServer(...args) {
    lazy.XPCShellContentUtils.ensureInitialized(this.testScope);
    return lazy.XPCShellContentUtils.createHttpServer(...args);
  },

  registerJSON(...args) {
    return lazy.XPCShellContentUtils.registerJSON(...args);
  },

  info(msg) {
    // info() for mochitests, do_print for xpcshell.
    let print = this.testScope.info || this.testScope.do_print;
    print(msg);
  },

  cleanupTempXPIs() {
    let didGC = false;

    for (let file of this.tempXPIs.splice(0)) {
      if (file.exists()) {
        try {
          Services.obs.notifyObservers(file, "flush-cache-entry");
          file.remove(false);
        } catch (e) {
          if (didGC) {
            Cu.reportError(`Failed to remove ${file.path}: ${e}`);
          } else {
            // Bug 1606684 - Sometimes XPI files are still in use by a process
            // after the test has been finished. Force a GC once and try again.
            this.info(`Force a GC`);
            Cu.forceGC();
            didGC = true;

            try {
              file.remove(false);
            } catch (e) {
              Cu.reportError(`Failed to remove ${file.path} after GC: ${e}`);
            }
          }
        }
      }
    }
  },

  createAppInfo(ID, name, version, platformVersion = "1.0") {
    lazy.updateAppInfo({
      ID,
      name,
      version,
      platformVersion,
      crashReporter: true,
    });
    this.appInfo = lazy.getAppInfo();
  },

  getManifestURI(file) {
    if (file.isDirectory()) {
      file.leafName = "manifest.json";
      if (file.exists()) {
        return NetUtil.newURI(file);
      }

      throw new Error("No manifest file present");
    }

    let zip = ZipReader(file);
    try {
      let uri = NetUtil.newURI(file);

      if (zip.hasEntry("manifest.json")) {
        return NetUtil.newURI(`jar:${uri.spec}!/manifest.json`);
      }

      throw new Error("No manifest file present");
    } finally {
      zip.close();
    }
  },

  getIDFromExtension(file) {
    return this.getIDFromManifest(this.getManifestURI(file));
  },

  async getIDFromManifest(manifestURI) {
    let body = await fetch(manifestURI.spec);
    let manifest = await body.json();
    try {
      if (manifest.browser_specific_settings?.gecko?.id) {
        return manifest.browser_specific_settings.gecko.id;
      }
      return manifest.applications.gecko.id;
    } catch (e) {
      // IDs for WebExtensions are extracted from the certificate when
      // not present in the manifest, so just generate a random one.
      return Services.uuid.generateUUID().number;
    }
  },

  overrideCertDB() {
    let verifyCert = async (file, result, cert, callback) => {
      if (
        result == Cr.NS_ERROR_SIGNED_JAR_NOT_SIGNED &&
        !this.useRealCertChecks &&
        callback.wrappedJSObject
      ) {
        // Bypassing XPConnect allows us to create a fake x509 certificate from JS
        callback = callback.wrappedJSObject;

        try {
          let id;
          try {
            let manifestURI = this.getManifestURI(file);
            id = await this.getIDFromManifest(manifestURI);
          } catch (err) {
            if (file.leafName.endsWith(".xpi")) {
              id = file.leafName.slice(0, -4);
            }
          }

          let fakeCert = { commonName: id };
          if (this.usePrivilegedSignatures) {
            let privileged =
              typeof this.usePrivilegedSignatures == "function"
                ? this.usePrivilegedSignatures(id)
                : this.usePrivilegedSignatures;
            if (privileged === "system") {
              fakeCert.organizationalUnit = "Mozilla Components";
            } else if (privileged) {
              fakeCert.organizationalUnit = "Mozilla Extensions";
            }
          }
          if (this.certSignatureDate) {
            // addon.signedDate is derived from this, used by the blocklist.
            fakeCert.validity = {
              notBefore: this.certSignatureDate * 1000,
            };
          }

          return [callback, Cr.NS_OK, fakeCert];
        } catch (e) {
          // If there is any error then just pass along the original results
        } finally {
          // Make sure to close the open zip file or it will be locked.
          if (file.isFile()) {
            Services.obs.notifyObservers(
              file,
              "flush-cache-entry",
              "cert-override"
            );
          }
        }
      }

      return [callback, result, cert];
    };

    let FakeCertDB = {
      init() {
        for (let property of Object.keys(
          this._genuine.QueryInterface(Ci.nsIX509CertDB)
        )) {
          if (property in this) {
            continue;
          }

          if (typeof this._genuine[property] == "function") {
            this[property] = this._genuine[property].bind(this._genuine);
          }
        }
      },

      openSignedAppFileAsync(root, file, callback) {
        // First try calling the real cert DB
        this._genuine.openSignedAppFileAsync(
          root,
          file,
          (result, zipReader, cert) => {
            verifyCert(file.clone(), result, cert, callback).then(
              ([callback, result, cert]) => {
                callback.openSignedAppFileFinished(result, zipReader, cert);
              }
            );
          }
        );
      },

      QueryInterface: ChromeUtils.generateQI(["nsIX509CertDB"]),
    };

    // Unregister the real database. This only works because the add-ons manager
    // hasn't started up and grabbed the certificate database yet.
    lazy.MockRegistrar.register(CERTDB_CONTRACTID, FakeCertDB);

    // Initialize the mock service.
    Cc[CERTDB_CONTRACTID].getService();
    FakeCertDB.init();
  },

  /**
   * Load the data from the specified files into the *real* blocklist providers.
   * Loads using loadBlocklistRawData, which will treat this as an update.
   *
   * @param {nsIFile} dir
   *        The directory in which the files live.
   * @param {string} prefix
   *        a prefix for the files which ought to be loaded.
   *        This method will suffix -extensions.json
   *        to the prefix it is given, and attempt to load it.
   *        If it exists, its data will be dumped into
   *        the respective store, and the update handler
   *        will be called.
   */
  async loadBlocklistData(dir, prefix) {
    let loadedData = {};
    let fileSuffix = "extensions";
    const fileName = `${prefix}-${fileSuffix}.json`;
    let jsonStr = await OS.File.read(OS.Path.join(dir.path, fileName), {
      encoding: "UTF-8",
    }).catch(() => {});
    if (jsonStr) {
      this.info(`Loaded ${fileName}`);
      loadedData[fileSuffix] = JSON.parse(jsonStr);
    }
    return this.loadBlocklistRawData(loadedData);
  },

  /**
   * Load the following data into the *real* blocklist providers.
   * Fires update methods as would happen if this data came from
   * an actual blocklist update, etc.
   *
   * @param {object} data
   *        The data to load.
   */
  async loadBlocklistRawData(data) {
    const { BlocklistPrivate } = ChromeUtils.import(
      "resource://gre/modules/Blocklist.jsm"
    );
    const blocklistMapping = {
      extensions: BlocklistPrivate.ExtensionBlocklistRS,
      extensionsMLBF: BlocklistPrivate.ExtensionBlocklistMLBF,
    };

    for (const [dataProp, blocklistObj] of Object.entries(blocklistMapping)) {
      let newData = data[dataProp];
      if (!newData) {
        continue;
      }
      if (!Array.isArray(newData)) {
        throw new Error(
          "Expected an array of new items to put in the " +
            dataProp +
            " blocklist!"
        );
      }
      for (let item of newData) {
        if (!item.id) {
          item.id = Services.uuid.generateUUID().number.slice(1, -1);
        }
        if (!item.last_modified) {
          item.last_modified = Date.now();
        }
      }
      blocklistObj.ensureInitialized();
      let db = await blocklistObj._client.db;
      const collectionTimestamp = Math.max(
        ...newData.map(r => r.last_modified)
      );
      await db.importChanges({}, collectionTimestamp, newData, {
        clear: true,
      });
      // We manually call _onUpdate... which is evil, but at the moment kinto doesn't have
      // a better abstraction unless you want to mock your own http server to do the update.
      await blocklistObj._onUpdate();
    }
  },

  /**
   * Starts up the add-on manager as if it was started by the application.
   *
   * @param {Object} params
   *        The new params are in an object and new code should use that.
   * @param {boolean} params.earlyStartup
   *        Notifies early startup phase. default is true
   * @param {boolean} params.lateStartup
   *        Notifies late startup phase which ensures addons are started or
   *        listeners are primed. default is true
   * @param {boolean} params.newVersion
   *        If provided, the application version is changed to this string
   *        before the AddonManager is started.
   */
  async promiseStartupManager(params) {
    if (this.addonIntegrationService) {
      throw new Error(
        "Attempting to startup manager that was already started."
      );
    }
    // Support old arguments
    if (typeof params != "object") {
      params = {
        newVersion: arguments[0],
      };
    }
    let { earlyStartup = true, lateStartup = true, newVersion } = params;

    lateStartup = earlyStartup && lateStartup;

    if (newVersion) {
      this.appInfo.version = newVersion;
      this.appInfo.platformVersion = newVersion;
    }

    // AddonListeners are removed when the addonManager is shutdown,
    // ensure the Extension observer is added.  We call uninit in
    // promiseShutdown to allow re-initialization.
    lazy.ExtensionAddonObserver.init();

    const { XPIInternal, XPIProvider } = ChromeUtils.import(
      "resource://gre/modules/addons/XPIProvider.jsm"
    );
    XPIInternal.overrideAsyncShutdown(MockAsyncShutdown);

    XPIInternal.BootstrapScope.prototype._beforeCallBootstrapMethod = (
      method,
      params,
      reason
    ) => {
      try {
        this.emit("bootstrap-method", { method, params, reason });
      } catch (e) {
        try {
          this.testScope.do_throw(e);
        } catch (e) {
          // Le sigh.
        }
      }
    };

    this.addonIntegrationService = Cc[
      "@mozilla.org/addons/integration;1"
    ].getService(Ci.nsIObserver);

    this.addonIntegrationService.observe(null, "addons-startup", null);

    this.emit("addon-manager-started");

    await Promise.all(XPIProvider.startupPromises);

    // Load the add-ons list as it was after extension registration
    await this.loadAddonsList(true);

    // Wait for all add-ons to finish starting up before resolving.
    await Promise.all(
      Array.from(
        XPIProvider.activeAddons.values(),
        addon => addon.startupPromise
      )
    );
    if (earlyStartup) {
      lazy.ExtensionTestCommon.notifyEarlyStartup();
    }
    if (lateStartup) {
      lazy.ExtensionTestCommon.notifyLateStartup();
    }
  },

  async promiseShutdownManager({
    clearOverrides = true,
    clearL10nRegistry = true,
  } = {}) {
    if (!this.addonIntegrationService) {
      return false;
    }

    if (this.overrideEntry && clearOverrides) {
      this.overrideEntry.destruct();
      this.overrideEntry = null;
    }

    const { XPIProvider } = ChromeUtils.import(
      "resource://gre/modules/addons/XPIProvider.jsm"
    );
    const { XPIDatabase } = ChromeUtils.import(
      "resource://gre/modules/addons/XPIDatabase.jsm"
    );

    // Ensure some startup observers in XPIProvider are released.
    Services.obs.notifyObservers(null, "test-load-xpi-database");

    Services.obs.notifyObservers(null, "quit-application-granted");
    await MockAsyncShutdown.quitApplicationGranted.trigger();

    // If XPIDatabase.asyncLoadDB() has been called before, then _dbPromise is
    // a promise, potentially still pending. Wait for it to settle before
    // triggering profileBeforeChange, because the latter can trigger errors in
    // the pending asyncLoadDB() by an indirect call to XPIDatabase.shutdown().
    await XPIDatabase._dbPromise;

    await MockAsyncShutdown.profileBeforeChange.trigger();
    await MockAsyncShutdown.profileChangeTeardown.trigger();

    this.emit("addon-manager-shutdown");

    this.addonIntegrationService = null;

    // Load the add-ons list as it was after application shutdown
    await this.loadAddonsList();

    // Flush the jar cache entries for each bootstrapped XPI so that
    // we don't run into file locking issues on Windows.
    for (let file of this.addonsList.xpis) {
      Services.obs.notifyObservers(file, "flush-cache-entry");
    }

    // Clear L10nRegistry entries so restaring the AOM will work correctly with locales.
    if (clearL10nRegistry) {
      L10nRegistry.getInstance().clearSources();
    }

    // Clear any crash report annotations
    this.appInfo.annotations = {};

    // Force the XPIProvider provider to reload to better
    // simulate real-world usage.

    // This would be cleaner if I could get it as the rejection reason from
    // the AddonManagerInternal.shutdown() promise
    let shutdownError = XPIDatabase._saveError;

    AddonManagerPrivate.unregisterProvider(XPIProvider);
    Cu.unload("resource://gre/modules/addons/XPIProvider.jsm");
    Cu.unload("resource://gre/modules/addons/XPIDatabase.jsm");
    Cu.unload("resource://gre/modules/addons/XPIInstall.jsm");

    lazy.ExtensionAddonObserver.uninit();

    lazy.ExtensionTestCommon.resetStartupPromises();

    if (shutdownError) {
      throw shutdownError;
    }

    return true;
  },

  /**
   * Asynchronously restart the AddonManager.  If newVersion is provided,
   * simulate an application upgrade (or downgrade) where the version
   * is changed to newVersion when re-started.
   *
   * @param {Object} params
   *        The new params are in an object and new code should use that.
   *        See promiseStartupManager for param details.
   */
  async promiseRestartManager(params) {
    await this.promiseShutdownManager({ clearOverrides: false });
    await this.promiseStartupManager(params);
  },

  /**
   * If promiseStartupManager is called with earlyStartup: false, then
   * use this to notify early startup.
   *
   * @returns {Promise} resolves when notification is complete
   */
  notifyEarlyStartup() {
    return lazy.ExtensionTestCommon.notifyEarlyStartup();
  },

  /**
   * If promiseStartupManager is called with lateStartup: false, then
   * use this to notify late startup.  You should also call early startup
   * if necessary.
   *
   * @returns {Promise} resolves when notification is complete
   */
  notifyLateStartup() {
    return lazy.ExtensionTestCommon.notifyLateStartup();
  },

  async loadAddonsList(flush = false) {
    if (flush) {
      const { XPIInternal } = ChromeUtils.import(
        "resource://gre/modules/addons/XPIProvider.jsm"
      );
      XPIInternal.XPIStates.save();
      await XPIInternal.XPIStates._jsonFile._save();
    }

    this.addonsList = new AddonsList(this.addonStartup);
  },

  /**
   * Recursively create all directories up to and including the given
   * path, if they do not exist.
   *
   * @param {string} path The path of the directory to create.
   * @returns {Promise} Resolves when all directories have been created.
   */
  recursiveMakeDir(path) {
    let paths = [];
    for (
      let lastPath;
      path != lastPath;
      lastPath = path, path = OS.Path.dirname(path)
    ) {
      paths.push(path);
    }

    return Promise.all(
      paths
        .reverse()
        .map(path =>
          OS.File.makeDir(path, { ignoreExisting: true }).catch(() => {})
        )
    );
  },

  /**
   * Writes the given data to a file in the given zip file.
   *
   * @param {string|nsIFile} zipFile
   *        The zip file to write to.
   * @param {Object} files
   *        An object containing filenames and the data to write to the
   *        corresponding paths in the zip file.
   * @param {integer} [flags = 0]
   *        Additional flags to open the file with.
   */
  writeFilesToZip(zipFile, files, flags = 0) {
    if (typeof zipFile == "string") {
      zipFile = nsFile(zipFile);
    }

    var zipW = ZipWriter(
      zipFile,
      FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE | flags
    );

    for (let [path, data] of Object.entries(files)) {
      if (
        typeof data === "object" &&
        ChromeUtils.getClassName(data) === "Object"
      ) {
        data = JSON.stringify(data);
      }
      if (!(data instanceof ArrayBuffer)) {
        data = new TextEncoder("utf-8").encode(data).buffer;
      }

      let stream = ArrayBufferInputStream(data, 0, data.byteLength);

      // Note these files are being created in the XPI archive with date
      // 1 << 49, which is a valid time for ZipWriter.
      zipW.addEntryStream(
        path,
        Math.pow(2, 49),
        Ci.nsIZipWriter.COMPRESSION_NONE,
        stream,
        false
      );
    }

    zipW.close();
  },

  async promiseWriteFilesToZip(zip, files, flags) {
    await this.recursiveMakeDir(OS.Path.dirname(zip));

    this.writeFilesToZip(zip, files, flags);

    return Promise.resolve(nsFile(zip));
  },

  async promiseWriteFilesToDir(dir, files) {
    await this.recursiveMakeDir(dir);

    for (let [path, data] of Object.entries(files)) {
      path = path.split("/");
      let leafName = path.pop();

      // Create parent directories, if necessary.
      let dirPath = dir;
      for (let subDir of path) {
        dirPath = OS.Path.join(dirPath, subDir);
        await OS.File.makeDir(dirPath, { ignoreExisting: true });
      }

      if (
        typeof data == "object" &&
        ChromeUtils.getClassName(data) == "Object"
      ) {
        data = JSON.stringify(data);
      }
      if (typeof data == "string") {
        data = new TextEncoder("utf-8").encode(data);
      }

      await OS.File.writeAtomic(OS.Path.join(dirPath, leafName), data);
    }

    return nsFile(dir);
  },

  promiseWriteFilesToExtension(dir, id, files, unpacked = this.testUnpacked) {
    if (unpacked) {
      let path = OS.Path.join(dir, id);

      return this.promiseWriteFilesToDir(path, files);
    }

    let xpi = OS.Path.join(dir, `${id}.xpi`);

    return this.promiseWriteFilesToZip(xpi, files);
  },

  tempXPIs: [],

  allocTempXPIFile() {
    let file = this.tempDir.clone();
    let uuid = Services.uuid.generateUUID().number.slice(1, -1);
    file.append(`${uuid}.xpi`);

    this.tempXPIs.push(file);

    return file;
  },

  /**
   * Creates an XPI file for some manifest data in the temporary directory and
   * returns the nsIFile for it. The file will be deleted when the test completes.
   *
   * @param {object} files
   *          The object holding data about the add-on
   * @return {nsIFile} A file pointing to the created XPI file
   */
  createTempXPIFile(files) {
    let file = this.allocTempXPIFile();
    this.writeFilesToZip(file.path, files);
    return file;
  },

  /**
   * Creates an XPI file for some WebExtension data in the temporary directory and
   * returns the nsIFile for it. The file will be deleted when the test completes.
   *
   * @param {Object} data
   *        The object holding data about the add-on, as expected by
   *        |ExtensionTestCommon.generateXPI|.
   * @return {nsIFile} A file pointing to the created XPI file
   */
  createTempWebExtensionFile(data) {
    let file = lazy.ExtensionTestCommon.generateXPI(data);
    this.tempXPIs.push(file);
    return file;
  },

  /**
   * Creates an XPI with the given files and installs it.
   *
   * @param {object} files
   *        A files object as would be passed to {@see #createTempXPI}.
   * @returns {Promise}
   *        A promise which resolves when the add-on is installed.
   */
  promiseInstallXPI(files) {
    return this.promiseInstallFile(this.createTempXPIFile(files));
  },

  /**
   * Creates an extension proxy file.
   * See: https://developer.mozilla.org/en-US/Add-ons/Setting_up_extension_development_environment#Firefox_extension_proxy_file
   *
   * @param {nsIFile} dir
   *        The directory to add the proxy file to.
   * @param {nsIFile} addon
   *        An nsIFile for the add-on file that this is a proxy file for.
   * @param {string} id
   *        A string to use for the add-on ID.
   * @returns {Promise} Resolves when the file has been created.
   */
  promiseWriteProxyFileToDir(dir, addon, id) {
    let files = {
      [id]: addon.path,
    };

    return this.promiseWriteFilesToDir(dir.path, files);
  },

  /**
   * Manually installs an XPI file into an install location by either copying the
   * XPI there or extracting it depending on whether unpacking is being tested
   * or not.
   *
   * @param {nsIFile} xpiFile
   *        The XPI file to install.
   * @param {nsIFile} [installLocation = this.profileExtensions]
   *        The install location (an nsIFile) to install into.
   * @param {string} [id]
   *        The ID to install as.
   * @param {boolean} [unpacked = this.testUnpacked]
   *        If true, install as an unpacked directory, rather than a
   *        packed XPI.
   * @returns {nsIFile}
   *        A file pointing to the installed location of the XPI file or
   *        unpacked directory.
   */
  async manuallyInstall(
    xpiFile,
    installLocation = this.profileExtensions,
    id = null,
    unpacked = this.testUnpacked
  ) {
    if (id == null) {
      id = await this.getIDFromExtension(xpiFile);
    }

    if (unpacked) {
      let dir = installLocation.clone();
      dir.append(id);
      dir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

      let zip = ZipReader(xpiFile);
      for (let entry of zip.findEntries(null)) {
        let target = dir.clone();
        for (let part of entry.split("/")) {
          target.append(part);
        }
        if (!target.parent.exists()) {
          target.parent.create(
            Ci.nsIFile.DIRECTORY_TYPE,
            FileUtils.PERMS_DIRECTORY
          );
        }
        try {
          zip.extract(entry, target);
        } catch (e) {
          if (
            e.result != Cr.NS_ERROR_FILE_DIR_NOT_EMPTY &&
            !(target.exists() && target.isDirectory())
          ) {
            throw e;
          }
        }
        target.permissions |= FileUtils.PERMS_FILE;
      }
      zip.close();

      return dir;
    }

    let target = installLocation.clone();
    target.append(`${id}.xpi`);
    xpiFile.copyTo(target.parent, target.leafName);
    return target;
  },

  /**
   * Manually uninstalls an add-on by removing its files from the install
   * location.
   *
   * @param {nsIFile} installLocation
   *        The nsIFile of the install location to remove from.
   * @param {string} id
   *        The ID of the add-on to remove.
   * @param {boolean} [unpacked = this.testUnpacked]
   *        If true, uninstall an unpacked directory, rather than a
   *        packed XPI.
   */
  manuallyUninstall(installLocation, id, unpacked = this.testUnpacked) {
    let file = this.getFileForAddon(installLocation, id, unpacked);

    // In reality because the app is restarted a flush isn't necessary for XPIs
    // removed outside the app, but for testing we must flush manually.
    if (file.isFile()) {
      Services.obs.notifyObservers(file, "flush-cache-entry");
    }

    file.remove(true);
  },

  /**
   * Gets the nsIFile for where an add-on is installed. It may point to a file or
   * a directory depending on whether add-ons are being installed unpacked or not.
   *
   * @param {nsIFile} dir
   *         The nsIFile for the install location
   * @param {string} id
   *        The ID of the add-on
   * @param {boolean} [unpacked = this.testUnpacked]
   *        If true, return the path to an unpacked directory, rather than a
   *        packed XPI.
   * @returns {nsIFile}
   *        A file pointing to the XPI file or unpacked directory where
   *        the add-on should be installed.
   */
  getFileForAddon(dir, id, unpacked = this.testUnpacked) {
    dir = dir.clone();
    if (unpacked) {
      dir.append(id);
    } else {
      dir.append(`${id}.xpi`);
    }
    return dir;
  },

  /**
   * Sets the last modified time of the extension, usually to trigger an update
   * of its metadata.
   *
   * @param {nsIFile} ext A file pointing to either the packed extension or its unpacked directory.
   * @param {number} time The time to which we set the lastModifiedTime of the extension
   *
   * @deprecated Please use promiseSetExtensionModifiedTime instead
   */
  setExtensionModifiedTime(ext, time) {
    ext.lastModifiedTime = time;
    if (ext.isDirectory()) {
      for (let file of this.iterDirectory(ext)) {
        this.setExtensionModifiedTime(file, time);
      }
    }
  },

  async promiseSetExtensionModifiedTime(path, time) {
    await IOUtils.setModificationTime(path, time);

    let iterator = new OS.File.DirectoryIterator(path);
    try {
      await iterator.forEach(entry => {
        return this.promiseSetExtensionModifiedTime(entry.path, time);
      });
    } catch (ex) {
      if (ex instanceof OS.File.Error) {
        return;
      }
      throw ex;
    } finally {
      iterator.close().catch(() => {});
    }
  },

  registerDirectory(key, dir) {
    var dirProvider = {
      getFile(prop, persistent) {
        persistent.value = false;
        if (prop == key) {
          return dir.clone();
        }
        return null;
      },

      QueryInterface: ChromeUtils.generateQI(["nsIDirectoryServiceProvider"]),
    };
    Services.dirsvc.registerProvider(dirProvider);

    try {
      Services.dirsvc.undefine(key);
    } catch (e) {
      // This throws if the key is not already registered, but that
      // doesn't matter.
      if (e.result != Cr.NS_ERROR_FAILURE) {
        throw e;
      }
    }
  },

  /**
   * Returns a promise that resolves when the given add-on event is fired. The
   * resolved value is an array of arguments passed for the event.
   *
   * @param {string} event
   *        The name of the AddonListener event handler method for which
   *        an event is expected.
   * @param {function} checkFn [optional]
   *        A function to check if this is the right event. Should return true
   *        for the event that it wants, false otherwise. Will be passed
   *        all the relevant arguments.
   *        If not passed, any event will do to resolve the promise.
   * @returns {Promise<Array>}
   *        Resolves to an array containing the event handler's
   *        arguments the first time it is called.
   */
  promiseAddonEvent(event, checkFn) {
    return new Promise(resolve => {
      let listener = {
        [event](...args) {
          if (typeof checkFn == "function" && !checkFn(...args)) {
            return;
          }
          AddonManager.removeAddonListener(listener);
          resolve(args);
        },
      };

      AddonManager.addAddonListener(listener);
    });
  },

  promiseInstallEvent(event) {
    return new Promise(resolve => {
      let listener = {
        [event](...args) {
          AddonManager.removeInstallListener(listener);
          resolve(args);
        },
      };

      AddonManager.addInstallListener(listener);
    });
  },

  /**
   * A helper method to install AddonInstall and wait for completion.
   *
   * @param {AddonInstall} install
   *        The add-on to install.
   * @returns {Promise<AddonInstall>}
   *        Resolves when the install completes, either successfully or
   *        in failure.
   */
  promiseCompleteInstall(install) {
    let listener;
    return new Promise(resolve => {
      let installPromise;
      listener = {
        onDownloadFailed: resolve,
        onDownloadCancelled: resolve,
        onInstallFailed: resolve,
        onInstallCancelled: resolve,
        onInstallEnded() {
          // onInstallEnded is called right when an add-on has been installed.
          // install() may still be pending, e.g. for updates, and be awaiting
          // the completion of the update, part of which is the removal of the
          // temporary XPI file of the downloaded update. To avoid intermittent
          // test failures due to lingering temporary files, await install().
          resolve(installPromise);
        },
        onInstallPostponed: resolve,
      };

      install.addListener(listener);
      installPromise = install.install();
    }).then(() => {
      install.removeListener(listener);
      return install;
    });
  },

  /**
   * A helper method to install a file.
   *
   * @param {nsIFile} file
   *        The file to install
   * @param {boolean} [ignoreIncompatible = false]
   *        Optional parameter to ignore add-ons that are incompatible
   *        with the application
   * @param {Object} [installTelemetryInfo = undefined]
   *        Optional parameter to set the install telemetry info for the
   *        installed addon
   * @returns {Promise}
   *        Resolves when the install has completed.
   */
  async promiseInstallFile(
    file,
    ignoreIncompatible = false,
    installTelemetryInfo
  ) {
    let install = await AddonManager.getInstallForFile(
      file,
      null,
      installTelemetryInfo
    );
    if (!install) {
      throw new Error(`No AddonInstall created for ${file.path}`);
    }

    if (install.state != AddonManager.STATE_DOWNLOADED) {
      throw new Error(
        `Expected file to be downloaded for install of ${file.path}`
      );
    }

    if (ignoreIncompatible && install.addon.appDisabled) {
      return null;
    }

    await install.install();
    return install;
  },

  /**
   * A helper method to install an array of files.
   *
   * @param {Iterable<nsIFile>} files
   *        The files to install
   * @param {boolean} [ignoreIncompatible = false]
   *        Optional parameter to ignore add-ons that are incompatible
   *        with the application
   * @returns {Promise}
   *        Resolves when the installs have completed.
   */
  promiseInstallAllFiles(files, ignoreIncompatible = false) {
    return Promise.all(
      Array.from(files, file =>
        this.promiseInstallFile(file, ignoreIncompatible)
      )
    );
  },

  promiseCompleteAllInstalls(installs) {
    return Promise.all(Array.from(installs, this.promiseCompleteInstall));
  },

  /**
   * @property {number} updateReason
   *        The default update reason for {@see promiseFindAddonUpdates}
   *        calls. May be overwritten by tests which primarily check for
   *        updates with a particular reason.
   */
  updateReason: AddonManager.UPDATE_WHEN_PERIODIC_UPDATE,

  /**
   * Returns a promise that will be resolved when an add-on update check is
   * complete. The value resolved will be an AddonInstall if a new version was
   * found.
   *
   * @param {object} addon The add-on to find updates for.
   * @param {integer} reason The type of update to find.
   * @param {Array} args Additional args to pass to `checkUpdates` after
   *                     the update reason.
   * @return {Promise<object>} an object containing information about the update.
   */
  promiseFindAddonUpdates(
    addon,
    reason = AddonTestUtils.updateReason,
    ...args
  ) {
    // Retrieve the test assertion helper from the testScope
    // (which is `equal` in xpcshell-test and `is` in mochitest)
    let equal = this.testScope.equal || this.testScope.is;
    return new Promise((resolve, reject) => {
      let result = {};
      addon.findUpdates(
        {
          onNoCompatibilityUpdateAvailable(addon2) {
            if ("compatibilityUpdate" in result) {
              throw new Error("Saw multiple compatibility update events");
            }
            equal(addon, addon2, "onNoCompatibilityUpdateAvailable");
            result.compatibilityUpdate = false;
          },

          onCompatibilityUpdateAvailable(addon2) {
            if ("compatibilityUpdate" in result) {
              throw new Error("Saw multiple compatibility update events");
            }
            equal(addon, addon2, "onCompatibilityUpdateAvailable");
            result.compatibilityUpdate = true;
          },

          onNoUpdateAvailable(addon2) {
            if ("updateAvailable" in result) {
              throw new Error("Saw multiple update available events");
            }
            equal(addon, addon2, "onNoUpdateAvailable");
            result.updateAvailable = false;
          },

          onUpdateAvailable(addon2, install) {
            if ("updateAvailable" in result) {
              throw new Error("Saw multiple update available events");
            }
            equal(addon, addon2, "onUpdateAvailable");
            result.updateAvailable = install;
          },

          onUpdateFinished(addon2, error) {
            equal(addon, addon2, "onUpdateFinished");
            if (error == AddonManager.UPDATE_STATUS_NO_ERROR) {
              resolve(result);
            } else {
              result.error = error;
              reject(result);
            }
          },
        },
        reason,
        ...args
      );
    });
  },

  /**
   * Monitors console output for the duration of a task, and returns a promise
   * which resolves to a tuple containing a list of all console messages
   * generated during the task's execution, and the result of the task itself.
   *
   * @param {function} task
   *        The task to run while monitoring console output. May be
   *        an async function, or an ordinary function which returns a promose.
   * @return {Promise<[Array<nsIConsoleMessage>, *]>}
   *        Resolves to an object containing a `messages` property, with
   *        the array of console messages emitted during the execution
   *        of the task, and a `result` property, containing the task's
   *        return value.
   */
  async promiseConsoleOutput(task) {
    const DONE = "=== xpcshell test console listener done ===";

    let listener,
      messages = [];
    let awaitListener = new Promise(resolve => {
      listener = msg => {
        if (msg == DONE) {
          resolve();
        } else {
          msg instanceof Ci.nsIScriptError;
          messages.push(msg);
        }
      };
    });

    Services.console.registerListener(listener);
    try {
      let result = await task();

      Services.console.logStringMessage(DONE);
      await awaitListener;

      return { messages, result };
    } finally {
      Services.console.unregisterListener(listener);
    }
  },

  /**
   * An object describing an expected or forbidden console message. Each
   * property in the object corresponds to a property with the same name
   * in a console message. If the value in the pattern object is a
   * regular expression, it must match the value of the corresponding
   * console message property. If it is any other value, it must be
   * strictly equal to the correspondng console message property.
   *
   * @typedef {object} ConsoleMessagePattern
   */

  /**
   * Checks the list of messages returned from `promiseConsoleOutput`
   * against the given set of expected messages.
   *
   * This is roughly equivalent to the expected and forbidden message
   * matching functionality of SimpleTest.monitorConsole.
   *
   * @param {Array<object>} messages
   *        The array of console messages to match.
   * @param {object} options
   *        Options describing how to perform the match.
   * @param {Array<ConsoleMessagePattern>} [options.expected = []]
   *        An array of messages which must appear in `messages`. The
   *        matching messages in the `messages` array must appear in the
   *        same order as the patterns in the `expected` array.
   * @param {Array<ConsoleMessagePattern>} [options.forbidden = []]
   *        An array of messages which must not appear in the `messages`
   *        array.
   * @param {bool} [options.forbidUnexpected = false]
   *        If true, the `messages` array must not contain any messages
   *        which are not matched by the given `expected` patterns.
   */
  checkMessages(
    messages,
    { expected = [], forbidden = [], forbidUnexpected = false }
  ) {
    function msgMatches(msg, expectedMsg) {
      for (let [prop, pattern] of Object.entries(expectedMsg)) {
        if (isRegExp(pattern) && typeof msg[prop] === "string") {
          if (!pattern.test(msg[prop])) {
            return false;
          }
        } else if (msg[prop] !== pattern) {
          return false;
        }
      }
      return true;
    }

    function validateOptionFormat(optionName, optionValue) {
      for (let item of optionValue) {
        if (!item || typeof item !== "object" || isRegExp(item)) {
          throw new Error(
            `Unexpected format in AddonTestUtils.checkMessages "${optionName}" parameter`
          );
        }
      }
    }

    validateOptionFormat("expected", expected);
    validateOptionFormat("forbidden", forbidden);

    let i = 0;
    for (let msg of messages) {
      if (forbidden.some(pat => msgMatches(msg, pat))) {
        this.testScope.ok(false, `Got forbidden console message: ${msg}`);
        continue;
      }

      if (i < expected.length && msgMatches(msg, expected[i])) {
        this.info(`Matched expected console message: ${msg}`);
        i++;
      } else if (forbidUnexpected) {
        this.testScope.ok(false, `Got unexpected console message: ${msg}`);
      }
    }
    for (let pat of expected.slice(i)) {
      this.testScope.ok(
        false,
        `Did not get expected console message: ${uneval(pat)}`
      );
    }
  },

  /**
   * Asserts that the expected installTelemetryInfo properties are available
   * on the AddonWrapper or AddonInstall objects.
   *
   * @param {AddonWrapper|AddonInstall} addonOrInstall
   *        The addon or addonInstall object to check.
   * @param {Object} expectedInstallInfo
   *        The expected installTelemetryInfo properties
   *        (every property can be a primitive value or a regular expression).
   * @param {string} [msg]
   *        Optional assertion message suffix.
   */
  checkInstallInfo(addonOrInstall, expectedInstallInfo, msg = undefined) {
    const installInfo = addonOrInstall.installTelemetryInfo;
    const { Assert } = this.testScope;

    msg = msg ? ` ${msg}` : "";

    for (const key of Object.keys(expectedInstallInfo)) {
      const actual = installInfo[key];
      let expected = expectedInstallInfo[key];

      // Assert the property value using a regular expression.
      if (expected && typeof expected.test == "function") {
        Assert.ok(
          expected.test(actual),
          `${key} value "${actual}" has the value expected "${expected}"${msg}`
        );
      } else {
        Assert.deepEqual(
          actual,
          expected,
          `Got the expected value for ${key}${msg}`
        );
      }
    }
  },

  /**
   * Helper to wait for a webextension to completely start
   *
   * @param {string} [id]
   *        An optional extension id to look for.
   *
   * @returns {Promise<Extension>}
   *           A promise that resolves with the extension, once it is started.
   */
  promiseWebExtensionStartup(id) {
    return new Promise(resolve => {
      lazy.Management.on("ready", function listener(event, extension) {
        if (!id || extension.id == id) {
          lazy.Management.off("ready", listener);
          resolve(extension);
        }
      });
    });
  },

  /**
   * Wait until an extension with a search provider has been loaded.
   * This should be called after the extension has started, but before shutdown.
   *
   * @param {object} extension
   *        The return value of ExtensionTestUtils.loadExtension.
   *        For browser tests, see mochitest/tests/SimpleTest/ExtensionTestUtils.js
   *        For xpcshell tests, see toolkit/components/extensions/ExtensionXPCShellUtils.jsm
   * @param {object} [options]
   *        Optional options.
   * @param {boolean} [options.expectPending = false]
   *        Whether to expect the search provider to still be starting up.
   */
  async waitForSearchProviderStartup(
    extension,
    { expectPending = false } = {}
  ) {
    // In xpcshell tests, equal/ok are defined in the global scope.
    let { equal, ok } = this.testScope;
    if (!equal || !ok) {
      // In mochitests, these are available via Assert.jsm.
      let { Assert } = this.testScope;
      equal = Assert.equal.bind(Assert);
      ok = Assert.ok.bind(Assert);
    }

    equal(
      extension.state,
      "running",
      "Search provider extension should be running"
    );
    ok(extension.id, "Extension ID of search provider should be set");

    // The map of promises from browser/components/extensions/parent/ext-chrome-settings-overrides.js
    let { pendingSearchSetupTasks } = lazy.Management.global;
    let searchStartupPromise = pendingSearchSetupTasks.get(extension.id);
    if (expectPending) {
      ok(
        searchStartupPromise,
        "Search provider registration should be in progress"
      );
    }
    return searchStartupPromise;
  },

  /**
   * Initializes the URLPreloader, which is required in order to load
   * built_in_addons.json. This has the side-effect of setting
   * preferences which flip Cu.isInAutomation to true.
   */
  initializeURLPreloader() {
    Services.prefs.setBoolPref(PREF_DISABLE_SECURITY, true);
    lazy.aomStartup.initializeURLPreloader();
  },

  /**
   * Override chrome URL for specifying allowed built-in add-ons.
   *
   * @param {object} data - An object specifying which add-on IDs are permitted
   *                        to load, for instance: { "system": ["id1", "..."] }
   */
  async overrideBuiltIns(data) {
    // We need to set this in order load the URL preloader service, which
    // is only possible when running in automation.
    let prevPrefVal = Services.prefs.getBoolPref(PREF_DISABLE_SECURITY, false);
    this.initializeURLPreloader();

    let file = this.tempDir.clone();
    file.append("override.txt");
    this.tempXPIs.push(file);

    let manifest = Services.io.newFileURI(file);
    await OS.File.writeAtomic(
      file.path,
      new TextEncoder().encode(JSON.stringify(data))
    );
    this.overrideEntry = lazy.aomStartup.registerChrome(manifest, [
      [
        "override",
        "chrome://browser/content/built_in_addons.json",
        Services.io.newFileURI(file).spec,
      ],
    ]);
    Services.prefs.setBoolPref(PREF_DISABLE_SECURITY, prevPrefVal);
  },

  // AMTelemetry events helpers.

  /**
   * Redefine AMTelemetry.recordEvent to collect the recorded telemetry events and
   * ensure that there are no unexamined events after the test file is exiting.
   */
  hookAMTelemetryEvents() {
    let originalRecordEvent = lazy.AMTelemetry.recordEvent;
    lazy.AMTelemetry.recordEvent = event => {
      this.collectedTelemetryEvents.push(event);
    };
    this.testScope.registerCleanupFunction(() => {
      this.testScope.Assert.deepEqual(
        [],
        this.collectedTelemetryEvents,
        "No unexamined telemetry events after test is finished"
      );
      lazy.AMTelemetry.recordEvent = originalRecordEvent;
    });
  },

  /**
   * Retrive any AMTelemetry event collected and empty the array of the collected events.
   *
   * @returns {Array<Object>}
   *          The array of the collected telemetry data.
   */
  getAMTelemetryEvents() {
    let events = this.collectedTelemetryEvents;
    this.collectedTelemetryEvents = [];
    return events;
  },
};

for (let [key, val] of Object.entries(AddonTestUtils)) {
  if (typeof val == "function") {
    AddonTestUtils[key] = val.bind(AddonTestUtils);
  }
}

EventEmitter.decorate(AddonTestUtils);
