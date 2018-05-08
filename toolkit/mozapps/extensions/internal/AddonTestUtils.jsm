/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* eslint "mozilla/no-aArgs": 1 */
/* eslint "no-unused-vars": [2, {"args": "none", "varsIgnorePattern": "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$"}] */
/* eslint "semi": [2, "always"] */
/* eslint "valid-jsdoc": [2, {requireReturn: false}] */

var EXPORTED_SYMBOLS = ["AddonTestUtils", "MockAsyncShutdown"];

const CERTDB_CONTRACTID = "@mozilla.org/security/x509certdb;1";

Cu.importGlobalProperties(["fetch", "TextEncoder"]);

ChromeUtils.import("resource://gre/modules/AsyncShutdown.jsm");
ChromeUtils.import("resource://gre/modules/FileUtils.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const {EventEmitter} = ChromeUtils.import("resource://gre/modules/EventEmitter.jsm", {});
const {OS} = ChromeUtils.import("resource://gre/modules/osfile.jsm", {});

ChromeUtils.defineModuleGetter(this, "Extension",
                               "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyGetter(this, "Management", () => {
  let {Management} = ChromeUtils.import("resource://gre/modules/Extension.jsm", {});
  return Management;
});

ChromeUtils.defineModuleGetter(this, "FileTestUtils",
                               "resource://testing-common/FileTestUtils.jsm");
ChromeUtils.defineModuleGetter(this, "HttpServer",
                               "resource://testing-common/httpd.js");
ChromeUtils.defineModuleGetter(this, "MockRegistrar",
                               "resource://testing-common/MockRegistrar.jsm");

XPCOMUtils.defineLazyServiceGetters(this, {
  aomStartup: ["@mozilla.org/addons/addon-manager-startup;1", "amIAddonManagerStartup"],
  proxyService: ["@mozilla.org/network/protocol-proxy-service;1", "nsIProtocolProxyService"],
  rdfService: ["@mozilla.org/rdf/rdf-service;1", "nsIRDFService"],
  uuidGen: ["@mozilla.org/uuid-generator;1", "nsIUUIDGenerator"],
});

XPCOMUtils.defineLazyGetter(this, "AppInfo", () => {
  let AppInfo = {};
  ChromeUtils.import("resource://testing-common/AppInfo.jsm", AppInfo);
  return AppInfo;
});

const PREF_DISABLE_SECURITY = ("security.turn_off_all_security_so_that_" +
                               "viruses_can_take_over_this_computer");

const ArrayBufferInputStream = Components.Constructor(
  "@mozilla.org/io/arraybuffer-input-stream;1",
  "nsIArrayBufferInputStream", "setData");

const nsFile = Components.Constructor(
  "@mozilla.org/file/local;1",
  "nsIFile", "initWithPath");

const RDFXMLParser = Components.Constructor(
  "@mozilla.org/rdf/xml-parser;1",
  "nsIRDFXMLParser", "parseString");

const RDFDataSource = Components.Constructor(
  "@mozilla.org/rdf/datasource;1?name=in-memory-datasource",
  "nsIRDFDataSource");

const ZipReader = Components.Constructor(
  "@mozilla.org/libjar/zip-reader;1",
  "nsIZipReader", "open");

const ZipWriter = Components.Constructor(
  "@mozilla.org/zipwriter;1",
  "nsIZipWriter", "open");

function isRegExp(val) {
  return val && typeof val === "object" && typeof val.test === "function";
}

// We need some internal bits of AddonManager
var AMscope = ChromeUtils.import("resource://gre/modules/AddonManager.jsm", {});
var {AddonManager, AddonManagerPrivate} = AMscope;


// Mock out AddonManager's reference to the AsyncShutdown module so we can shut
// down AddonManager from the test
var MockAsyncShutdown = {
  hook: null,
  status: null,
  profileBeforeChange: {
    addBlocker(name, blocker, options) {
      MockAsyncShutdown.hook = blocker;
      MockAsyncShutdown.status = options.fetchState;
    }
  },
  // We can use the real Barrier
  Barrier: AsyncShutdown.Barrier,
};

AMscope.AsyncShutdown = MockAsyncShutdown;

class MockBlocklist {
  constructor(addons) {
    if (ChromeUtils.getClassName(addons) === "Object") {
      addons = new Map(Object.entries(addons));
    }
    this.addons = addons;
    this.wrappedJSObject = this;

    // Copy blocklist constants.
    for (let [k, v] of Object.entries(Ci.nsIBlocklistService)) {
      if (typeof v === "number") {
        this[k] = v;
      }
    }

    this._xpidb = ChromeUtils.import("resource://gre/modules/addons/XPIDatabase.jsm", null);
  }

  get contractID() {
    return "@mozilla.org/extensions/blocklist;1";
  }

  _reLazifyService() {
    XPCOMUtils.defineLazyServiceGetter(Services, "blocklist", this.contractID);
    ChromeUtils.defineModuleGetter(this._xpidb, "Blocklist", "resource://gre/modules/Blocklist.jsm");
  }

  register() {
    this.originalCID = MockRegistrar.register(this.contractID, this);
    this._reLazifyService();
    this._xpidb.Blocklist = this;
  }

  unregister() {
    MockRegistrar.unregister(this.originalCID);
    this._reLazifyService();
  }

  async getAddonBlocklistState(addon, appVersion, toolkitVersion) {
    await new Promise(r => setTimeout(r, 150));
    return this.addons.get(addon.id) || Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  }

  async getAddonBlocklistEntry(addon, appVersion, toolkitVersion) {
    let state = await this.getAddonBlocklistState(addon, appVersion, toolkitVersion);
    if (state != Ci.nsIBlocklistService.STATE_NOT_BLOCKED) {
      return {
        state,
        url: "http://example.com/",
      };
    }
    return null;
  }

  async getPluginBlocklistState(plugin, version, appVersion, toolkitVersion) {
    await new Promise(r => setTimeout(r, 150));
    return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  }
}

MockBlocklist.prototype.QueryInterface = ChromeUtils.generateQI(["nsIBlocklistService"]);


/**
 * Escapes any occurrences of &, ", < or > with XML entities.
 *
 * @param {string} str
 *        The string to escape.
 * @return {string} The escaped string.
 */
function escapeXML(str) {
  let replacements = {"&": "&amp;", '"': "&quot;", "'": "&apos;", "<": "&lt;", ">": "&gt;"};
  return String(str).replace(/[&"''<>]/g, m => replacements[m]);
}

/**
 * A tagged template function which escapes any XML metacharacters in
 * interpolated values.
 *
 * @param {Array<string>} strings
 *        An array of literal strings extracted from the templates.
 * @param {Array} values
 *        An array of interpolated values extracted from the template.
 * @returns {string}
 *        The result of the escaped values interpolated with the literal
 *        strings.
 */
function escaped(strings, ...values) {
  let result = [];

  for (let [i, string] of strings.entries()) {
    result.push(string);
    if (i < values.length)
      result.push(escapeXML(values[i]));
  }

  return result.join("");
}


class AddonsList {
  constructor(file) {
    this.extensions = [];
    this.bootstrapped = [];
    this.themes = [];
    this.xpis = [];

    if (!file.exists()) {
      return;
    }

    let data = aomStartup.readStartupData();

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
        } else {
          file = new nsFile(addon.path);
        }

        this.xpis.push(file);

        if (addon.enabled) {
          addon.type = addon.type || "extension";

          if (addon.type == "theme") {
            this.themes.push(file);
          } else if (addon.bootstrapped) {
            this.bootstrapped.push(file);
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
      if (!file.exists())
        throw new Error(`Non-existent path found in addonStartup.json: ${file.path}`);

      if (file.isDirectory())
        return file.equals(path);
      if (file.isFile())
        return file.equals(xpiPath);
      return false;
    });
  }

  hasTheme(dir, id) {
    return this.hasItem("themes", dir, id);
  }

  hasBootstrapped(dir, id) {
    return this.hasItem("bootstrapped", dir, id);
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
  testUnpacked: false,
  useRealCertChecks: false,
  usePrivilegedSignatures: true,
  overrideEntry: null,

  maybeInit(testScope) {
    if (this.testScope != testScope) {
      this.init(testScope);
    }
  },

  init(testScope) {
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
    appDirForAddons.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    this.registerDirectory("XREAddonAppDir", appDirForAddons);


    // Enable more extensive EM logging
    Services.prefs.setBoolPref("extensions.logging.enabled", true);

    // By default only load extensions from the profile install location
    Services.prefs.setIntPref("extensions.enabledScopes", AddonManager.SCOPE_PROFILE);

    // By default don't disable add-ons from any scope
    Services.prefs.setIntPref("extensions.autoDisableScopes", 0);

    // And scan for changes at startup
    Services.prefs.setIntPref("extensions.startupScanScopes", 15);

    // By default, don't cache add-ons in AddonRepository.jsm
    Services.prefs.setBoolPref("extensions.getAddons.cache.enabled", false);

    // Point update checks to the local machine for fast failures
    Services.prefs.setCharPref("extensions.update.url", "http://127.0.0.1/updateURL");
    Services.prefs.setCharPref("extensions.update.background.url", "http://127.0.0.1/updateBackgroundURL");
    Services.prefs.setCharPref("extensions.blocklist.url", "http://127.0.0.1/blocklistURL");
    Services.prefs.setCharPref("services.settings.server", "http://localhost/dummy-kinto/v1");

    // By default ignore bundled add-ons
    Services.prefs.setBoolPref("extensions.installDistroAddons", false);

    // By default don't check for hotfixes
    Services.prefs.setCharPref("extensions.hotfix.id", "");

    // Ensure signature checks are enabled by default
    Services.prefs.setBoolPref("xpinstall.signatures.required", true);


    // Write out an empty blocklist.xml file to the profile to ensure nothing
    // is blocklisted by default
    var blockFile = OS.Path.join(this.profileDir.path, "blocklist.xml");

    var data = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
               "<blocklist xmlns=\"http://www.mozilla.org/2006/addons-blocklist\">\n" +
               "</blocklist>\n";

    this.awaitPromise(OS.File.writeAtomic(blockFile, new TextEncoder().encode(data)));


    // Make sure that a given path does not exist
    function pathShouldntExist(file) {
      if (file.exists()) {
        throw new Error(`Test cleanup: path ${file.path} exists when it should not`);
      }
    }

    testScope.registerCleanupFunction(() => {
      this.cleanupTempXPIs();

      let ignoreEntries = new Set();
      {
        // FileTestUtils lazily creates a directory to hold the temporary files
        // it creates. If that directory exists, ignore it.
        let {value} = Object.getOwnPropertyDescriptor(FileTestUtils,
                                                      "_globalTemporaryDirectory");
        if (value) {
          ignoreEntries.add(value.leafName);
        }
      }

      // Check that the temporary directory is empty
      var dirEntries = this.tempDir.directoryEntries
                           .QueryInterface(Ci.nsIDirectoryEnumerator);
      var entries = [];
      while (dirEntries.hasMoreElements()) {
        let {leafName} = dirEntries.nextFile;
        if (!ignoreEntries.has(leafName)) {
          entries.push(leafName);
        }
      }
      if (entries.length)
        throw new Error(`Found unexpected files in temporary directory: ${entries.join(", ")}`);

      dirEntries.close();

      try {
        appDirForAddons.remove(true);
      } catch (ex) {
        testScope.info(`Got exception removing addon app dir: ${ex}`);
      }

      // ensure no leftover files in the system addon upgrade location
      let featuresDir = this.profileDir.clone();
      featuresDir.append("features");
      // upgrade directories will be in UUID folders under features/
      let systemAddonDirs = [];
      if (featuresDir.exists()) {
        let featuresDirEntries = featuresDir.directoryEntries
                                            .QueryInterface(Ci.nsIDirectoryEnumerator);
        while (featuresDirEntries.hasMoreElements()) {
          let entry = featuresDirEntries.getNext();
          entry.QueryInterface(Ci.nsIFile);
          systemAddonDirs.push(entry);
        }

        systemAddonDirs.map(dir => {
          dir.append("stage");
          pathShouldntExist(dir);
        });
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
    this.profileDir = FileUtils.getDir("ProfD", []);

    this.profileExtensions = FileUtils.getDir("ProfD", ["extensions"]);

    this.tempDir = FileUtils.getDir("TmpD", []);
    this.tempDir.append("addons-mochitest");
    this.tempDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

    testScope.registerCleanupFunction(() => {
      this.cleanupTempXPIs();
      try {
        this.tempDir.remove(true);
      } catch (e) {
        Cu.reportError(e);
      }
    });
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
  createHttpServer({port = -1, hosts} = {}) {
    let server = new HttpServer();
    server.start(port);

    if (hosts) {
      hosts = new Set(hosts);
      const serverHost = "localhost";
      const serverPort = server.identity.primaryPort;

      for (let host of hosts) {
        server.identity.add("http", host, 80);
      }

      const proxyFilter = {
        proxyInfo: proxyService.newProxyInfo("http", serverHost, serverPort, 0, 4096, null),

        applyFilter(service, channel, defaultProxyInfo, callback) {
          if (hosts.has(channel.URI.host)) {
            callback.onProxyFilterResult(this.proxyInfo);
          } else {
            callback.onProxyFilterResult(defaultProxyInfo);
          }
        },
      };

      proxyService.registerChannelFilter(proxyFilter, 0);
      this.testScope.registerCleanupFunction(() => {
        proxyService.unregisterChannelFilter(proxyFilter);
      });
    }

    this.testScope.registerCleanupFunction(() => {
      return new Promise(resolve => {
        server.stop(resolve);
      });
    });

    return server;
  },

  info(msg) {
    // info() for mochitests, do_print for xpcshell.
    let print = this.testScope.info || this.testScope.do_print;
    print(msg);
  },

  cleanupTempXPIs() {
    for (let file of this.tempXPIs.splice(0)) {
      if (file.exists()) {
        try {
          file.remove(false);
        } catch (e) {
          Cu.reportError(e);
        }
      }
    }
  },

  /**
   * Helper to spin the event loop until a promise resolves or rejects
   *
   * @param {Promise} promise
   *        The promise to wait on.
   * @returns {*} The promise's resolution value.
   * @throws The promise's rejection value, if it rejects.
   */
  awaitPromise(promise) {
    let done = false;
    let result;
    let error;
    promise.then(
      val => { result = val; },
      err => { error = err; }
    ).then(() => {
      done = true;
    });

    Services.tm.spinEventLoopUntil(() => done);

    if (error !== undefined)
      throw error;
    return result;
  },

  createAppInfo(ID, name, version, platformVersion = "1.0") {
    AppInfo.updateAppInfo({
      ID, name, version, platformVersion,
      crashReporter: true,
      extraProps: {
        browserTabsRemoteAutostart: false,
      },
    });
    this.appInfo = AppInfo.getAppInfo();
  },

  getManifestURI(file) {
    if (file.isDirectory()) {
      file.append("install.rdf");
      if (file.exists()) {
        return NetUtil.newURI(file);
      }

      file.leafName = "manifest.json";
      if (file.exists())
        return NetUtil.newURI(file);

      throw new Error("No manifest file present");
    }

    let zip = ZipReader(file);
    try {
      let uri = NetUtil.newURI(file);

      if (zip.hasEntry("install.rdf")) {
        return NetUtil.newURI(`jar:${uri.spec}!/install.rdf`);
      }

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

    if (manifestURI.spec.endsWith(".rdf")) {
      let data = await body.text();

      let ds = new RDFDataSource();
      new RDFXMLParser(ds, manifestURI, data);

      let rdfID = ds.GetTarget(rdfService.GetResource("urn:mozilla:install-manifest"),
                               rdfService.GetResource("http://www.mozilla.org/2004/em-rdf#id"),
                               true);
      return rdfID.QueryInterface(Ci.nsIRDFLiteral).Value;
    }

    let manifest = await body.json();
    try {
      return manifest.applications.gecko.id;
    } catch (e) {
      // IDs for WebExtensions are extracted from the certificate when
      // not present in the manifest, so just generate a random one.
      return uuidGen.generateUUID().number;
    }
  },

  overrideCertDB() {
    let verifyCert = async (file, result, cert, callback) => {
      if (result == Cr.NS_ERROR_SIGNED_JAR_NOT_SIGNED &&
          !this.useRealCertChecks && callback.wrappedJSObject) {
        // Bypassing XPConnect allows us to create a fake x509 certificate from JS
        callback = callback.wrappedJSObject;

        try {
          let manifestURI = this.getManifestURI(file);

          let id = await this.getIDFromManifest(manifestURI);

          let fakeCert = {commonName: id};
          if (this.usePrivilegedSignatures) {
            let privileged = typeof this.usePrivilegedSignatures == "function" ?
                             this.usePrivilegedSignatures(id) : this.usePrivilegedSignatures;
            if (privileged) {
              fakeCert.organizationalUnit = "Mozilla Extensions";
            }
          }

          return [callback, Cr.NS_OK, fakeCert];
        } catch (e) {
          // If there is any error then just pass along the original results
        } finally {
          // Make sure to close the open zip file or it will be locked.
          if (file.isFile())
            Services.obs.notifyObservers(file, "flush-cache-entry", "cert-override");
        }
      }

      return [callback, result, cert];
    };

    let FakeCertDB = {
      init() {
        for (let property of Object.keys(this._genuine.QueryInterface(Ci.nsIX509CertDB))) {
          if (property in this)
            continue;

          if (typeof this._genuine[property] == "function")
            this[property] = this._genuine[property].bind(this._genuine);
        }
      },

      openSignedAppFileAsync(root, file, callback) {
        // First try calling the real cert DB
        this._genuine.openSignedAppFileAsync(root, file, (result, zipReader, cert) => {
          verifyCert(file.clone(), result, cert, callback)
            .then(([callback, result, cert]) => {
              callback.openSignedAppFileFinished(result, zipReader, cert);
            });
        });
      },

      verifySignedDirectoryAsync(root, dir, callback) {
        // First try calling the real cert DB
        this._genuine.verifySignedDirectoryAsync(root, dir, (result, cert) => {
          verifyCert(dir.clone(), result, cert, callback)
            .then(([callback, result, cert]) => {
              callback.verifySignedDirectoryFinished(result, cert);
            });
        });
      },

      QueryInterface: ChromeUtils.generateQI([Ci.nsIX509CertDB]),
    };

    // Unregister the real database. This only works because the add-ons manager
    // hasn't started up and grabbed the certificate database yet.
    MockRegistrar.register(CERTDB_CONTRACTID, FakeCertDB);

    // Initialize the mock service.
    Cc[CERTDB_CONTRACTID].getService();
    FakeCertDB.init();
  },

  /**
   * Overrides the blocklist service, and returns the given blocklist
   * states for the given add-ons.
   *
   * @param {object|Map} addons
   *        A mapping of add-on IDs to their blocklist states.
   * @returns {MockBlocklist}
   *        A mock blocklist service, which should be unregistered when
   *        the test is complete.
   */
  overrideBlocklist(addons) {
    let mock = new MockBlocklist(addons);
    mock.register();
    return mock;
  },

  /**
   * Starts up the add-on manager as if it was started by the application.
   *
   * @param {boolean} [appChanged = true]
   *        An optional boolean parameter to simulate the case where the
   *        application has changed version since the last run. If not passed it
   *        defaults to true
   */
  async promiseStartupManager(appChanged = true) {
    if (this.addonIntegrationService)
      throw new Error("Attempting to startup manager that was already started.");

    if (appChanged && this.addonStartup.exists())
      this.addonStartup.remove(true);

    this.addonIntegrationService = Cc["@mozilla.org/addons/integration;1"]
          .getService(Ci.nsIObserver);

    this.addonIntegrationService.observe(null, "addons-startup", null);

    this.emit("addon-manager-started");

    // Load the add-ons list as it was after extension registration
    await this.loadAddonsList(true);
  },

  promiseShutdownManager() {
    if (!this.addonIntegrationService)
      return Promise.resolve(false);

    if (this.overrideEntry) {
      this.overrideEntry.destruct();
    }

    Services.obs.notifyObservers(null, "quit-application-granted");
    return MockAsyncShutdown.hook()
      .then(async () => {
        this.emit("addon-manager-shutdown");

        this.addonIntegrationService = null;

        // Load the add-ons list as it was after application shutdown
        await this.loadAddonsList();

        // Flush the jar cache entries for each bootstrapped XPI so that
        // we don't run into file locking issues on Windows.
        for (let file of this.addonsList.xpis) {
          Services.obs.notifyObservers(file, "flush-cache-entry");
        }

        // Clear any crash report annotations
        this.appInfo.annotations = {};

        // Force the XPIProvider provider to reload to better
        // simulate real-world usage.
        let XPIscope = ChromeUtils.import("resource://gre/modules/addons/XPIProvider.jsm", {});
        // This would be cleaner if I could get it as the rejection reason from
        // the AddonManagerInternal.shutdown() promise
        let shutdownError = XPIscope.XPIDatabase._saveError;

        AddonManagerPrivate.unregisterProvider(XPIscope.XPIProvider);
        Cu.unload("resource://gre/modules/addons/XPIProvider.jsm");
        Cu.unload("resource://gre/modules/addons/XPIDatabase.jsm");
        Cu.unload("resource://gre/modules/addons/XPIInstall.jsm");

        if (shutdownError)
          throw shutdownError;

        return true;
      });
  },

  promiseRestartManager(newVersion) {
    return this.promiseShutdownManager()
      .then(() => {
        if (newVersion)
          this.appInfo.version = newVersion;

        return this.promiseStartupManager(!!newVersion);
      });
  },

  async loadAddonsList(flush = false) {
    if (flush) {
      let XPIScope = ChromeUtils.import("resource://gre/modules/addons/XPIProvider.jsm", {});
      XPIScope.XPIStates.save();
      await XPIScope.XPIStates._jsonFile._save();
    }

    this.addonsList = new AddonsList(this.addonStartup);
  },

  /**
   * Creates an update.rdf structure as a string using for the update data passed.
   *
   * @param {Object} data
   *        The update data as a JS object. Each property name is an add-on ID,
   *        the property value is an array of each version of the add-on. Each
   *        array value is a JS object containing the data for the version, at
   *        minimum a "version" and "targetApplications" property should be
   *        included to create a functional update manifest.
   * @return {string} The update.rdf structure as a string.
   */
  createUpdateRDF(data) {
    var rdf = '<?xml version="1.0"?>\n';
    rdf += '<RDF xmlns="http://www.w3.org/1999/02/22-rdf-syntax-ns#"\n' +
           '     xmlns:em="http://www.mozilla.org/2004/em-rdf#">\n';

    for (let addon in data) {
      rdf += escaped`  <Description about="urn:mozilla:extension:${addon}"><em:updates><Seq>\n`;

      for (let versionData of data[addon]) {
        rdf += "    <li><Description>\n";
        rdf += this._writeProps(versionData, ["version"],
                                `      `);
        for (let app of versionData.targetApplications || []) {
          rdf += "      <em:targetApplication><Description>\n";
          rdf += this._writeProps(app, ["id", "minVersion", "maxVersion", "updateLink", "updateHash"],
                                  `        `);
          rdf += "      </Description></em:targetApplication>\n";
        }
        rdf += "    </Description></li>\n";
      }
      rdf += "  </Seq></em:updates></Description>\n";
    }
    rdf += "</RDF>\n";

    return rdf;
  },

  _writeProps(obj, props, indent = "  ") {
    let items = [];
    for (let prop of props) {
      if (prop in obj)
        items.push(escaped`${indent}<em:${prop}>${obj[prop]}</em:${prop}>\n`);
    }
    return items.join("");
  },

  _writeArrayProps(obj, props, indent = "  ") {
    let items = [];
    for (let prop of props) {
      for (let val of obj[prop] || [])
        items.push(escaped`${indent}<em:${prop}>${val}</em:${prop}>\n`);
    }
    return items.join("");
  },

  _writeLocaleStrings(data) {
    let items = [];

    items.push(this._writeProps(data, ["name", "description", "creator", "homepageURL"]));
    items.push(this._writeArrayProps(data, ["developer", "translator", "contributor"]));

    return items.join("");
  },

  createInstallRDF(data) {
    var rdf = '<?xml version="1.0"?>\n';
    rdf += '<RDF xmlns="http://www.w3.org/1999/02/22-rdf-syntax-ns#"\n' +
           '     xmlns:em="http://www.mozilla.org/2004/em-rdf#">\n';

    rdf += '<Description about="urn:mozilla:install-manifest">\n';

    let props = ["id", "version", "type", "internalName", "updateURL",
                 "optionsURL", "optionsType", "aboutURL", "iconURL", "icon64URL",
                 "skinnable", "bootstrap", "unpack", "strictCompatibility",
                 "hasEmbeddedWebExtension"];
    rdf += this._writeProps(data, props);

    rdf += this._writeLocaleStrings(data);

    for (let platform of data.targetPlatforms || [])
      rdf += escaped`<em:targetPlatform>${platform}</em:targetPlatform>\n`;

    for (let app of data.targetApplications || []) {
      rdf += "<em:targetApplication><Description>\n";
      rdf += this._writeProps(app, ["id", "minVersion", "maxVersion"]);
      rdf += "</Description></em:targetApplication>\n";
    }

    for (let localized of data.localized || []) {
      rdf += "<em:localized><Description>\n";
      rdf += this._writeArrayProps(localized, ["locale"]);
      rdf += this._writeLocaleStrings(localized);
      rdf += "</Description></em:localized>\n";
    }

    for (let dep of data.dependencies || [])
      rdf += escaped`<em:dependency><Description em:id="${dep}"/></em:dependency>\n`;

    rdf += "</Description>\n</RDF>\n";
    return rdf;
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
    for (let lastPath; path != lastPath; lastPath = path, path = OS.Path.dirname(path))
      paths.push(path);

    return Promise.all(paths.reverse().map(path =>
      OS.File.makeDir(path, {ignoreExisting: true}).catch(() => {})));
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
    if (typeof zipFile == "string")
      zipFile = nsFile(zipFile);

    var zipW = ZipWriter(zipFile, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE | flags);

    for (let [path, data] of Object.entries(files)) {
      if (typeof data === "object" && ChromeUtils.getClassName(data) === "Object")
        data = JSON.stringify(data);
      if (!(data instanceof ArrayBuffer))
        data = new TextEncoder("utf-8").encode(data).buffer;

      let stream = ArrayBufferInputStream(data, 0, data.byteLength);

      // Note these files are being created in the XPI archive with date "0" which is 1970-01-01.
      zipW.addEntryStream(path, 0, Ci.nsIZipWriter.COMPRESSION_NONE,
                          stream, false);
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
        await OS.Path.makeDir(dirPath, {ignoreExisting: true});
      }

      if (typeof data == "string")
        data = new TextEncoder("utf-8").encode(data);

      await OS.File.writeAtomic(OS.Path.join(dirPath, leafName), data);
    }

    return nsFile(dir);
  },

  promiseWriteFilesToExtension(dir, id, files, unpacked = this.testUnpacked) {
    if (typeof files["install.rdf"] === "object")
      files["install.rdf"] = this.createInstallRDF(files["install.rdf"]);

    if (unpacked) {
      let path = OS.Path.join(dir, id);

      return this.promiseWriteFilesToDir(path, files);
    }

    let xpi = OS.Path.join(dir, `${id}.xpi`);

    return this.promiseWriteFilesToZip(xpi, files);
  },

  tempXPIs: [],
  /**
   * Creates an XPI file for some manifest data in the temporary directory and
   * returns the nsIFile for it. The file will be deleted when the test completes.
   *
   * @param {object} files
   *          The object holding data about the add-on
   * @return {nsIFile} A file pointing to the created XPI file
   */
  createTempXPIFile(files) {
    var file = this.tempDir.clone();
    let uuid = uuidGen.generateUUID().number.slice(1, -1);
    file.append(`${uuid}.xpi`);

    this.tempXPIs.push(file);

    if (typeof files["install.rdf"] === "object")
      files["install.rdf"] = this.createInstallRDF(files["install.rdf"]);

    this.writeFilesToZip(file.path, files);
    return file;
  },

  /**
   * Creates an XPI file for some WebExtension data in the temporary directory and
   * returns the nsIFile for it. The file will be deleted when the test completes.
   *
   * @param {Object} data
   *        The object holding data about the add-on, as expected by
   *        |Extension.generateXPI|.
   * @return {nsIFile} A file pointing to the created XPI file
   */
  createTempWebExtensionFile(data) {
    let file = Extension.generateXPI(data);
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
  async manuallyInstall(xpiFile, installLocation = this.profileExtensions, id = null, unpacked = this.testUnpacked) {
    if (id == null) {
      id = await this.getIDFromExtension(xpiFile);
    }

    if (unpacked) {
      let dir = installLocation.clone();
      dir.append(id);
      dir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

      let zip = ZipReader(xpiFile);
      let entries = zip.findEntries(null);
      while (entries.hasMore()) {
        let entry = entries.getNext();
        let target = dir.clone();
        for (let part of entry.split("/"))
          target.append(part);
        if (!target.parent.exists())
          target.parent.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
        try {
          zip.extract(entry, target);
        } catch (e) {
          if (e.result != Cr.NS_ERROR_FILE_DIR_NOT_EMPTY &&
              !(target.exists() && target.isDirectory())) {
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
    if (file.isFile())
      Services.obs.notifyObservers(file, "flush-cache-entry");

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
    if (unpacked)
      dir.append(id);
    else
      dir.append(`${id}.xpi`);
    return dir;
  },

  /**
   * Sets the last modified time of the extension, usually to trigger an update
   * of its metadata. If the extension is unpacked, this function assumes that
   * the extension contains only the install.rdf file.
   *
   * @param {nsIFile} ext A file pointing to either the packed extension or its unpacked directory.
   * @param {number} time The time to which we set the lastModifiedTime of the extension
   *
   * @deprecated Please use promiseSetExtensionModifiedTime instead
   */
  setExtensionModifiedTime(ext, time) {
    ext.lastModifiedTime = time;
    if (ext.isDirectory()) {
      let entries = ext.directoryEntries
                       .QueryInterface(Ci.nsIDirectoryEnumerator);
      while (entries.hasMoreElements())
        this.setExtensionModifiedTime(entries.nextFile, time);
      entries.close();
    }
  },

  async promiseSetExtensionModifiedTime(path, time) {
    await OS.File.setDates(path, time, time);

    let iterator = new OS.File.DirectoryIterator(path);
    try {
      await iterator.forEach(entry => {
        return this.promiseSetExtensionModifiedTime(entry.path, time);
      });
    } catch (ex) {
      if (ex instanceof OS.File.Error)
        return;
      throw ex;
    } finally {
      iterator.close().catch(() => {});
    }
  },

  registerDirectory(key, dir) {
    var dirProvider = {
      getFile(prop, persistent) {
        persistent.value = false;
        if (prop == key)
          return dir.clone();
        return null;
      },

      QueryInterface: ChromeUtils.generateQI([Ci.nsIDirectoryServiceProvider]),
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
   * @returns {Promise<Array>}
   *        Resolves to an array containing the event handler's
   *        arguments the first time it is called.
   */
  promiseAddonEvent(event) {
    return new Promise(resolve => {
      let listener = {
        [event](...args) {
          AddonManager.removeAddonListener(listener);
          resolve(args);
        },
      };

      AddonManager.addAddonListener(listener);
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
      listener = {
        onDownloadFailed: resolve,
        onDownloadCancelled: resolve,
        onInstallFailed: resolve,
        onInstallCancelled: resolve,
        onInstallEnded: resolve,
        onInstallPostponed: resolve,
      };

      install.addListener(listener);
      install.install();
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
   * @returns {Promise}
   *        Resolves when the install has completed.
   */
  promiseInstallFile(file, ignoreIncompatible = false) {
    return AddonManager.getInstallForFile(file).then(install => {
      if (!install)
        throw new Error(`No AddonInstall created for ${file.path}`);

      if (install.state != AddonManager.STATE_DOWNLOADED)
        throw new Error(`Expected file to be downloaded for install of ${file.path}`);

      if (ignoreIncompatible && install.addon.appDisabled)
        return null;

      return this.promiseCompleteInstall(install);
    });
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
    return Promise.all(Array.from(
      files,
      file => this.promiseInstallFile(file, ignoreIncompatible)));
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
  promiseFindAddonUpdates(addon, reason = AddonTestUtils.updateReason, ...args) {
    let equal = this.testScope.equal;
    return new Promise((resolve, reject) => {
      let result = {};
      addon.findUpdates({
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
        }
      }, reason, ...args);
    });
  },

  /**
   * Monitors console output for the duration of a task, and returns a promise
   * which resolves to a tuple containing a list of all console messages
   * generated during the task's execution, and the result of the task itself.
   *
   * @param {function} task
   *        The task to run while monitoring console output. May be
   *        either a generator function, per Task.jsm, or an ordinary
   *        function which returns promose.
   * @return {Promise<[Array<nsIConsoleMessage>, *]>}
   *        Resolves to an object containing a `messages` property, with
   *        the array of console messages emitted during the execution
   *        of the task, and a `result` property, containing the task's
   *        return value.
   */
  async promiseConsoleOutput(task) {
    const DONE = "=== xpcshell test console listener done ===";

    let listener, messages = [];
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

      return {messages, result};
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
  checkMessages(messages, {expected = [], forbidden = [], forbidUnexpected = false}) {
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
      this.testScope.ok(false, `Did not get expected console message: ${uneval(pat)}`);
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
      Management.on("ready", function listener(event, extension) {
        if (!id || extension.id == id) {
          Management.off("ready", listener);
          resolve(extension);
        }
      });
    });
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
    Services.prefs.setBoolPref(PREF_DISABLE_SECURITY, true);
    aomStartup.initializeURLPreloader();

    let file = this.tempDir.clone();
    file.append("override.txt");
    this.tempXPIs.push(file);

    let manifest = Services.io.newFileURI(file);
    await OS.File.writeAtomic(file.path,
      new TextEncoder().encode(JSON.stringify(data)));
    this.overrideEntry = aomStartup.registerChrome(manifest, [
      ["override", "chrome://browser/content/built_in_addons.json",
       Services.io.newFileURI(file).spec],
    ]);
    Services.prefs.setBoolPref(PREF_DISABLE_SECURITY, prevPrefVal);
  }
};

for (let [key, val] of Object.entries(AddonTestUtils)) {
  if (typeof val == "function")
    AddonTestUtils[key] = val.bind(AddonTestUtils);
}

EventEmitter.decorate(AddonTestUtils);
