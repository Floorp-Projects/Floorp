"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonTestUtils: "resource://testing-common/AddonTestUtils.jsm",
  ExtensionTestUtils: "resource://testing-common/ExtensionXPCShellUtils.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  sinon: "resource://testing-common/Sinon.jsm",
});

var EXPORTED_SYMBOLS = ["SearchTestUtils"];

var gTestGlobals;

var SearchTestUtils = Object.freeze({
  init(Assert, registerCleanupFunction) {
    gTestGlobals = {
      Assert,
      registerCleanupFunction,
    };
  },

  /**
   * Adds an OpenSearch based engine to the search service. It will remove
   * the engine at the end of the test.
   *
   * @param {string}   url                     The URL of the engine to add.
   * @param {Function} registerCleanupFunction Pass the registerCleanupFunction
   *                                           from the test's scope.
   * @returns {Promise} Returns a promise that is resolved with the new engine
   *                    or rejected if it fails.
   */
  async promiseNewSearchEngine(url) {
    let engine = await Services.search.addOpenSearchEngine(url, "");
    gTestGlobals.registerCleanupFunction(async () =>
      Services.search.removeEngine(engine)
    );
    return engine;
  },

  /**
   * Returns a promise that is resolved when an observer notification from the
   * search service fires with the specified data.
   *
   * @param {*} expectedData
   *        The value the observer notification sends that causes us to resolve
   *        the promise.
   * @param {string} topic
   *        The notification topic to observe. Defaults to 'browser-search-service'.
   * @returns {Promise}
   *        Returns a promise that is resolved with the subject of the
   *        topic once the topic with the data has been observed.
   */
  promiseSearchNotification(expectedData, topic = "browser-search-service") {
    return new Promise(resolve => {
      Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
        if (aData != expectedData) {
          return;
        }

        Services.obs.removeObserver(observer, topic);
        resolve(aSubject);
      }, topic);
    });
  },

  parseJsonFromStream(aInputStream) {
    let bytes = NetUtil.readInputStream(aInputStream, aInputStream.available());
    return JSON.parse(new TextDecoder().decode(bytes));
  },

  /**
   * Load engines from test data located in particular folders.
   *
   * @param {string} [folder]
   *   The folder name to use.
   * @param {string} [subFolder]
   *   The subfolder to use, if any.
   * @param {array} [config]
   *   An array which contains the configuration to set.
   */
  async useTestEngines(folder = "data", subFolder = null, config = null) {
    let url = `resource://test/${folder}/`;
    if (subFolder) {
      url += `${subFolder}/`;
    }
    let resProt = Services.io
      .getProtocolHandler("resource")
      .QueryInterface(Ci.nsIResProtocolHandler);
    resProt.setSubstitution("search-extensions", Services.io.newURI(url));
    if (
      Services.prefs.getBoolPref(
        SearchUtils.BROWSER_SEARCH_PREF + "modernConfig"
      )
    ) {
      const settings = await RemoteSettings(SearchUtils.SETTINGS_KEY);
      if (config) {
        sinon.stub(settings, "get").returns(config);
      } else {
        let chan = NetUtil.newChannel({
          uri: "resource://search-extensions/engines.json",
          loadUsingSystemPrincipal: true,
        });
        let json = this.parseJsonFromStream(chan.open());
        sinon.stub(settings, "get").returns(json.data);
      }
    }
  },

  /**
   * Convert a list of engine configurations into engine objects.
   *
   * @param {Array} engineConfigurations
   **/
  async searchConfigToEngines(engineConfigurations) {
    let engines = [];
    for (let config of engineConfigurations) {
      let engine = await Services.search.wrappedJSObject.makeEngineFromConfig(
        config
      );
      engines.push(engine);
    }
    return engines;
  },

  /**
   * Provides various setup for xpcshell-tests installing WebExtensions. Should
   * be called from the global scope of the test.
   *
   * @param {object} scope
   *  The global scope of the test being run.
   * @param {*} usePrivilegedSignatures
   *  How to sign created addons.
   */
  initXPCShellAddonManager(scope, usePrivilegedSignatures = false) {
    let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION;
    Services.prefs.setIntPref("extensions.enabledScopes", scopes);
    Services.prefs.setBoolPref(
      "extensions.webextensions.background-delayed-startup",
      false
    );
    ExtensionTestUtils.init(scope);
    AddonTestUtils.usePrivilegedSignatures = usePrivilegedSignatures;
    AddonTestUtils.overrideCertDB();
  },

  /**
   * Add a search engine as a WebExtension. For xpcshell-tests only.
   *
   * Note: You should call `initXPCShellAddonManager` before calling this.
   *
   * @param {object} [options]
   */
  async installSearchExtension(options = {}) {
    options.id = (options.id ?? "example") + "@tests.mozilla.org";
    let extensionInfo = {
      useAddonManager: "permanent",
      manifest: this.createEngineManifest(options),
    };

    let extension = ExtensionTestUtils.loadExtension(extensionInfo);
    await extension.startup();
    await AddonTestUtils.waitForSearchProviderStartup(extension);
    return extension;
  },

  /**
   * Install a search engine as a system extension to simulate
   * Normandy updates. For xpcshell-tests only.
   *
   * Note: You should call `initXPCShellAddonManager` before calling this.
   *
   * @param {object} [options]
   */
  async installSystemSearchExtension(options = {}) {
    options.id = (options.id ?? "example") + "@search.mozilla.org";
    let xpi = await AddonTestUtils.createTempWebExtensionFile({
      manifest: this.createEngineManifest(options),
      background() {
        // eslint-disable-next-line no-undef
        browser.test.sendMessage("started");
      },
    });
    let wrapper = ExtensionTestUtils.expectExtension(options.id);

    const install = await AddonManager.getInstallForURL(`file://${xpi.path}`, {
      useSystemLocation: true,
    });

    install.install();

    await wrapper.awaitStartup();
    await wrapper.awaitMessage("started");

    return wrapper;
  },

  /**
   * Create a search engine extension manifest.
   *
   * @param {object} [options]
   * @param {string} [options.id]
   *   The id to use for the WebExtension.
   * @param {string} [options.name]
   *   The display name to use for the WebExtension.
   * @param {string} [options.version]
   *   The version to use for the WebExtension.
   * @param {string} [options.keyword]
   *   The keyword to use for the WebExtension.
   * @returns {object}
   *   The generated manifest.
   */
  createEngineManifest(options = {}) {
    options.id = options.id ?? "example@tests.mozilla.org";
    options.name = options.name ?? "Example";
    options.version = options.version ?? "1.0";
    let manifest = {
      version: options.version,
      applications: {
        gecko: {
          id: options.id,
        },
      },
      chrome_settings_overrides: {
        search_provider: {
          name: options.name,
          search_url: "https://example.com/",
          search_url_get_params: "?q={searchTerms}",
        },
      },
    };
    if (options.keyword) {
      manifest.chrome_settings_overrides.search_provider.keyword =
        options.keyword;
    }
    return manifest;
  },

  /**
   * A mock idleService that allows us to simulate RemoteSettings
   * configuration updates.
   */
  idleService: {
    _observers: new Set(),

    _reset() {
      this._observers.clear();
    },

    _fireObservers(state) {
      for (let observer of this._observers.values()) {
        observer.observe(observer, state, null);
      }
    },

    QueryInterface: ChromeUtils.generateQI(["nsIUserIdleService"]),
    idleTime: 19999,

    addIdleObserver(observer, time) {
      this._observers.add(observer);
    },

    removeIdleObserver(observer, time) {
      this._observers.delete(observer);
    },
  },

  /**
   * Register the mock idleSerice.
   *
   * @param {Fun} registerCleanupFunction
   */
  useMockIdleService(registerCleanupFunction) {
    let fakeIdleService = MockRegistrar.register(
      "@mozilla.org/widget/useridleservice;1",
      SearchTestUtils.idleService
    );
    registerCleanupFunction(() => {
      MockRegistrar.unregister(fakeIdleService);
    });
  },

  /**
   * Simulates an update to the RemoteSettings configuration.
   *
   * @param {object} config
   *  The new configuration.
   */
  async updateRemoteSettingsConfig(config) {
    const reloadObserved = SearchTestUtils.promiseSearchNotification(
      "engines-reloaded"
    );
    await RemoteSettings(SearchUtils.SETTINGS_KEY).emit("sync", {
      data: { current: config },
    });

    this.idleService._fireObservers("idle");
    await reloadObserved;
  },
});
